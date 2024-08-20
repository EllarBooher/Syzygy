#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace syzygy
{
// Contains reflected data from a ShaderModule, to aid with UI
// and proper piping of data.
// Work in progress, for now supports a very limited amount of reflection.
struct ShaderReflectionData
{
    // Type names correspond to the SPIR-V specification.
    // The type names are not meant to exactly match the specification opcodes
    // and layouts, just model it in a way that's useful.
    //
    // See https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html
    // Section "2.2.2. Types"

    // Corresponds to OpTypeInt
    struct Integer
    {
        bool signedness;

        auto operator==(Integer const& other) const -> bool;
    };

    // Corresponds to OpTypeFloat
    using Float = std::monostate;
    using Scalar = std::monostate;

    struct Vector
    {
        uint32_t componentCount;

        auto operator==(Vector const& other) const -> bool;
    };
    struct Matrix
    {
        uint32_t columnCount;
        uint32_t rowCount;

        auto operator==(Matrix const& other) const -> bool;
    };

    struct NumericType
    {
        using ComponentType = std::variant<Integer, Float>;
        using Format = std::variant<Scalar, Vector, Matrix>;

        uint32_t componentBitWidth;
        ComponentType componentType;
        Format format;

        auto operator==(NumericType const& other) const -> bool;
    };

    // TODO: add info of what it points to
    struct Pointer
    {
        auto operator==(Pointer const& /*other*/) const -> bool = default;
    };

    // Represents a type whose reflection data could not be generated,
    // usually because the specific type is not supported yet.
    using UnsupportedType = std::monostate;

    struct SizedType
    {
        std::variant<NumericType, Pointer, UnsupportedType> typeData;

        std::string name;
        uint32_t sizeBytes;
        uint32_t paddedSizeBytes;
    };

    struct Member
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
        std::vector<Member> members;

        // Mutually checks if the members of this struct match
        // any bitwise overlapping members in the other struct.
        [[nodiscard]] auto logicallyCompatible(Structure const& other) const
            -> bool;
    };

    // TODO: structs can have padding, as in bits in their representation
    // that are not overlapped by members. I need to investigate exactly
    // how this works, and how best to model this.

    // As per the Vulkan specification, Push constants must be structs.
    // There can also only be one per entry point.
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#interfaces-resources-pushconst
    struct PushConstant
    {
        Structure type{};
        std::string name{};

        // This is the minimum offset in the struct.
        // The reflection data will include the implicit padding before this
        // offset in the total size.
        uint32_t layoutOffsetBytes{0};

        [[nodiscard]] auto totalRange(VkShaderStageFlags const stageFlags) const
            -> VkPushConstantRange
        {
            return VkPushConstantRange{
                .stageFlags = stageFlags,
                .offset = layoutOffsetBytes,
                .size = type.sizeBytes - layoutOffsetBytes,
            };
        }
    };

    std::map<std::string, PushConstant> pushConstantsByEntryPoint{};

    std::string defaultEntryPoint{};

    [[nodiscard]] auto defaultEntryPointHasPushConstant() const -> bool
    {
        return pushConstantsByEntryPoint.contains(defaultEntryPoint);
    }

    [[nodiscard]] auto defaultPushConstant() const -> PushConstant const&
    {
        return pushConstantsByEntryPoint.at(defaultEntryPoint);
    }
};

auto generateReflectionData(std::span<uint8_t const> spirv_bytecode)
    -> ShaderReflectionData;

class ShaderReflectedBase
{
public:
    ShaderReflectedBase() = delete;

    [[nodiscard]] auto reflectionData() const -> ShaderReflectionData const&
    {
        return m_reflectionData;
    }
    [[nodiscard]] auto name() const -> std::string { return m_name; }

    void cleanup(VkDevice device);

protected:
    ShaderReflectedBase(
        std::string name,
        ShaderReflectionData reflectionData,
        std::variant<VkShaderModule, VkShaderEXT> shaderHandle
    )
        : m_name(std::move(name))
        , m_reflectionData(std::move(reflectionData))
        , m_shaderHandle(shaderHandle)
    {
    }

