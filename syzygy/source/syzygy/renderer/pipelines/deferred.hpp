#pragma once

#include "syzygy/core/integer.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/gbuffer.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/shaders.hpp"
#include "syzygy/renderer/shadowpass.hpp"
#include "syzygy/vulkanusage.hpp"
#include <glm/vec2.hpp>
#include <memory>
#include <span>

namespace szg_renderer
{
struct Image;
} // namespace szg_renderer

class DescriptorAllocator;
namespace szg_scene
{
struct MeshInstanced;
} // namespace szg_scene

namespace szg_renderer
{
class DeferredShadingPipeline
{
public:
    DeferredShadingPipeline(
        VkDevice device,
        VmaAllocator allocator,
        DescriptorAllocator& descriptorAllocator,
        VkExtent2D dimensionCapacity
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        VkRect2D drawRect,
        szg_renderer::Image& color,
        szg_renderer::ImageView& depth,
        std::span<szg_renderer::DirectionalLightPacked const> directionalLights,
        std::span<szg_renderer::SpotLightPacked const> spotLights,
        uint32_t viewCameraIndex,
        TStagedBuffer<szg_renderer::CameraPacked> const& cameras,
        uint32_t atmosphereIndex,
        TStagedBuffer<szg_renderer::AtmospherePacked> const& atmospheres,
        std::span<szg_scene::MeshInstanced const> sceneGeometry
    );

    void updateRenderTargetDescriptors(
        VkDevice, szg_renderer::ImageView& depthImage
    );

    void cleanup(VkDevice device, VmaAllocator allocator);

private:
    ShadowPassArray m_shadowPassArray{};

    std::unique_ptr<szg_renderer::ImageView> m_drawImage{};

    typedef TStagedBuffer<szg_renderer::DirectionalLightPacked>
        LightDirectionalBuffer;
    std::unique_ptr<LightDirectionalBuffer> m_directionalLights{};

    typedef TStagedBuffer<szg_renderer::SpotLightPacked> LightSpotBuffer;
    std::unique_ptr<LightSpotBuffer> m_spotLights{};

    VkDescriptorSet m_drawImageSet{VK_NULL_HANDLE};
    // Used by compute shaders to output final image
    VkDescriptorSetLayout m_drawImageLayout{VK_NULL_HANDLE};

    VkDescriptorSet m_depthImageSet{VK_NULL_HANDLE};
    // Used by compute shaders to read szg_scene depth
    VkDescriptorSetLayout m_depthImageLayout{VK_NULL_HANDLE};

    VkSampler m_depthImageImmutableSampler{VK_NULL_HANDLE};

    GBuffer m_gBuffer{};

    struct GBufferVertexPushConstant
    {
        VkDeviceAddress vertexBuffer{};
        VkDeviceAddress modelBuffer{};

        VkDeviceAddress modelInverseTransposeBuffer{};
        VkDeviceAddress cameraBuffer{};

        uint32_t cameraIndex{0};
        uint8_t padding0[12]{};
    };

    ShaderObjectReflected m_gBufferVertexShader{
        ShaderObjectReflected::makeInvalid()
    };
    ShaderObjectReflected m_gBufferFragmentShader{
        ShaderObjectReflected::makeInvalid()
    };

    VkPipelineLayout m_gBufferLayout{VK_NULL_HANDLE};

    struct LightingPassComputePushConstant
    {
        VkDeviceAddress cameraBuffer{};
        VkDeviceAddress atmosphereBuffer{};

        VkDeviceAddress directionalLightsBuffer{};
        VkDeviceAddress spotLightsBuffer{};

        uint32_t directionalLightCount{};
        uint32_t spotLightCount{};
        uint32_t atmosphereIndex{0};
        uint32_t cameraIndex{0};

        glm::vec2 gbufferOffset{};
        glm::vec2 gbufferExtent{};
    };

    ShaderObjectReflected m_lightingPassComputeShader{
        ShaderObjectReflected::makeInvalid()
    };

    VkPipelineLayout m_lightingPassLayout{VK_NULL_HANDLE};

    struct SkyPassComputePushConstant
    {
        VkDeviceAddress atmosphereBuffer{};
        VkDeviceAddress cameraBuffer{};

        uint32_t atmosphereIndex{0};
        uint32_t cameraIndex{0};

        glm::vec2 drawOffset{};
        glm::vec2 drawExtent{};

        uint8_t padding0[8]{};
    };

    ShaderObjectReflected m_skyPassComputeShader{
        ShaderObjectReflected::makeInvalid()
    };

    VkPipelineLayout m_skyPassLayout{VK_NULL_HANDLE};

public:
    struct Parameters
    {
        ShadowPassParameters shadowPassParameters{};
    };
    Parameters m_parameters;
};
} // namespace szg_renderer