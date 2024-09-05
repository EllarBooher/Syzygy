#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include <glm/mat4x4.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace syzygy
{
class DescriptorAllocator;
struct DirectionalLightPacked;
struct SpotLightPacked;
struct MeshInstanced;
} // namespace syzygy

namespace syzygy
{
struct ShadowPassParameters
{
    static float constexpr REVERSE_Z_CONSTANT{-2.00F};
    static float constexpr REVERSE_Z_SLOPE{-1.75F};

    // Reverse-Z means to avoid acne, we push depth values in the negative
    // direction towards 0
    float depthBiasConstant{REVERSE_Z_CONSTANT};
    float depthBiasSlope{REVERSE_Z_SLOPE};
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

    // Prepares shadow maps for a specified number of syzygy.
    // Calling this twice overwrites the previous results.
    void recordInitialize(
        VkCommandBuffer cmd,
        ShadowPassParameters parameters,
        std::span<syzygy::DirectionalLightPacked const> directionalLights,
        std::span<syzygy::SpotLightPacked const> spotLights
    );

    void recordDrawCommands(
        VkCommandBuffer cmd,
        std::span<syzygy::MeshInstanced const> geometry,
        std::span<RenderOverride const> renderOverrides
    );

    // Transitions all the shadow map VkImages, with a total memory barrier.
    void
    recordTransitionActiveShadowMaps(VkCommandBuffer, VkImageLayout dstLayout);

    [[nodiscard]] auto samplerSetLayout() const -> VkDescriptorSetLayout;
    [[nodiscard]] auto texturesSetLayout() const -> VkDescriptorSetLayout;
    [[nodiscard]] auto samplerSet() const -> VkDescriptorSet;
    [[nodiscard]] auto textureSet() const -> VkDescriptorSet;

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

    std::vector<std::unique_ptr<syzygy::ImageView>> m_shadowmaps{};

    VkDescriptorSetLayout m_shadowmapSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_shadowmapSet{VK_NULL_HANDLE};

    std::unique_ptr<OffscreenPassGraphicsPipeline> m_pipeline{};
};
} // namespace syzygy