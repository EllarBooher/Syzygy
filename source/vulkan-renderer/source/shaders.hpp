#pragma once

#include "enginetypes.hpp"
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

		bool operator==(Integer const& other) const;
	};

	// Corresponds to OpTypeFloat
	using Float = std::monostate;
	using Scalar = std::monostate;

	struct Vector {
		uint32_t componentCount;

		bool operator==(Vector const& other) const;
	};
	struct Matrix {
		uint32_t columnCount;
		uint32_t rowCount;

		bool operator==(Matrix const& other) const;
	};

	struct NumericType
	{
		using ComponentType = std::variant<Integer, Float>;
		using Format = std::variant<Scalar, Vector, Matrix>;

		uint32_t componentBitWidth;
		ComponentType componentType;
		Format format;

		bool operator==(NumericType const& other) const;
	};

	// TODO: add info of what it points to
	struct Pointer
	{
		bool operator==(Pointer const& other) const = default;
	};

	/**
		Represents a type whose reflection data could not be generated,
		usually because the specific type is not supported yet.
	*/
	using UnsupportedType = std::monostate;

	struct SizedType
	{
		std::variant<NumericType, Pointer, UnsupportedType> typeData;

		std::string name;
		uint32_t sizeBytes;
		uint32_t paddedSizeBytes;
	};

	struct StructureMember
	{
		uint32_t offsetBytes;
		std::string name;
		SizedType type;
	};
	// Corresponds to OpTypeStruct
	struct Structure
	{
		// TODO: test if structures can be anonymous.
		std::string name;
		uint32_t sizeBytes;
		// TODO: figure out what exactly determines the padding size
		uint32_t paddedSizeBytes;
		std::vector<StructureMember> members;

		/**
			Mutually checks if the members of this struct match any bitwise overlapping members in the other struct.
		*/
		bool logicallyCompatible(Structure const& other) const;
	};

	/*
		TODO: structs can have padding, as in bits in their representation that are not overlapped by members.
		I need to investigate exactly how this works, and how best to model this.
		This is important for push constants, since padding bits in one shader may be
		accessed in another.
	*/

	/**
		As per the Vulkan specification, Push constants must be structs.
		There can also only be one per entry point.
		https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#interfaces-resources-pushconst
	*/
	struct PushConstant
	{
		Structure type{};
		std::string name{};

		// This is the minimum offset in the struct.
		// The reflection data will include the implicit padding before this offset in the total size.
		uint32_t layoutOffsetBytes{ 0 };

		VkPushConstantRange totalRange(VkShaderStageFlags stageFlags) const
		{
			return VkPushConstantRange{
				.stageFlags{ stageFlags },
				.offset{ layoutOffsetBytes },
				.size{ type.sizeBytes - layoutOffsetBytes },
			};
		}
	};
	std::map<std::string, PushConstant> pushConstantsByEntryPoint{};

	std::string defaultEntryPoint{};

	bool defaultEntryPointHasPushConstant() const { return pushConstantsByEntryPoint.contains(defaultEntryPoint); }
	PushConstant const& defaultPushConstant() const 
	{ 
		return pushConstantsByEntryPoint.at(defaultEntryPoint); 
	}
};

class ShaderReflectedBase
{
public:
	ShaderReflectedBase() = delete;

	ShaderReflectionData const& reflectionData() const { return m_reflectionData; }
	std::string name() const { return m_name; }

	void cleanup(VkDevice device);

protected:
	ShaderReflectedBase(
		std::string name,
		ShaderReflectionData reflectionData,
		std::variant<VkShaderModule, VkShaderEXT> shaderHandle
	)
		: m_name(name)
		, m_reflectionData(reflectionData)
		, m_shaderHandle(shaderHandle)
	{}

