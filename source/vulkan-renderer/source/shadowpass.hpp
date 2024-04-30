#pragma once

#include "enginetypes.hpp"

struct ShadowPass
{
    AllocatedImage depthImage{};
    VkSampler depthSampler{ VK_NULL_HANDLE };
    std::unique_ptr<OffscreenPassInstancedMeshGraphicsPipeline> pipeline{};

    VkDescriptorSetLayout shadowMapDescriptorLayout{ VK_NULL_HANDLE };
    VkDescriptorSet shadowMapDescriptors{ VK_NULL_HANDLE };

    ShadowPassParameters parameters;
   
    void cleanup(VkDevice device, VmaAllocator allocator)
    {
        depthImage.cleanup(device, allocator);

        vkDestroySampler(device, depthSampler, nullptr);

        pipeline->cleanup(device);

        vkDestroyDescriptorSetLayout(device, shadowMapDescriptorLayout, nullptr);

        pipeline.reset();
        depthImage = {};
    }
};