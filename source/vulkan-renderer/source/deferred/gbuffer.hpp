#pragma once

#include "../enginetypes.hpp"
#include "../images.hpp"
#include "../descriptors.hpp"

struct GBuffer
{
    AllocatedImage diffuseColor{};
    AllocatedImage specularColor{};
    AllocatedImage normal{};
    AllocatedImage worldPosition{};

    VkDescriptorSetLayout descriptorLayout{ VK_NULL_HANDLE };
    VkDescriptorSet descriptors{ VK_NULL_HANDLE };

    // We keep these around since every usage of the descriptor layout relies on them
    std::vector<VkSampler> immutableSamplers{};

    static std::optional<GBuffer> create(
        VkDevice device
        , VkExtent2D drawExtent
        , VmaAllocator allocator
        , DescriptorAllocator& descriptorAllocator
    );

    void recordTransitionImages(VkCommandBuffer cmd, VkImageLayout srcLayout, VkImageLayout dstLayout);

    void cleanup(
        VkDevice device
        , VmaAllocator allocator
    );
};