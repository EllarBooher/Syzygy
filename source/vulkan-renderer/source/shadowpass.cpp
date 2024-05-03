#include "shadowpass.hpp"
#include "helpers.hpp"
#include "images.hpp"
#include "initializers.hpp"

ShadowPass ShadowPass::create(
    VkDevice device
    , DescriptorAllocator& descriptorAllocator
    , VmaAllocator allocator
    , uint32_t shadowMapSize
)
{
    // The descriptor for accessing the shadow map in shaders
    VkDescriptorSetLayout const shadowMapDescriptorLayout{
        DescriptorLayoutBuilder{}
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 1, 0)
            .build(device, 0)
    };

    VkDescriptorSet const shadowMapDescriptors{ descriptorAllocator.allocate(device, shadowMapDescriptorLayout) };

    VkExtent3D const shadowmapExtent{
        .width{ shadowMapSize },
        .height{ shadowMapSize },
        .depth{ 1 }
    };

    // The shadow map image
    AllocatedImage const depthImage{ 
        AllocatedImage::allocate(
            allocator
            , device
            , shadowmapExtent
            , VK_FORMAT_D32_SFLOAT
            , VK_IMAGE_ASPECT_DEPTH_BIT
            , VK_IMAGE_USAGE_SAMPLED_BIT // read in main render pass
            | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        ) 
    };

    VkSamplerCreateInfo const shadowMapSampler{
        vkinit::samplerCreateInfo(0, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
    };

    VkSampler depthSampler{ VK_NULL_HANDLE };
    LogVkResult(vkCreateSampler(device, &shadowMapSampler, nullptr, &depthSampler), "Creating Shadow Pass Sampler");

    VkDescriptorImageInfo const shadowMapInfo{
        .sampler{ depthSampler },
        .imageView{ depthImage.imageView },
        .imageLayout{ VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL },
    };

    VkWriteDescriptorSet const shadowMapWrite{
        .sType{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
        .pNext{ nullptr },

        .dstSet{ shadowMapDescriptors },
        .dstBinding{ 0 },
        .dstArrayElement{ 0 },
        .descriptorCount{ 1 },
        .descriptorType{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },

        .pImageInfo{ &shadowMapInfo },
        .pBufferInfo{ nullptr },
        .pTexelBufferView{ nullptr },
    };

    std::vector<VkWriteDescriptorSet> const writes{
        shadowMapWrite
    };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    auto pipeline{ std::make_unique<OffscreenPassInstancedMeshGraphicsPipeline>(
        device,
        VK_FORMAT_D32_SFLOAT
    ) };

    return ShadowPass{
        .depthImage{ depthImage },
        .depthSampler{ depthSampler },
        .pipeline{ std::move(pipeline) },
        .shadowMapDescriptorLayout{ shadowMapDescriptorLayout },
        .shadowMapDescriptors{ shadowMapDescriptors },
    };
}