    [[nodiscard]] auto shaderModule() const -> VkShaderModule
    {
        return std::get<VkShaderModule>(m_shaderHandle);
    }
    [[nodiscard]] auto shaderObject() const -> VkShaderEXT
    {
        return std::get<VkShaderEXT>(m_shaderHandle);
    }

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
        : ShaderReflectedBase(
            std::move(name), std::move(reflectionData), shaderHandle
        )
    {
    }

public:
    static auto FromBytecode(
        VkDevice device,
        std::string const& name,
        std::span<uint8_t const> spirvBytecode
    ) -> std::optional<ShaderModuleReflected>;
    static auto MakeInvalid() -> ShaderModuleReflected
    {
        return ShaderModuleReflected(
            "invalid_shader_module", {}, VK_NULL_HANDLE
        );
    }

    [[nodiscard]] auto shaderModule() const -> VkShaderModule
    {
        return ShaderReflectedBase::shaderModule();
    }
};

class ShaderObjectReflected : public ShaderReflectedBase
{
private:
    ShaderObjectReflected(
        std::string name,
        ShaderReflectionData reflectionData,
        VkShaderEXT shaderHandle
    )
        : ShaderReflectedBase(
            std::move(name), std::move(reflectionData), shaderHandle
        )
    {
    }

public:
    static auto fromBytecode(
        VkDevice device,
        std::string const& name,
        std::span<uint8_t const> spirvBytecode,
        VkShaderStageFlagBits stage,
        VkShaderStageFlags nextStage,
        std::span<VkDescriptorSetLayout const> layouts,
        std::span<VkPushConstantRange const> pushConstantRanges,
        VkSpecializationInfo specializationInfo
    ) -> std::optional<ShaderObjectReflected>;

    // Compiles a shader object,
    // but derives push constant data from reflection
    static auto fromBytecodeReflected(
        VkDevice device,
        std::string const& name,
        std::span<uint8_t const> spirvBytecode,
        VkShaderStageFlagBits stage,
        VkShaderStageFlags nextStage,
        std::span<VkDescriptorSetLayout const> layouts,
        VkSpecializationInfo specializationInfo
    ) -> std::optional<ShaderObjectReflected>;

    static auto makeInvalid() -> ShaderObjectReflected
    {
        return ShaderObjectReflected(
            "invalid_shader_object", {}, VK_NULL_HANDLE
        );
    }

    [[nodiscard]] auto shaderObject() const -> VkShaderEXT
    {
        return ShaderReflectedBase::shaderObject();
    }
};

template <typename T> struct ShaderResult
{
    T shader;
    VkResult result;
};

auto compileShaderObject(
    VkDevice device,
    std::span<uint8_t const> spirvBytecode,
    VkShaderStageFlagBits stage,
    VkShaderStageFlags nextStage,
    std::span<VkDescriptorSetLayout const> layouts,
    std::span<VkPushConstantRange const> pushConstantRanges,
    VkSpecializationInfo specializationInfo
) -> ShaderResult<VkShaderEXT>;

auto compileShaderModule(
    VkDevice device, std::span<uint8_t const> spirvBytecode
) -> ShaderResult<VkShaderModule>;

auto loadShaderObject(
    VkDevice device,
    std::filesystem::path const& path,
    VkShaderStageFlagBits stage,
    VkShaderStageFlags nextStage,
    std::span<VkDescriptorSetLayout const> layouts,
    VkSpecializationInfo specializationInfo
) -> std::optional<ShaderObjectReflected>;
auto loadShaderObject(
    VkDevice device,
    std::filesystem::path const& path,
    VkShaderStageFlagBits stage,
    VkShaderStageFlags nextStage,
    std::span<VkDescriptorSetLayout const> layouts,
    VkPushConstantRange rangeOverride,
    VkSpecializationInfo specializationInfo
) -> std::optional<ShaderObjectReflected>;

auto loadShaderModule(VkDevice device, std::string const& path)
    -> std::optional<ShaderModuleReflected>;

struct ComputeShaderWrapper
{
    ShaderModuleReflected computeShader{ShaderModuleReflected::MakeInvalid()};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};

    void cleanup(VkDevice device)
    {
        computeShader.cleanup(device);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
    }
};
} // namespace syzygy