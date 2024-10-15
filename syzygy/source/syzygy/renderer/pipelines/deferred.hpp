#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/gbuffer.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/shaders.hpp"
#include "syzygy/renderer/shadowpass.hpp"
#include <glm/vec2.hpp>
#include <memory>
#include <span>

namespace syzygy
{
struct MeshInstanced;
struct DescriptorAllocator;
struct SceneTexture;
} // namespace syzygy

namespace syzygy
{
struct DeferredShadingPipeline
{
public:
    DeferredShadingPipeline(
        VkDevice device,
        VmaAllocator allocator,
        SceneTexture const& sceneTexture,
        DescriptorAllocator& descriptorAllocator,
        VkExtent2D dimensionCapacity
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        VkRect2D drawRect,
        SceneTexture& sceneTexture,
        uint32_t atmosphericDirectionalLightsCount,
        TStagedBuffer<DirectionalLightPacked> const& directionalLights,
        std::span<syzygy::SpotLightPacked const> spotLights,
        uint32_t viewCameraIndex,
        TStagedBuffer<syzygy::CameraPacked> const& cameras,
        std::span<std::reference_wrapper<MeshRenderResources> const>
            sceneGeometry
    );

    [[nodiscard]] auto gbuffer() -> GBuffer const&;
    [[nodiscard]] auto shadowMaps() -> ShadowPassArray const&;

    void cleanup(VkDevice device, VmaAllocator allocator);

private:
    ShadowPassArray m_shadowPassArray{};

    using LightSpotBuffer = TStagedBuffer<syzygy::SpotLightPacked>;
    std::unique_ptr<LightSpotBuffer> m_spotLights{};

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
        // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
        uint8_t padding0to1[8]{};

        VkDeviceAddress directionalLightsBuffer{};
        VkDeviceAddress spotLightsBuffer{};

        uint32_t directionalLightCount{};
        uint32_t spotLightCount{};
        uint32_t directionalLightSkipCount{};
        uint32_t cameraIndex{0};

        glm::vec2 gbufferOffset{};
        glm::vec2 gbufferExtent{};
    };

    ShaderObjectReflected m_lightingPassComputeShader{
        ShaderObjectReflected::makeInvalid()
    };

    VkDescriptorSetLayout m_shadowPassArraySamplerSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_shadowPassArrayTextureSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout m_lightingPassLayout{VK_NULL_HANDLE};

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