	VkShaderModule shaderModule() const { return std::get<VkShaderModule>(m_shaderHandle); }
	VkShaderEXT shaderObject() const { return std::get<VkShaderEXT>(m_shaderHandle); }

private:
	std::string m_name{};
	ShaderReflectionData m_reflectionData{};
	std::variant<VkShaderModule, VkShaderEXT> m_shaderHandle{};
};

class ShaderModuleReflected : public ShaderReflectedBase
{
private:
	ShaderModuleReflected(
		std::string name,
		ShaderReflectionData reflectionData,
		VkShaderModule shaderHandle
	)
		: ShaderReflectedBase(name, reflectionData, shaderHandle)
	{}

public:
	static std::optional<ShaderModuleReflected> FromBytecode(
		VkDevice device
		, std::string name
		, std::span<uint8_t const> spirvBytecode
	);
	static ShaderModuleReflected MakeInvalid()
	{
		return ShaderModuleReflected("", {}, VK_NULL_HANDLE);
	}

	VkShaderModule shaderModule() const { return ShaderReflectedBase::shaderModule(); }
};

class ShaderObjectReflected : public ShaderReflectedBase
{
private:
	ShaderObjectReflected(
		std::string name,
		ShaderReflectionData reflectionData,
		VkShaderEXT shaderHandle
	)
		: ShaderReflectedBase(name, reflectionData, shaderHandle)
	{}

public:
	static std::optional<ShaderObjectReflected> FromBytecode(
		VkDevice device
		, std::string name
		, std::span<uint8_t const> spirvBytecode
		, VkShaderStageFlagBits stage
		, VkShaderStageFlags nextStage
		, std::span<VkDescriptorSetLayout const> layouts
		, std::span<VkPushConstantRange const> pushConstantRanges
		, VkSpecializationInfo specializationInfo
	);
	// Compiles a shader object, but derives push constant data from reflection
	static std::optional<ShaderObjectReflected> FromBytecodeReflected(
		VkDevice device
		, std::string name
		, std::span<uint8_t const> spirvBytecode
		, VkShaderStageFlagBits stage
		, VkShaderStageFlags nextStage
		, std::span<VkDescriptorSetLayout const> layouts
		, VkSpecializationInfo specializationInfo
	);

	static ShaderObjectReflected MakeInvalid()
	{
		return ShaderObjectReflected("", {}, VK_NULL_HANDLE);
	}

	VkShaderEXT shaderObject() const { return ShaderReflectedBase::shaderObject(); }
};

namespace vkutil
{
	template<typename T>
	struct ShaderResult
	{
		T shader;
		VkResult result;
	};

	ShaderResult<VkShaderEXT> CompileShaderObject(
		VkDevice device
		, std::span<uint8_t const> spirvBytecode
		, VkShaderStageFlagBits stage
		, VkShaderStageFlags nextStage
		, std::span<VkDescriptorSetLayout const> layouts
		, std::span<VkPushConstantRange const> pushConstantRanges
		, VkSpecializationInfo specializationInfo
	);

	ShaderResult<VkShaderModule> CompileShaderModule(
		VkDevice device
		, std::span<uint8_t const> spirvBytecode
	);

	std::optional<ShaderObjectReflected> loadShaderObject(
		VkDevice device
		, std::string path
		, VkShaderStageFlagBits stage
		, VkShaderStageFlags nextStage
		, std::span<VkDescriptorSetLayout const> layouts
		, VkSpecializationInfo specializationInfo
	);
	std::optional<ShaderObjectReflected> loadShaderObject(
		VkDevice device
		, std::string path
		, VkShaderStageFlagBits stage
		, VkShaderStageFlags nextStage
		, std::span<VkDescriptorSetLayout const> layouts
		, VkPushConstantRange rangeOverride
		, VkSpecializationInfo specializationInfo
	);

	std::optional<ShaderModuleReflected> loadShaderModule(
		VkDevice device
		, std::string path
	);
}

struct ComputeShaderWrapper
{
	ShaderModuleReflected computeShader{ ShaderModuleReflected::MakeInvalid() };
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