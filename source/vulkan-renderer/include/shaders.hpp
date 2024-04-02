#pragma once

#include "engine_types.h"
#include <variant>
#include <vector>

/**
	Contains reflected data from a ShaderModule, to aid with UI and proper piping of data.
	Work in progress, for now supports a very limited amount of reflection.
*/
struct ShaderReflectionData
{
	/**
		Type names correspond to the SPIR-V specification.
		The type names are not meant to exactly match the specification opcodes and layouts,
		just model it in a way that's useful.
		See https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html section "2.2.2. Types"
	*/

	// Corresponds to OpTypeBool
	struct Boolean {};

	// Corresponds to OpTypeInt
	struct Integer
	{
		bool signedness;
	};

	// Corresponds to OpTypeFloat
	struct Float {};

	struct Scalar {};
	struct Vector {
		uint32_t componentCount;
	};
	struct Matrix {
		uint32_t columnCount;
		uint32_t rowCount;
	};

	// Corresponds to a composite/scalar numeric type.
	struct NumericMember
	{
		using ComponentType = std::variant<Boolean, Integer, Float>;
		using Format = std::variant<Scalar, Vector, Matrix>;

		std::string name;
		uint32_t componentBitWidth;
		ComponentType componentType;
		Format format;
	};

	/**
		Represents a type whose reflection data could not be generated,
		usually because the specific type is not supported yet.
	*/
	struct UnsupportedMember
	{
		std::string name;
	};

	// Corresponds to OpTypeStruct
	struct Structure
	{
		using StructureMember = std::variant<UnsupportedMember, NumericMember>;

		std::string name;
		uint32_t sizeBytes;
		uint32_t paddedSizeBytes;
		std::vector<StructureMember> members;
	};

	/**
		As per the Vulkan specification, Push constants must be structs.
		https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#interfaces-resources-pushconst
	*/
	using PushConstant = Structure;

	std::vector<Structure> pushConstants{};
};

struct ShaderWrapper
{
	static ShaderWrapper Invalid() { return ShaderWrapper("", {}, VK_NULL_HANDLE); }
	static ShaderWrapper FromBytecode(VkDevice device, std::string name, std::span<uint8_t const> spirv_bytecode);

	VkShaderModule shaderModule() const { return m_shaderModule; }
	ShaderReflectionData const& reflectionData() const { return m_reflectionData; }
	std::string name() const { return m_name; }
	VkPushConstantRange pushConstantRange() const;

	template<typename T, int N>
	inline bool validatePushConstant(std::array<T, N> pushConstantData)
	{
		assert(m_reflectionData.pushConstants.size() == 1);
		
		ShaderReflectionData::PushConstant const& pushConstant{ m_reflectionData.pushConstants[0] };

		if (pushConstant.sizeBytes != sizeof(std::array<T, N>))
		{
			return false;
		}

		// TODO: check types of each member
		return true;
	}

	void cleanup(VkDevice device) const;

	bool isValid() const { return m_shaderModule != VK_NULL_HANDLE; }

private:
	ShaderWrapper() = delete;
	ShaderWrapper(std::string name, ShaderReflectionData reflectionData, VkShaderModule shaderModule)
		: m_name(name)
		, m_reflectionData(reflectionData)
		, m_shaderModule(shaderModule)
	{};

	std::string m_name{};
	ShaderReflectionData m_reflectionData{};
	VkShaderModule m_shaderModule{ VK_NULL_HANDLE };
};

namespace vkutil
{
	ShaderReflectionData generateReflectionData(std::span<uint8_t const> spirv_bytecode);
}