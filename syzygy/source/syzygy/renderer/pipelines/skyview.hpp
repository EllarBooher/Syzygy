#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/shaders.hpp"
#include <glm/vec2.hpp>
#include <memory>

namespace syzygy
{
struct AtmospherePacked;
struct CameraPacked;
struct DirectionalLightPacked;
struct SceneTexture;
template <typename T> struct TStagedBuffer;
struct GBuffer;
struct ShadowPassArray;
} // namespace syzygy

namespace syzygy
{
struct SkyViewComputePipeline
{
public:
    auto operator=(SkyViewComputePipeline&&)
        -> SkyViewComputePipeline& = delete;
    SkyViewComputePipeline(SkyViewComputePipeline const&) = delete;
    auto operator=(SkyViewComputePipeline const&)
        -> SkyViewComputePipeline& = delete;

    SkyViewComputePipeline(SkyViewComputePipeline&&) noexcept;
    ~SkyViewComputePipeline();

    [[nodiscard]] static auto create(VkDevice device, VmaAllocator allocator)
        -> std::unique_ptr<SkyViewComputePipeline>;

    void recordDrawCommands(
        VkCommandBuffer cmd,
        SceneTexture& sceneTexture,
        VkRect2D drawRect,
        GBuffer const& gbuffer,
        ShadowPassArray const& shadowMaps,
        uint32_t atmosphereIndex,
        TStagedBuffer<AtmospherePacked> const& atmospheres,
        uint32_t viewCameraIndex,
        TStagedBuffer<CameraPacked> const& cameras,
        uint32_t sunLightIndex,
        TStagedBuffer<DirectionalLightPacked> const& lights
    );

    struct TransmittanceLUTResources
    {
        std::unique_ptr<ImageView> map{};

        // Shader excerpt:
        // set = 0
        // binding = 0 -> image2D transmittance_LUT;
        VkDescriptorSet set{VK_NULL_HANDLE};
        VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
        VkPipelineLayout layout{VK_NULL_HANDLE};

        struct PushConstant
        {
            VkDeviceAddress atmosphereBuffer{};
            uint32_t atmosphereIndex{0};

            // NOLINTBEGIN(modernize-avoid-c-arrays, readability-magic-numbers)
            uint8_t padding[4]{0};
            // NOLINTEND(modernize-avoid-c-arrays, readability-magic-numbers)
        };
        ShaderObjectReflected shader{ShaderObjectReflected::makeInvalid()};
    };
    struct SkyViewLUTResources
    {
        std::unique_ptr<ImageView> map{};

        // Shader excerpt:
        // set = 0
        // binding = 0 -> image2D skyview_LUT
        // binding = 1 -> sampler2D transmittance_LUT
        VkDescriptorSet set{VK_NULL_HANDLE};
        VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
        VkPipelineLayout layout{VK_NULL_HANDLE};

        VkSampler transmittanceImmutableSampler{VK_NULL_HANDLE};

        struct PushConstant
        {
            VkDeviceAddress atmosphereBuffer{};
            VkDeviceAddress cameraBuffer{};

            uint32_t atmosphereIndex{0};
            uint32_t cameraIndex{0};
        };
        ShaderObjectReflected shader{ShaderObjectReflected::makeInvalid()};
    };
    struct PerspectiveMapResources
    {
        // Shader excerpt:
        // layout(rgba16, set = 0, binding = 0) uniform image2D image;
        // layout(set = 0, binding = 1) uniform sampler2D fragmentDepth;
        //
        // layout(set = 1, binding = 0) uniform sampler2D skyview_LUT;
        // layout(set = 1, binding = 1) uniform sampler2D transmittance_LUT;

        // set 0 is provided by the SceneTexture we are rendering into

        VkDescriptorSetLayout sceneTextureLayout{VK_NULL_HANDLE};

        VkDescriptorSet LUTSet{VK_NULL_HANDLE};
        VkDescriptorSetLayout LUTSetLayout{VK_NULL_HANDLE};

        VkDescriptorSetLayout GBufferSetLayout{VK_NULL_HANDLE};

        VkDescriptorSetLayout shadowMapSamplerSetLayout{VK_NULL_HANDLE};
        VkDescriptorSetLayout shadowMapTextureSetLayout{VK_NULL_HANDLE};

        VkPipelineLayout layout{VK_NULL_HANDLE};

        VkSampler skyviewImmutableSampler{VK_NULL_HANDLE};
        VkSampler transmittanceImmutableSampler{VK_NULL_HANDLE};

        struct PushConstant
        {
            VkDeviceAddress atmosphereBuffer{};
            VkDeviceAddress cameraBuffer{};

            uint32_t atmosphereIndex{0};
            uint32_t cameraIndex{0};

            glm::uvec2 drawExtent{};

            uint32_t sunShadowMapIndex{};
            // NOLINTBEGIN(modernize-avoid-c-arrays, readability-magic-numbers)
            uint8_t padding[4]{0};
            // NOLINTEND(modernize-avoid-c-arrays, readability-magic-numbers)

            glm::uvec2 gbufferExtent{};

            VkDeviceAddress directionalLights{};
            uint32_t sunLightIndex{0};
        };
        ShaderObjectReflected shader{ShaderObjectReflected::makeInvalid()};
    };

private:
    SkyViewComputePipeline() = default;
    void destroy();

    bool m_hasAllocations{false};

    VkDevice m_device{VK_NULL_HANDLE};

    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator{};

    TransmittanceLUTResources m_transmittanceLUT{};
    SkyViewLUTResources m_skyViewLUT{};
    PerspectiveMapResources m_perspectiveMap{};
};
} // namespace syzygy