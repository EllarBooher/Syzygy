#pragma once

#include "syzygy/core/integer.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/vulkanusage.hpp"
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace szg_renderer
{
class DescriptorAllocator;
struct DirectionalLightPacked;
struct SpotLightPacked;
} // namespace szg_renderer
namespace szg_scene
{
struct MeshInstanced;
}

namespace szg_renderer
{
struct ShadowPassParameters
{
    float depthBiasConstant{2.00f};
    float depthBiasSlope{-1.75f};
};

// Handles the resources for an array of depth maps,
// which share a sampler and should be accessed via a descriptor array.
// TODO: Implement move/copy/constructors to properly manage resources
class ShadowPassArray
{
public:
    static size_t constexpr SHADOWPASS_CAMERA_CAPACITY{100};

    static auto create(
        VkDevice,
        DescriptorAllocator&,
        VmaAllocator,
        VkExtent2D shadowmapExtent,
        size_t capacity
    ) -> std::optional<ShadowPassArray>;

    // Prepares shadow maps for a specified number of szg_renderer.
    // Calling this twice overwrites the previous results.
    void recordInitialize(
        VkCommandBuffer cmd,
        ShadowPassParameters parameters,
        std::span<szg_renderer::DirectionalLightPacked const> directionalLights,
        std::span<szg_renderer::SpotLightPacked const> spotLights
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        std::span<szg_scene::MeshInstanced const> geometry,
        std::span<RenderOverride const> renderOverrides
    );

    // Transitions all the shadow map VkImages, with a total memory barrier.
    void
    recordTransitionActiveShadowMaps(VkCommandBuffer, VkImageLayout dstLayout);

    auto samplerSetLayout() const -> VkDescriptorSetLayout;
    auto texturesSetLayout() const -> VkDescriptorSetLayout;
    auto samplerSet() const -> VkDescriptorSet;
    auto textureSet() const -> VkDescriptorSet;

    void cleanup(VkDevice, VmaAllocator);

private:
    float m_depthBias{0};
    float m_depthBiasSlope{0};
    // Each of these staged values represents
    // a shadow map we are going to write
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> m_projViewMatrices{};

    VmaAllocator m_allocator{VK_NULL_HANDLE};

    VkSampler m_sampler{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_samplerSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_samplerSet{VK_NULL_HANDLE};

    std::vector<std::unique_ptr<szg_renderer::ImageView>> m_shadowmaps{};

    VkDescriptorSetLayout m_shadowmapSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_shadowmapSet{VK_NULL_HANDLE};

    std::unique_ptr<OffscreenPassGraphicsPipeline> m_pipeline{};
};
} // namespace szg_renderer