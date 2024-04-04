#include "shaders.hpp"

#include "helpers.h"
#include <spirv_reflect.h>

ShaderReflectionData vkutil::generateReflectionData(std::span<uint8_t const> spirv_bytecode)
{
	SpvReflectShaderModule module;
	SpvReflectResult const result = spvReflectCreateShaderModule(
		spirv_bytecode.size_bytes(),
		spirv_bytecode.data(),
		&module
	);

	Log(fmt::format("Reflection: create module {}", static_cast<int32_t>(result)));

	uint32_t pushConstantCounts;
	spvReflectEnumeratePushConstants(
		&module,
		&pushConstantCounts,
		nullptr
	);
	std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCounts);
	spvReflectEnumeratePushConstants(
		&module,
		&pushConstantCounts,
		pushConstants.data()
	);

	std::map<std::string, ShaderReflectionData::PushConstant> pushConstantsByEntryPoint{};
	for (auto const pEntryPoint : std::span<SpvReflectEntryPoint* const>(&module.entry_points, module.entry_point_count))
	{
		SpvReflectEntryPoint const& entryPoint{ *pEntryPoint };
		Log(fmt::format("Reflection: entry point name \"{}\"", entryPoint.name));

		SpvReflectResult result;
		SpvReflectBlockVariable const* pPushConstant{ spvReflectGetEntryPointPushConstantBlock(&module, entryPoint.name, &result) };

		if (result == SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND)
		{
			// No push constant on the entry point
			continue;
		}

		// The only way the result is not success is if 1) the module is null or 2) the entry point does not exist, which we both know to be false.
		assert(result == SPV_REFLECT_RESULT_SUCCESS);

		SpvReflectBlockVariable const& pushConstant{ *pPushConstant };

		Log(fmt::format("Reflection: pushConstant name \"{}\" with \"{}\" elements, byte size \"{}\"",
			pushConstant.name,
			pushConstant.member_count,
			pushConstant.size
		));

		std::vector<ShaderReflectionData::StructureMember> processedMembers{};

		assert(pushConstant.type_description->op == SpvOp::SpvOpTypeStruct);

		// Process members
		std::span<SpvReflectBlockVariable> const members{ pushConstant.members, pushConstant.member_count };
		for (SpvReflectBlockVariable const& member : members)
		{
			// Parse spirv-reflect type descriptions into type-safe form

			uint32_t const offsetBytes{ member.offset };

			SpvReflectTypeDescription const& typeDescription{ *member.type_description };
			SpvReflectNumericTraits const& numericTraits{ typeDescription.traits.numeric };

			// SPIR-V type names are empty for built in types ?
			std::string const typeName{ typeDescription.type_name == nullptr ? "" : typeDescription.type_name };

			uint32_t const bitWidth = member.numeric.scalar.width;

			// For early exit, if the type ends up being unsupported

			ShaderReflectionData::StructureMember const unsupportedMember{
					.offsetBytes{ offsetBytes },
					.name{ member.name },
					.type{ ShaderReflectionData::SizedType{
						.typeData{ ShaderReflectionData::UnsupportedType{} },

						.name{ typeName },
						.sizeBytes{ member.size },
						.paddedSizeBytes{ member.padded_size },
					} },
			};

			// Validate type flags early
			uint32_t const typeFlagValidMask{ 0x0000FFFF };
			if ((typeDescription.type_flags & ~typeFlagValidMask) != 0)
			{
				Error(fmt::format("Unsupported push constant member flag types \"{}\" for \"{}\""
					, std::to_string(typeDescription.type_flags)
					, member.name)
				);
				processedMembers.push_back(unsupportedMember);
				continue;
			}

			uint32_t const typeFlagComponentTypeMask{ 0x000000FF };
			ShaderReflectionData::NumericType::ComponentType componentType;
			switch (typeDescription.type_flags & typeFlagComponentTypeMask)
			{
			case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_INT:
				componentType = ShaderReflectionData::Integer{
					.signedness{ static_cast<bool>(numericTraits.scalar.signedness) }
				};
				break;
			case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_FLOAT:
				componentType = ShaderReflectionData::Float{};
				break;
			default:
				Error(fmt::format("Unsupported push constant member type \"{}\" for \"{}\""
					, std::to_string(typeDescription.type_flags & typeFlagComponentTypeMask)
					, member.name)
				);
				processedMembers.push_back(unsupportedMember);
				continue;
			}

			uint32_t const typeFlagFormatMask{ 0x0000FF00 };
			ShaderReflectionData::NumericType::Format format;
			switch (typeDescription.type_flags & typeFlagFormatMask)
			{
			case 0:
				format = ShaderReflectionData::Scalar{};
				break;
			case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_VECTOR:
				format = ShaderReflectionData::Vector{
					.componentCount{ numericTraits.vector.component_count }
				};
				break;
			case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_MATRIX | SPV_REFLECT_TYPE_FLAG_VECTOR:
				format = ShaderReflectionData::Matrix{
					.columnCount{ numericTraits.matrix.column_count },
					.rowCount{ numericTraits.matrix.row_count }
				};
				break;
			default:
				Error(fmt::format("Unsupported push constant member format \"{}\" for \"{}\""
					, std::to_string(typeDescription.type_flags & typeFlagFormatMask)
					, member.name)
				);
				processedMembers.push_back(unsupportedMember);
				continue;
			}

			processedMembers.push_back(ShaderReflectionData::StructureMember{
				.offsetBytes{ offsetBytes },
				.name{ member.name },
				.type{
					ShaderReflectionData::SizedType{
						.typeData{ ShaderReflectionData::NumericType{
							.componentBitWidth{ numericTraits.scalar.width },
							.componentType{ componentType },
							.format{ format },
						}},
						.name{ typeName },
						.sizeBytes{ member.size },
						.paddedSizeBytes{ member.padded_size },
					}
				}
			});
		}

		pushConstantsByEntryPoint[std::string{ entryPoint.name }] = ShaderReflectionData::PushConstant{
			.type{
				.name{	pushConstant.type_description->type_name },
				.sizeBytes{ pushConstant.size },
				.paddedSizeBytes{ pushConstant.padded_size },
				.members{ processedMembers },
			},
			.name{ pushConstant.name },
			.layoutOffsetBytes{ pushConstant.offset }
		};
	}

	std::string defaultEntryPoint{ module.entry_point_name };

	spvReflectDestroyShaderModule(&module);

	return ShaderReflectionData{
		.pushConstantsByEntryPoint{ pushConstantsByEntryPoint },
		.defaultEntryPoint{ defaultEntryPoint },
	};
}

