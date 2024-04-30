#pragma once

#include "enginetypes.hpp"

struct ShadowPass
{
    AllocatedImage depthImage{};
    VkSampler depthSampler{};
    std::unique_ptr<OffscreenPassInstancedMeshGraphicsPipeline> pipeline{};
    float depthBias{ 2.00f };
    float depthBiasSlope{ -1.75f };

    glm::vec3 forward{ 0.0, 1.0, 0.0 };
    glm::vec3 center{ 0.0, -4.0, 0.0 };
    glm::vec3 extent{ 45.0, 4.0, 45.0 };

    void cleanup(VkDevice device, VmaAllocator allocator)
    {
        depthImage.cleanup(device, allocator);
        pipeline->cleanup(device);
        pipeline.reset();
        depthImage = {};
    }
};