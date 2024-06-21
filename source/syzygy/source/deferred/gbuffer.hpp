#pragma once

#include "../descriptors.hpp"
#include "../enginetypes.hpp"
#include "../images.hpp"

struct GBuffer
{
    std::unique_ptr<AllocatedImage> diffuseColor{};
    std::unique_ptr<AllocatedImage> specularColor{};
    std::unique_ptr<AllocatedImage> normal{};
    std::unique_ptr<AllocatedImage> worldPosition{};

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

    VkExtent2D extent() const { return diffuseColor->extent2D(); }

    void
    recordTransitionImages(VkCommandBuffer cmd, VkImageLayout dstLayout) const;

    void cleanup(VkDevice device);
};