ShaderWrapper ShaderWrapper::FromBytecode(VkDevice device, std::string name, std::span<uint8_t const> spirv_bytecode)
{
	// vkCreateShaderModule takes in an array of 32 bit words
	size_t const codeSize{ spirv_bytecode.size() / 4 };

	assert(codeSize * 4 == spirv_bytecode.size());

	VkShaderModuleCreateInfo const createInfo{
		.sType{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO },
		.pNext{ nullptr },

		.codeSize{ codeSize * 4 },
		.pCode{ reinterpret_cast<uint32_t const*>(spirv_bytecode.data()) },
	};

	VkShaderModule shaderModule{ VK_NULL_HANDLE };
	VkResult const result{ vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) };
	LogVkResult(result, "Creating Shader Module");
	if (result != VK_SUCCESS)
	{
		return ShaderWrapper::Invalid();
	}

	ShaderReflectionData reflectionData{ vkutil::generateReflectionData(spirv_bytecode) };

	ShaderWrapper shaderWrapper(
		name,
		reflectionData,
		shaderModule
	);

	shaderWrapper.resetRuntimeData();

	return shaderWrapper;
}

VkPushConstantRange ShaderWrapper::pushConstantRange(VkShaderStageFlags stageMask) const
{
	assert(m_reflectionData.defaultEntryPointHasPushConstant());
	ShaderReflectionData::PushConstant const& pushConstant{ m_reflectionData.defaultPushConstant() };
	return {
		.stageFlags{ stageMask },
		.offset{ pushConstant.layoutOffsetBytes },
		.size{ pushConstant.type.sizeBytes },
	};
}

