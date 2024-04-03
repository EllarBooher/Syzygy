#pragma once

#include "engine_types.h"
#include <variant>
#include <vector>
#include <map>

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

	struct NumericType
	{
		using ComponentType = std::variant<Integer, Float>;
		using Format = std::variant<Scalar, Vector, Matrix>;

		// May be an empty string for some types.
		std::string name;
		uint32_t componentBitWidth;
		ComponentType componentType;
		Format format;
	};

	/**
		Represents a type whose reflection data could not be generated,
		usually because the specific type is not supported yet.
	*/
	struct UnsupportedType
	{
		std::string name;
	};

	struct StructureMember
	{
		uint32_t offsetBytes;
		std::string name;
		std::variant<NumericType, UnsupportedType> typeData;
	};
	// Corresponds to OpTypeStruct
	struct Structure
	{
		// TODO: test if structures can be anonymous.
		std::string name;
		uint32_t sizeBytes;
		uint32_t paddedSizeBytes;
		std::vector<StructureMember> members;
	};

	/**
		As per the Vulkan specification, Push constants must be structs.
		There can also only be one per entry point.
		https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#interfaces-resources-pushconst
	*/
	using PushConstant = Structure;
	std::map<std::string, PushConstant> pushConstantsByEntryPoint{};

	std::string defaultEntryPoint{};

	bool defaultEntryPointHasPushConstant() const { return pushConstantsByEntryPoint.contains(defaultEntryPoint); }
	PushConstant const& defaultPushConstant() const { return pushConstantsByEntryPoint.at(defaultEntryPoint); }
};

struct ShaderWrapper
{
	static ShaderWrapper Invalid() { return ShaderWrapper("", {}, VK_NULL_HANDLE); }
	static ShaderWrapper FromBytecode(VkDevice device, std::string name, std::span<uint8_t const> spirv_bytecode);

	VkShaderModule shaderModule() const { return m_shaderModule; }
	ShaderReflectionData const& reflectionData() const { return m_reflectionData; }
	std::string name() const { return m_name; }
	VkPushConstantRange pushConstantRange() const;

	std::span<uint8_t> mapRuntimePushConstant(std::string entryPoint);
	std::span<uint8_t const> readRuntimePushConstant(std::string entryPoint) const;

	template<typename T, int N>
	inline bool validatePushConstant(std::array<T, N> pushConstantData, std::string entryPoint) const
	{
		std::span<uint8_t const> byteSpan{ reinterpret_cast<uint8_t const*>(pushConstantData.data()), sizeof(T) * N };
		return validatePushConstant(byteSpan, entryPoint);
	}

	inline bool validatePushConstant(std::span<uint8_t const> pushConstantData, std::string entryPoint) const
	{
		ShaderReflectionData::PushConstant const& pushConstant{ m_reflectionData.pushConstantsByEntryPoint.at(entryPoint) };

		if (pushConstant.sizeBytes != pushConstantData.size())
		{
			return false;
		}

		// TODO: check types of each member
		return true;
	}

	void cleanup(VkDevice device) const;

	bool isValid() const { return m_shaderModule != VK_NULL_HANDLE; }

	void resetRuntimeData();

	ShaderWrapper() {};
private:
	ShaderWrapper(std::string name, ShaderReflectionData reflectionData, VkShaderModule shaderModule)
		: m_name(name)
		, m_reflectionData(reflectionData)
		, m_shaderModule(shaderModule)
	{};

	std::string m_name{};
	ShaderReflectionData m_reflectionData{};
	VkShaderModule m_shaderModule{ VK_NULL_HANDLE };

	std::map<std::string, std::vector<uint8_t>> m_runtimePushConstantsByEntryPoint{};
};

struct ComputeShaderWrapper
{
	ShaderWrapper computeShader{};
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

	void cleanup(VkDevice device) {
		computeShader.cleanup(device);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
	}
};

namespace vkutil
{
	ShaderReflectionData generateReflectionData(std::span<uint8_t const> spirv_bytecode);
}