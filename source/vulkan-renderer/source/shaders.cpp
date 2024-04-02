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

	std::vector<ShaderReflectionData::PushConstant> processedPushConstants{};
	for (SpvReflectBlockVariable* const pPushConstant : pushConstants)
	{
		SpvReflectBlockVariable const& pushConstant{ *pPushConstant };

		Log(fmt::format("Reflection: pushConstant name \"{}\" with \"{}\" elements, byte size \"{}\"",
			pushConstant.name,
			pushConstant.member_count,
			pushConstant.size
		));

		std::vector<ShaderReflectionData::Structure::StructureMember> processedMembers{};

		assert(pushConstant.type_description->op == SpvOp::SpvOpTypeStruct);

		// Process members
		std::span<SpvReflectBlockVariable> const members{ pushConstant.members, pushConstant.member_count };
		for (SpvReflectBlockVariable const& member : members)
		{
			// Parse spirv-reflect type descriptions into type-safe form

			SpvReflectTypeDescription const& typeDescription{ *member.type_description };
			SpvReflectNumericTraits const& numericTraits{ typeDescription.traits.numeric };

			uint32_t const bitWidth = member.numeric.scalar.width;

			uint32_t const typeFlagValidMask{ 0x0000FFFF };
			if (~(typeDescription.type_flags & typeFlagValidMask) != 0)
			{
				Error(fmt::format("Unsupported push constant member flag types \"{}\" for \"{}\""
					, std::to_string(typeDescription.type_flags)
					, member.name)
				);
				processedMembers.push_back(ShaderReflectionData::UnsupportedMember{
					.name{ member.name }
					});
				continue;
			}

			uint32_t const typeFlagComponentTypeMask{ 0x000000FF };
			ShaderReflectionData::NumericMember::ComponentType componentType;
			switch (typeDescription.type_flags & typeFlagComponentTypeMask)
			{
			case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_BOOL:
				componentType = ShaderReflectionData::Boolean{};
				break;
			case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_INT:
				componentType = ShaderReflectionData::Integer{
					.signedness{ static_cast<bool>(numericTraits.scalar.signedness) }
				};
				break;
			case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_FLOAT:
				componentType = ShaderReflectionData::Float{};
				break;
			default:
				Error(fmt::format("Unsupported push constant member flag types \"{}\" for \"{}\""
					, std::to_string(typeDescription.type_flags)
					, member.name)
				);
				processedMembers.push_back(ShaderReflectionData::UnsupportedMember{
					.name{ member.name }
					});
				continue;
			}

			uint32_t const typeFlagFormatMask{ 0x0000FF00 };
			ShaderReflectionData::NumericMember::Format format;
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
			case SpvReflectTypeFlagBits::SPV_REFLECT_TYPE_FLAG_MATRIX:
				format = ShaderReflectionData::Matrix{
					.columnCount{ numericTraits.matrix.column_count },
					.rowCount{ numericTraits.matrix.row_count }
				};
				break;
			default:
				Error(fmt::format("Unsupported push constant member flag types \"{}\" for \"{}\""
					, std::to_string(typeDescription.type_flags)
					, member.name)
				);
				processedMembers.push_back(ShaderReflectionData::UnsupportedMember{
					.name{ member.name }
					});
				continue;
			}

			processedMembers.push_back(ShaderReflectionData::NumericMember{
				.name{ member.name },
				.componentBitWidth{ numericTraits.scalar.width },
				.componentType{ componentType },
				.format{ format }
				});
		}

		processedPushConstants.push_back(ShaderReflectionData::PushConstant{
			.name{ pushConstant.name },
			.sizeBytes{ pushConstant.size },
			.paddedSizeBytes{ pushConstant.padded_size },
			.members{ processedMembers },
			});
	}

	spvReflectDestroyShaderModule(&module);

	return ShaderReflectionData{
		.m_pushConstants{ processedPushConstants }
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

	return ShaderWrapper(
		name,
		vkutil::generateReflectionData(spirv_bytecode),
		shaderModule
	);
}

void ShaderWrapper::cleanup(VkDevice device) const
{
	vkDestroyShaderModule(device, m_shaderModule, nullptr);
}