std::span<uint8_t> ShaderWrapper::mapRuntimePushConstant(std::string pushConstantName)
{
	return std::span<uint8_t>{ m_runtimePushConstantsByEntryPoint[pushConstantName] };
}

std::span<uint8_t const> ShaderWrapper::readRuntimePushConstant(std::string pushConstantName) const
{
	return std::span<uint8_t const>{ m_runtimePushConstantsByEntryPoint.at(pushConstantName) };
}

void ShaderWrapper::cleanup(VkDevice device) const
{
	vkDestroyShaderModule(device, m_shaderModule, nullptr);
}

void ShaderWrapper::resetRuntimeData()
{
	std::map<std::string, std::vector<uint8_t>> runtimePushConstants{};
	for (auto const& [entryPoint, pushConstant] : m_reflectionData.pushConstantsByEntryPoint)
	{
		runtimePushConstants[entryPoint] = std::vector<uint8_t>(pushConstant.type.sizeBytes - pushConstant.layoutOffsetBytes);
	}

	m_runtimePushConstantsByEntryPoint = runtimePushConstants;
}

bool ShaderReflectionData::Structure::logicallyCompatible(Structure const& other) const
{
	uint32_t memberIndex{ 0 };
	uint32_t otherMemberIndex{ 0 };

	size_t iterations{ 0 };
	while (iterations < 100)
	{
		if (memberIndex >= members.size() || otherMemberIndex >= other.members.size())
		{
			// Reached the end without finding incompatible members, so the rest does not matter.
			return true;
		}

		struct ByteRange {
			uint32_t startByte{ 0 };
			uint32_t endUnpaddedByte{ 0 };
			uint32_t endPaddedByte{ 0 };
		};
		auto const getByteRange{
			[&](StructureMember const& member) {
				return ByteRange{
					.startByte{ member.offsetBytes },
					.endUnpaddedByte{ member.offsetBytes + member.type.sizeBytes },
					.endPaddedByte{ member.offsetBytes + member.type.paddedSizeBytes },
				};
			}
		};

		StructureMember const& member{ members[memberIndex] };
		StructureMember const& otherMember{ other.members[otherMemberIndex] };

		ByteRange const memberRange{ getByteRange(member) };
		ByteRange const otherMemberRange{ getByteRange(otherMember) };

		// If the members overlap, their types must be compatible
		
		// Assert monotonicity so interval calculations are valid
		assert(
			memberRange.endPaddedByte >= memberRange.endUnpaddedByte
			&& memberRange.endUnpaddedByte >= memberRange.startByte
		);
		assert(
			otherMemberRange.endPaddedByte >= otherMemberRange.endUnpaddedByte
			&& otherMemberRange.endUnpaddedByte >= otherMemberRange.startByte
		);

		if (memberRange.startByte < otherMemberRange.endPaddedByte
			&& memberRange.endPaddedByte > otherMemberRange.startByte)
		{
			// For now, require members to be identical.
			if (member.type.typeData != otherMember.type.typeData)
			{
				return false;
			}
		}
		
		if (memberRange.endUnpaddedByte <= otherMemberRange.endPaddedByte)
		{
			memberIndex += 1;
		}
		else
		{
			otherMemberIndex += 1;
		}
	}
	Error(fmt::format("Ran out of iterations while checking shader structure compatibility between {} and {}."
		, name
		, other.name
	));
	return false;
}

bool ShaderReflectionData::NumericType::operator==(NumericType const& other) const
{
	return other.componentBitWidth == componentBitWidth
		&& other.componentType == componentType
		&& other.format == format;
}

bool ShaderReflectionData::Integer::operator==(Integer const& other) const
{
	return other.signedness == signedness;
}

bool ShaderReflectionData::Vector::operator==(Vector const& other) const
{
	return other.componentCount == componentCount;
}

bool ShaderReflectionData::Matrix::operator==(Matrix const& other) const
{
	return other.columnCount == columnCount
		&& other.rowCount == rowCount;
}

