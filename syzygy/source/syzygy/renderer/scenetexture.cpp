#include "scenetexture.hpp"

#include "syzygy/core/deletionqueue.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include <array>
#include <functional>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <vector>

namespace syzygy
{
SceneTexture::SceneTexture(SceneTexture&& other) noexcept
{
    *this = std::move(other);
}

auto SceneTexture::operator=(SceneTexture&& other) noexcept -> SceneTexture&
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);

    m_descriptorPool = std::move(other.m_descriptorPool);

    m_colorSampler = std::exchange(other.m_colorSampler, VK_NULL_HANDLE);
    m_color = std::move(other.m_color);

    m_depthSampler = std::exchange(other.m_depthSampler, VK_NULL_HANDLE);
    m_depth = std::move(other.m_depth);

    m_singletonDescriptorLayout =
        std::exchange(other.m_singletonDescriptorLayout, VK_NULL_HANDLE);
    m_singletonDescriptor =
        std::exchange(other.m_singletonDescriptor, VK_NULL_HANDLE);

    m_combinedDescriptorLayout =
        std::exchange(other.m_combinedDescriptorLayout, VK_NULL_HANDLE);
    m_combinedDescriptor =
        std::exchange(other.m_combinedDescriptor, VK_NULL_HANDLE);

    return *this;
}

SceneTexture::~SceneTexture() { destroy(); }

auto SceneTexture::create(
    VkDevice const device,
    VmaAllocator const allocator,
    VkExtent2D const textureMax,
    VkFormat const colorFormat,
    VkFormat const depthFormat
) -> std::optional<SceneTexture>
{
    if (ImGui::GetIO().BackendRendererUserData == nullptr)
    {
        SZG_ERROR("ImGui backend not initialized.");
        return std::nullopt;
    }

    std::optional<SceneTexture> result{SceneTexture{}};
    SceneTexture& sceneTexture{result.value()};
    sceneTexture.m_device = device;

    std::array<DescriptorAllocator::PoolSizeRatio, 2> poolRatios{
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        1.0F,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1.0F,
    };
    sceneTexture.m_descriptorPool =
        std::make_unique<DescriptorAllocator>(DescriptorAllocator::create(
            device, 4, poolRatios, static_cast<VkFlags>(0)
        ));

    VkImageUsageFlags const colorUsage{
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT // used as descriptor for e.g. ImGui
        | VK_IMAGE_USAGE_STORAGE_BIT // used in compute passes
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT // used in graphics passes
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT     // copy into
    };

    if (std::optional<std::unique_ptr<ImageView>> colorResult{
            ImageView::allocate(
                device,
                allocator,
                ImageAllocationParameters{
                    .extent = textureMax,
                    .format = colorFormat,
                    .usageFlags = colorUsage,
                },
                ImageViewAllocationParameters{}
            )
        };
        colorResult.has_value() && colorResult.value() != nullptr)
    {
        sceneTexture.m_color = std::move(colorResult).value();

        VkSamplerCreateInfo const samplerInfo{samplerCreateInfo(
            0,
            VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            VK_FILTER_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
        )};

        VkSampler sampler{VK_NULL_HANDLE};
        SZG_TRY_VK(
            vkCreateSampler(
                device, &samplerInfo, nullptr, &sceneTexture.m_colorSampler
            ),
            "Failed to allocate sampler.",
            std::nullopt
        );
    }
    else
    {
        SZG_ERROR("Failed to allocate color image.");
        return std::nullopt;
    }

    VkImageUsageFlags const depthUsage{
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    };

    if (std::optional<std::unique_ptr<ImageView>> depthResult{
            ImageView::allocate(
                device,
                allocator,
                ImageAllocationParameters{
                    .extent = textureMax,
                    .format = depthFormat,
                    .usageFlags = depthUsage,
                },
                ImageViewAllocationParameters{
                    .subresourceRange =
                        syzygy::imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT
                        ),
                }
            )
        };
        depthResult.has_value() && depthResult.value() != nullptr)
    {
        sceneTexture.m_depth = std::move(depthResult).value();

        VkSamplerCreateInfo const samplerInfo{samplerCreateInfo(
            0,
            VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            VK_FILTER_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
        )};

        VkSampler sampler{VK_NULL_HANDLE};
        SZG_TRY_VK(
            vkCreateSampler(
                device, &samplerInfo, nullptr, &sceneTexture.m_depthSampler
            ),
            "Failed to allocate sampler.",
            std::nullopt
        );
    }
    else
    {
        SZG_ERROR("Failed to allocate color image.");
        return std::nullopt;
    }

    ImageView& color{*sceneTexture.m_color};
    ImageView& depth{*sceneTexture.m_depth};

    if (auto const layoutResult{
            DescriptorLayoutBuilder{}
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = 0,
                    },
                    1
                )
                .build(device, 0)
        };
        layoutResult.has_value())
    {
        sceneTexture.m_singletonDescriptorLayout = layoutResult.value();
        sceneTexture.m_singletonDescriptor =
            sceneTexture.m_descriptorPool->allocate(
                device, sceneTexture.m_singletonDescriptorLayout
            );
    }
    else
    {
        SZG_ERROR("Failed to allocate singleton descriptor layout.");
        return std::nullopt;
    }

    if (auto const layoutResult{
            DescriptorLayoutBuilder{}
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = 0,
                    },
                    1
                )
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 1,
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = 0,
                    },
                    {sceneTexture.m_depthSampler}
                )
                .build(device, 0)
        };
        layoutResult.has_value())
    {
        sceneTexture.m_combinedDescriptorLayout = layoutResult.value();
        sceneTexture.m_combinedDescriptor =
            sceneTexture.m_descriptorPool->allocate(
                device, sceneTexture.m_combinedDescriptorLayout
            );
    }
    else
    {
        SZG_ERROR("Failed to allocate combined descriptor layout.");
        return std::nullopt;
    }

    {
        VkDescriptorImageInfo const colorInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = sceneTexture.m_color->view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        VkDescriptorImageInfo const depthInfo{
            .sampler = sceneTexture.m_depthSampler,
            .imageView = sceneTexture.m_depth->view(),
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet const singletonColorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = sceneTexture.m_singletonDescriptor,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,

            .pImageInfo = &colorInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        VkWriteDescriptorSet const combinedColorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = sceneTexture.m_combinedDescriptor,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,

            .pImageInfo = &colorInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        VkWriteDescriptorSet const combinedDepthWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = sceneTexture.m_combinedDescriptor,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,

            .pImageInfo = &colorInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        std::vector<VkWriteDescriptorSet> const writes{
            singletonColorWrite, combinedColorWrite, combinedDepthWrite
        };

        vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
    }

    return result;
}

