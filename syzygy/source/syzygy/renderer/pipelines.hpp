#pragma once

#include "syzygy/core/log.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/shaders.hpp"
#include <glm/mat4x4.hpp>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace syzygy
{
struct CameraPacked;
struct ImageView;
template <typename T> struct TStagedBuffer;
struct MeshInstanced;
struct VertexPacked;
} // namespace syzygy

namespace syzygy
{
enum class RenderingPipelines
{
    DEFERRED = 0,
    COMPUTE_COLLECTION = 1
};

auto computeDispatchCount(uint32_t invocations, uint32_t workgroupSize)
    -> uint32_t;

struct RenderOverride
{
    bool render{false};
};

struct DrawResultsGraphics
{
    size_t drawCalls{0};
    size_t verticesDrawn{0};
    size_t indicesDrawn{0};
};

class PipelineBuilder
{
public:
    PipelineBuilder() = default;

    auto buildPipeline(VkDevice device, VkPipelineLayout layout) const
        -> VkPipeline;

    void pushShader(
        ShaderModuleReflected const& shader, VkShaderStageFlagBits stage
    );

    void setInputTopology(VkPrimitiveTopology topology);

    void setPolygonMode(VkPolygonMode mode);

    void pushDynamicState(VkDynamicState dynamicState);

    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);

    void setMultisamplingNone();

    void setColorAttachment(VkFormat format);

    void setDepthFormat(VkFormat format);

    void enableDepthBias();

    void disableDepthTest();

    void enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp);

private:
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages{};
    std::set<VkDynamicState> m_dynamicStates{};

    VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    };
    VkPipelineRasterizationStateCreateInfo m_rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .lineWidth = 1.0,
    };

    struct ColorAttachmentSpecification
    {
        VkFormat format{VK_FORMAT_UNDEFINED};

        // TODO: expose blending in pipeline builder
        VkPipelineColorBlendAttachmentState blending{
            .blendEnable = VK_FALSE,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
    };

    VkPipelineMultisampleStateCreateInfo m_multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    };
    VkPipelineDepthStencilStateCreateInfo m_depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    };

    std::optional<ColorAttachmentSpecification> m_colorAttachment{};
    VkFormat m_depthAttachmentFormat{VK_FORMAT_UNDEFINED};
};

// This pipeline does an offscreen pass of some geometry
// to write depth information
class OffscreenPassGraphicsPipeline
{
public:
    OffscreenPassGraphicsPipeline(
        VkDevice device, VkFormat depthAttachmentFormat
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        bool reuseDepthAttachment,
        float depthBias,
        float depthBiasSlope,
        syzygy::ImageView& depth,
        uint32_t projViewIndex,
        TStagedBuffer<glm::mat4x4> const& projViewMatrices,
        std::span<syzygy::MeshInstanced const> geometry,
        std::span<RenderOverride const> renderOverrides
    ) const;

    void cleanup(VkDevice device);

private:
    ShaderModuleReflected m_vertexShader{ShaderModuleReflected::MakeInvalid()};

    VkPipeline m_graphicsPipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_graphicsPipelineLayout{VK_NULL_HANDLE};

    struct VertexPushConstant
    {
        VkDeviceAddress vertexBufferAddress{};
        VkDeviceAddress modelBufferAddress{};

        VkDeviceAddress projViewBufferAddress{};
        uint32_t projViewIndex{0};
        uint8_t padding0[4]{}; // NOLINT(modernize-avoid-c-arrays)
    };

public:
    [[nodiscard]] auto vertexShader() const -> ShaderModuleReflected const&
    {
        return m_vertexShader;
    };

    [[nodiscard]] auto vertexPushConstantReflected() const
        -> ShaderReflectionData::PushConstant const&
    {
        return m_vertexShader.reflectionData().defaultPushConstant();
    };
};

// A generic compute pipeline driven entirely by a push constant.
// Supports multiple shader objects, swapping between them and only using
// one of them during dispatch.
class ComputeCollectionPipeline
{
public:
    ComputeCollectionPipeline(
        VkDevice device,
        VkDescriptorSetLayout drawImageDescriptorLayout,
        std::span<std::string const> shaderPaths
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        VkDescriptorSet drawImageDescriptors,
        VkExtent2D drawExtent
    ) const;

    void cleanup(VkDevice device);

    auto mapPushConstantBytes() -> std::span<uint8_t>
    {
        return m_shaderPushConstants[m_shaderIndex];
    }
    [[nodiscard]] auto readPushConstantBytes() const -> std::span<uint8_t const>
    {
        return m_shaderPushConstants[m_shaderIndex];
    }

    [[nodiscard]] auto currentShader() const -> ShaderObjectReflected const&
    {
        return m_shaders[m_shaderIndex];
    }
    [[nodiscard]] auto currentLayout() const -> VkPipelineLayout
    {
        return m_layouts[m_shaderIndex];
    }

    void selectShader(size_t const index)
    {
        size_t const count{m_shaders.size()};
        if (count == 0)
        {
            return;
        }
        if (index >= count)
        {
            SZG_WARNING("Shader index {} is out of bounds of {}", index, count);
            return;
        }

        m_shaderIndex = index;
    }
    [[nodiscard]] auto shaderIndex() const -> size_t { return m_shaderIndex; };
    [[nodiscard]] auto shaderCount() const -> size_t
    {
        return m_shaders.size();
    };
    [[nodiscard]] auto shaders() const -> std::span<ShaderObjectReflected const>
    {
        return m_shaders;
    };

private:
    size_t m_shaderIndex{0};

    std::vector<ShaderObjectReflected> m_shaders{};
    std::vector<std::vector<uint8_t>> m_shaderPushConstants{};
    std::vector<VkPipelineLayout> m_layouts{};
};

// A pipeline that draws debug geometry such as lines and points in a compute
// pass
class DebugLineGraphicsPipeline
{
public:
    struct ImageFormats
    {
        VkFormat color;
        VkFormat depth;
    };

    DebugLineGraphicsPipeline(VkDevice device, ImageFormats formats);

    auto recordDrawCommands(
        VkCommandBuffer cmd,
        bool reuseDepthAttachment,
        float lineWidth,
        VkRect2D drawRect,
        syzygy::ImageView& color,
        syzygy::ImageView& depth,
        uint32_t cameraIndex,
        TStagedBuffer<syzygy::CameraPacked> const& cameras,
        TStagedBuffer<syzygy::VertexPacked> const& endpoints,
        TStagedBuffer<uint32_t> const& indices
    ) const -> DrawResultsGraphics;

    void cleanup(VkDevice device);

private:
    ShaderModuleReflected m_vertexShader{ShaderModuleReflected::MakeInvalid()};
    ShaderModuleReflected m_fragmentShader{ShaderModuleReflected::MakeInvalid()
    };

    struct VertexPushConstant
    {
        VkDeviceAddress vertexBuffer{};
        VkDeviceAddress cameraBuffer{};

        uint32_t cameraIndex{0};
        // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
        uint8_t padding0[12]{};
    };

    VertexPushConstant mutable m_vertexPushConstant{};

    VkPipeline m_graphicsPipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_graphicsPipelineLayout{VK_NULL_HANDLE};

public:
    auto vertexShader() const -> ShaderModuleReflected const&
    {
        return m_vertexShader;
    };
    auto vertexPushConstant() const -> VertexPushConstant const&
    {
        return m_vertexPushConstant;
    };

    auto fragmentShader() const -> ShaderModuleReflected const&
    {
        return m_fragmentShader;
    };
};
} // namespace syzygy