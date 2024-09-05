#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/gbuffer.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/shaders.hpp"
#include "syzygy/renderer/shadowpass.hpp"
#include <glm/vec2.hpp>
#include <memory>
#include <span>

namespace syzygy
{
struct Image;
struct MeshInstanced;
class DescriptorAllocator;
} // namespace syzygy

namespace syzygy
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
        syzygy::Image& color,
        syzygy::ImageView& depth,
        std::span<syzygy::DirectionalLightPacked const> directionalLights,
        std::span<syzygy::SpotLightPacked const> spotLights,
        uint32_t viewCameraIndex,
        TStagedBuffer<syzygy::CameraPacked> const& cameras,
        uint32_t atmosphereIndex,
        TStagedBuffer<syzygy::AtmospherePacked> const& atmospheres,
        std::span<syzygy::MeshInstanced const> sceneGeometry
    );

    void updateRenderTargetDescriptors(VkDevice, syzygy::ImageView& depthImage);

    void cleanup(VkDevice device, VmaAllocator allocator);

private:
    ShadowPassArray m_shadowPassArray{};

    std::unique_ptr<syzygy::ImageView> m_drawImage{};

    using LightDirectionalBuffer =
        TStagedBuffer<syzygy::DirectionalLightPacked>;
    std::unique_ptr<LightDirectionalBuffer> m_directionalLights{};

    using LightSpotBuffer = TStagedBuffer<syzygy::SpotLightPacked>;
    std::unique_ptr<LightSpotBuffer> m_spotLights{};

    VkDescriptorSet m_drawImageSet{VK_NULL_HANDLE};
    // Used by compute shaders to output final image
    VkDescriptorSetLayout m_drawImageLayout{VK_NULL_HANDLE};

    VkDescriptorSet m_depthImageSet{VK_NULL_HANDLE};
    // Used by compute shaders to read syzygy depth
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

        // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
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

        // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
        uint8_t padding0[8]{};
    };

    ShaderObjectReflected m_skyPassComputeShader{
        ShaderObjectReflected::makeInvalid()
    };

    VkPipelineLayout m_skyPassLayout{VK_NULL_HANDLE};

public:
    struct Configuration
    {
        ShadowPassParameters shadowPassParameters{};
    };

    [[nodiscard]] auto getConfiguration() const -> Configuration;
    void setConfiguration(Configuration);

private:
    Configuration m_configuration;
};
} // namespace syzygy