auto SceneTexture::colorSampler() const -> VkSampler { return m_colorSampler; }

auto SceneTexture::color() -> ImageView& { return *m_color; }

auto SceneTexture::color() const -> ImageView const& { return *m_color; }

auto SceneTexture::depth() -> ImageView& { return *m_depth; }

auto SceneTexture::depth() const -> ImageView const& { return *m_depth; }

auto SceneTexture::singletonDescriptor() const -> VkDescriptorSet
{
    return m_singletonDescriptor;
}

auto SceneTexture::singletonLayout() const -> VkDescriptorSetLayout
{
    return m_singletonDescriptorLayout;
}

auto SceneTexture::combinedDescriptor() const -> VkDescriptorSet
{
    return m_combinedDescriptor;
}

auto SceneTexture::combinedDescriptorLayout() const -> VkDescriptorSetLayout
{
    return m_combinedDescriptorLayout;
}

void SceneTexture::destroy() noexcept
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(
            m_device, m_singletonDescriptorLayout, nullptr
        );
        vkDestroyDescriptorSetLayout(
            m_device, m_combinedDescriptorLayout, nullptr
        );
        vkDestroySampler(m_device, m_colorSampler, nullptr);
        vkDestroySampler(m_device, m_depthSampler, nullptr);
    }

    m_descriptorPool.reset();

    m_singletonDescriptorLayout = VK_NULL_HANDLE;
    m_singletonDescriptor = VK_NULL_HANDLE;

    m_combinedDescriptorLayout = VK_NULL_HANDLE;
    m_combinedDescriptor = VK_NULL_HANDLE;

    m_color.reset();

    m_colorSampler = VK_NULL_HANDLE;

    m_depth.reset();

    m_depthSampler = VK_NULL_HANDLE;

    m_device = VK_NULL_HANDLE;
}
} // namespace syzygy