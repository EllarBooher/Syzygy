#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/imageview.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace syzygy
{
class DescriptorAllocator;
} // namespace syzygy

namespace syzygy
{
struct GBuffer
{
    std::unique_ptr<ImageView> diffuseColor{};
    std::unique_ptr<ImageView> specularColor{};
    std::unique_ptr<ImageView> normal{};
    std::unique_ptr<ImageView> worldPosition{};

    VkDescriptorSetLayout descriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSet descriptors{VK_NULL_HANDLE};

    // We keep these since their handles are baked into descriptors
    std::vector<VkSampler> immutableSamplers{};

    static auto create(
        VkDevice device,
        VkExtent2D drawExtent,
        VmaAllocator allocator,
        DescriptorAllocator& descriptorAllocator
    ) -> std::optional<GBuffer>;

    [[nodiscard]] auto extent() const -> VkExtent2D;

    void
    recordTransitionImages(VkCommandBuffer cmd, VkImageLayout dstLayout) const;

    void cleanup(VkDevice device);
};
} // namespace syzygy