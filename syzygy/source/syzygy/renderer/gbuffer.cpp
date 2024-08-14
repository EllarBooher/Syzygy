#include "gbuffer.hpp"

#include "syzygy/core/deletionqueue.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include <array>
#include <functional>
#include <spdlog/fmt/bundled/core.h>
#include <utility>

auto GBuffer::create(
    VkDevice const device,
    VkExtent2D const drawExtent,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator
) -> std::optional<GBuffer>
{
    DeletionQueue cleanupCallbacks{};

    szg_renderer::ImageAllocationParameters const imageParameters{
        .extent = drawExtent,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .usageFlags =
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    };
    szg_renderer::ImageAllocationParameters const worldPositionParameters{
        .extent = drawExtent,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .usageFlags =
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    };
    szg_renderer::ImageViewAllocationParameters const viewParameters{
        .subresourceRange =
            szg_renderer::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
    };

    std::optional<std::unique_ptr<szg_renderer::ImageView>> diffuseResult{
        szg_renderer::ImageView::allocate(
            device, allocator, imageParameters, viewParameters
        )
    };
    if (!diffuseResult.has_value() || diffuseResult.value() == nullptr)
    {
        SZG_ERROR("Failed to create GBuffer diffuse color image.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<szg_renderer::ImageView>> specularResult{
        szg_renderer::ImageView::allocate(
            device, allocator, imageParameters, viewParameters
        )
    };
    if (!specularResult.has_value() || specularResult.value() == nullptr)
    {
        SZG_ERROR("Failed to create GBuffer specular color image.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<szg_renderer::ImageView>> normalResult{
        szg_renderer::ImageView::allocate(
            device, allocator, imageParameters, viewParameters
        )
    };
    if (!normalResult.has_value() || normalResult.value() == nullptr)
    {
        SZG_ERROR("Failed to create GBuffer normal image.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<szg_renderer::ImageView>> positionResult{
        szg_renderer::ImageView::allocate(
            device, allocator, worldPositionParameters, viewParameters
        )
    };
    if (!positionResult.has_value() || positionResult.value() == nullptr)
    {
        SZG_ERROR("Failed to create GBuffer worldPosition image.");
        return std::nullopt;
    }

    std::array<VkSampler, 4> immutableSamplers{};
    cleanupCallbacks.pushFunction(
        [&]()
    {
        for (VkSampler const sampler : immutableSamplers)
        {
            vkDestroySampler(device, sampler, nullptr);
        }
    }
    );

    {
        VkSamplerCreateInfo const samplerInfo{szg_renderer::samplerCreateInfo(
            0,
            VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
            VK_FILTER_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        )};

        for (size_t i{0}; i < 4; i++)
        {
            VkSampler sampler{VK_NULL_HANDLE};
            VkResult const samplerResult{vkCreateSampler(
                device, &samplerInfo, nullptr, &immutableSamplers[i]
            )};
            if (samplerResult != VK_SUCCESS)
            {
                SZG_ERROR(fmt::format(
                    "Failed to create GBuffer immutable sampler {}", i
                ));
                cleanupCallbacks.flush();
                return std::nullopt;
            }
        }
    }
    VkSampler const diffuseColorSampler{immutableSamplers[0]};
    VkSampler const specularColorSampler{immutableSamplers[1]};
    VkSampler const normalSampler{immutableSamplers[2]};
    VkSampler const positionSampler{immutableSamplers[3]};

    // The descriptor for accessing all the samplers in the lighting passes
    std::optional<VkDescriptorSetLayout> const descriptorLayoutResult{
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
                {positionSampler}
            )
            .build(device, 0)
            .value_or(VK_NULL_HANDLE)
    };
    if (!descriptorLayoutResult.has_value())
    {
        SZG_ERROR("Failed to create GBuffer descriptor set layout.");
        return std::nullopt;
    }

    VkDescriptorSet const descriptorSet{
        descriptorAllocator.allocate(device, descriptorLayoutResult.value())
    };

    std::vector<VkDescriptorImageInfo> const imageInfos{
        VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = diffuseResult.value()->view(),
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = specularResult.value()->view(),
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = normalResult.value()->view(),
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = positionResult.value()->view(),
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

    cleanupCallbacks.clear();

    return GBuffer{
        .diffuseColor = std::move(diffuseResult).value(),
        .specularColor = std::move(specularResult).value(),
        .normal = std::move(normalResult).value(),
        .worldPosition = std::move(positionResult).value(),
        .descriptorLayout = descriptorLayoutResult.value(),
        .descriptors = descriptorSet,
        .immutableSamplers =
            {immutableSamplers.begin(), immutableSamplers.end()},
    };
}

auto GBuffer::extent() const -> VkExtent2D
{
    return diffuseColor != nullptr ? diffuseColor->image().extent2D()
                                   : VkExtent2D{0, 0};
}

void GBuffer::recordTransitionImages(
    VkCommandBuffer const cmd, VkImageLayout const dstLayout
) const
{
    diffuseColor->recordTransitionBarriered(cmd, dstLayout);
    specularColor->recordTransitionBarriered(cmd, dstLayout);
    normal->recordTransitionBarriered(cmd, dstLayout);
    worldPosition->recordTransitionBarriered(cmd, dstLayout);
}

void GBuffer::cleanup(VkDevice const device)
{
    diffuseColor.reset();
    specularColor.reset();
    normal.reset();
    worldPosition.reset();

    for (VkSampler const sampler : immutableSamplers)
    {
        vkDestroySampler(device, sampler, nullptr);
    }

    vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
}
