#include "gbuffer.hpp"

#include "../helpers.hpp"
#include "../initializers.hpp"

auto GBuffer::create(
    VkDevice const device,
    VkExtent2D const drawExtent,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator
) -> std::optional<GBuffer>
{
    VkExtent3D const extent{
        .width = drawExtent.width,
        .height = drawExtent.height,
        .depth = 1,
    };

    GBuffer buffer{};
    { // Diffuse Color
        std::optional<AllocatedImage> createDiffuseResult{
            AllocatedImage::allocate(
                allocator,
                device,
                AllocatedImage::AllocationParameters{
                    .extent = extent,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                    .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .viewFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                }
            )
        };
        if (!createDiffuseResult.has_value())
        {
            Error("Failed to create GBuffer diffuse color image.");
            return {};
        }

        buffer.diffuseColor = createDiffuseResult.value();
    }
    { // Specular Color
        std::optional<AllocatedImage> createSpecularResult{
            AllocatedImage::allocate(
                allocator,
                device,
                AllocatedImage::AllocationParameters{
                    .extent = extent,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                    .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .viewFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                }
            )
        };
        if (!createSpecularResult.has_value())
        {
            Error("Failed to create GBuffer specular color image.");
            return {};
        }

        buffer.specularColor = createSpecularResult.value();
    }
    { // Normals
        std::optional<AllocatedImage> createNormalResult{
            AllocatedImage::allocate(
                allocator,
                device,
                AllocatedImage::AllocationParameters{
                    .extent = extent,
                    .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                    .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .viewFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                }
            )
        };
        if (!createNormalResult.has_value())
        {
            Error("Failed to create GBuffer normal image.");
            return {};
        }

        buffer.normal = createNormalResult.value();
    }
    { // World Position
        std::optional<AllocatedImage> createWorldPositionResult{
            AllocatedImage::allocate(
                allocator,
                device,
                AllocatedImage::AllocationParameters{
                    .extent = extent,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                    .usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT
                                | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                    .viewFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                }
            )
        };
        if (!createWorldPositionResult.has_value())
        {
            Error("Failed to create GBuffer worldPosition image.");
            return {};
        }

        buffer.worldPosition = createWorldPositionResult.value();
    }

    {
        VkSamplerCreateInfo const samplerInfo{vkinit::samplerCreateInfo(
            0,
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            VK_FILTER_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        )};

        for (size_t i{0}; i < 4; i++)
        {
            VkSampler sampler{VK_NULL_HANDLE};
            VkResult const samplerResult{
                vkCreateSampler(device, &samplerInfo, nullptr, &sampler)
            };
            if (samplerResult != VK_SUCCESS)
            {
                Error(fmt::format(
                    "Failed to create GBuffer immutable sampler {}", i
                ));
                return {};
            }
            buffer.immutableSamplers.push_back(sampler);
        }
    }
    VkSampler const diffuseColorSampler{buffer.immutableSamplers[0]};
    VkSampler const specularColorSampler{buffer.immutableSamplers[1]};
    VkSampler const normalSampler{buffer.immutableSamplers[2]};
    VkSampler const worldPositionSampler{buffer.immutableSamplers[3]};

    // The descriptor for accessing all the samplers in the lighting passes
    VkDescriptorSetLayout const descriptorLayout{
        DescriptorLayoutBuilder{}
            .addBinding(
                DescriptorLayoutBuilder::AddBindingParameters{
                    .binding = 0,
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                    .bindingFlags = 0,
                },
                {diffuseColorSampler}
            )
            .addBinding(
                DescriptorLayoutBuilder::AddBindingParameters{
                    .binding = 1,
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                    .bindingFlags = 0,
                },
                {specularColorSampler}
            )
            .addBinding(
                DescriptorLayoutBuilder::AddBindingParameters{
                    .binding = 2,
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                    .bindingFlags = 0,
                },
                {normalSampler}
            )
            .addBinding(
                DescriptorLayoutBuilder::AddBindingParameters{
                    .binding = 3,
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                    .bindingFlags = 0,
                },
                {worldPositionSampler}
            )
            .build(device, 0)
            .value_or(VK_NULL_HANDLE)
    };
    if (descriptorLayout == VK_NULL_HANDLE)
    {
        Error("Failed to create GBuffer descriptor set layout.");
        return {};
    }

    VkDescriptorSet const descriptorSet{
        descriptorAllocator.allocate(device, descriptorLayout)
    };

    std::vector<VkDescriptorImageInfo> const imageInfos{
        VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = buffer.diffuseColor.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = buffer.specularColor.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = buffer.normal.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = buffer.worldPosition.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        }
    };

    VkWriteDescriptorSet const descriptorWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

        .pImageInfo = imageInfos.data(),
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    std::vector<VkWriteDescriptorSet> const writes{descriptorWrite};
    vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);

    buffer.descriptors = descriptorSet;
    buffer.descriptorLayout = descriptorLayout;

    return buffer;
}

void GBuffer::recordTransitionImages(
    VkCommandBuffer const cmd,
    VkImageLayout const srcLayout,
    VkImageLayout const dstLayout
) const
{
    vkutil::transitionImage(
        cmd, diffuseColor.image, srcLayout, dstLayout, VK_IMAGE_ASPECT_COLOR_BIT
    );
    vkutil::transitionImage(
        cmd,
        specularColor.image,
        srcLayout,
        dstLayout,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    vkutil::transitionImage(
        cmd, normal.image, srcLayout, dstLayout, VK_IMAGE_ASPECT_COLOR_BIT
    );
    vkutil::transitionImage(
        cmd,
        worldPosition.image,
        srcLayout,
        dstLayout,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
}

void GBuffer::cleanup(VkDevice const device, VmaAllocator const allocator)
{
    diffuseColor.cleanup(device, allocator);
    specularColor.cleanup(device, allocator);
    normal.cleanup(device, allocator);
    worldPosition.cleanup(device, allocator);

    for (VkSampler const sampler : immutableSamplers)
    {
        vkDestroySampler(device, sampler, nullptr);
    }

    vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
}
