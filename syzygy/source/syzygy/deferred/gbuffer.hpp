#pragma once

#include "syzygy/images.hpp"
#include "syzygy/vulkanusage.hpp"
#include <memory>
#include <optional>
#include <vector>

struct DescriptorAllocator;

struct GBuffer
{
    std::unique_ptr<szg_image::ImageView> diffuseColor{};
    std::unique_ptr<szg_image::ImageView> specularColor{};
    std::unique_ptr<szg_image::ImageView> normal{};
    std::unique_ptr<szg_image::ImageView> worldPosition{};

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