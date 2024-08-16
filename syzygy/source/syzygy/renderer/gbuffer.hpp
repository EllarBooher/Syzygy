#pragma once

#include "syzygy/renderer/imageview.hpp"
#include "syzygy/vulkanusage.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace syzygy
{
struct DescriptorAllocator;
}

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

    static std::optional<GBuffer> create(
        VkDevice device,
        VkExtent2D drawExtent,
        VmaAllocator allocator,
        DescriptorAllocator& descriptorAllocator
    );

    VkExtent2D extent() const;

    void
    recordTransitionImages(VkCommandBuffer cmd, VkImageLayout dstLayout) const;

    void cleanup(VkDevice device);
};
} // namespace syzygy