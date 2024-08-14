#include "scenetexture.hpp"

#include "deletionqueue.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/images/image.hpp"
#include "syzygy/images/imageview.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include <functional>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <vector>

scene::SceneTexture::SceneTexture(SceneTexture&& other) noexcept
{
    *this = std::move(other);
}

auto scene::SceneTexture::operator=(SceneTexture&& other) noexcept
    -> scene::SceneTexture&
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);

    m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    m_texture.swap(other.m_texture);

    m_singletonDescriptorLayout =
        std::exchange(other.m_singletonDescriptorLayout, VK_NULL_HANDLE);
    m_singletonDescriptor =
        std::exchange(other.m_singletonDescriptor, VK_NULL_HANDLE);

    m_imguiDescriptor = std::exchange(other.m_imguiDescriptor, VK_NULL_HANDLE);

    return *this;
}

scene::SceneTexture::~SceneTexture() { destroy(); }

auto scene::SceneTexture::create(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    VkExtent2D const textureMax,
    VkFormat const format
) -> std::optional<SceneTexture>
{
    if (ImGui::GetIO().BackendRendererUserData == nullptr)
    {
        SZG_ERROR("ImGui backend not initialized.");
        return std::nullopt;
    }

    DeletionQueue cleanupCallbacks{};

    VkImageUsageFlags const colorUsage{
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        | VK_IMAGE_USAGE_SAMPLED_BIT // used as descriptor for e.g. ImGui
        | VK_IMAGE_USAGE_STORAGE_BIT // used in compute passes
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT // used in graphics passes
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT     // copy into
    };

    std::optional<std::unique_ptr<szg_image::ImageView>> textureResult{
        szg_image::ImageView::allocate(
            device,
            allocator,
            szg_image::ImageAllocationParameters{
                .extent = textureMax,
                .format = format,
                .usageFlags = colorUsage,
            },
            szg_image::ImageViewAllocationParameters{}
        )
    };

    if (!textureResult.has_value() || textureResult.value() == nullptr)
    {
        SZG_ERROR("Failed to allocate image.");
        return std::nullopt;
    }
    cleanupCallbacks.pushFunction([&]() { textureResult.reset(); });
    szg_image::ImageView& texture{*textureResult.value()};

    VkSamplerCreateInfo const samplerInfo{szg_renderer::samplerCreateInfo(
        0,
        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        VK_FILTER_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
    )};

    VkSampler sampler{VK_NULL_HANDLE};
    SZG_TRY_VK(
        vkCreateSampler(device, &samplerInfo, nullptr, &sampler),
        "Failed to allocate sampler.",
        std::nullopt
    );
    cleanupCallbacks.pushFunction([&]()
    { vkDestroySampler(device, sampler, nullptr); });

    VkDescriptorSet const imguiDescriptor = ImGui_ImplVulkan_AddTexture(
        sampler, texture.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    VkDescriptorSetLayout singletonLayout;
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
        singletonLayout = layoutResult.value();
    }
    else
    {
        SZG_ERROR("Failed to allocate descriptor layout.");
        return std::nullopt;
    }
    cleanupCallbacks.pushFunction([&]()
    { vkDestroyDescriptorSetLayout(device, singletonLayout, nullptr); });

    VkDescriptorSet const singletonSet =
        descriptorAllocator.allocate(device, singletonLayout);

    {
        VkDescriptorImageInfo const sceneTextureInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = texture.view(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkWriteDescriptorSet const sceneTextureWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,

            .dstSet = singletonSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,

            .pImageInfo = &sceneTextureInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        };

        std::vector<VkWriteDescriptorSet> const writes{sceneTextureWrite};

        vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
    }

    cleanupCallbacks.clear();

    return SceneTexture{
        device,
        sampler,
        std::move(textureResult).value(),
        singletonLayout,
        singletonSet,
        imguiDescriptor,
    };
}

auto scene::SceneTexture::sampler() const -> VkSampler { return m_sampler; }

auto scene::SceneTexture::texture() -> szg_image::ImageView&
{
    return *m_texture;
}

auto scene::SceneTexture::texture() const -> szg_image::ImageView const&
{
    return *m_texture;
}

auto scene::SceneTexture::singletonDescriptor() const -> VkDescriptorSet
{
    return m_singletonDescriptor;
}

auto scene::SceneTexture::singletonLayout() const -> VkDescriptorSetLayout
{
    return m_singletonDescriptorLayout;
}

auto scene::SceneTexture::imguiDescriptor() const -> VkDescriptorSet
{
    return m_imguiDescriptor;
}

void scene::SceneTexture::destroy() noexcept
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(
            m_device, m_singletonDescriptorLayout, nullptr
        );
        vkDestroySampler(m_device, m_sampler, nullptr);
    }

    m_singletonDescriptorLayout = VK_NULL_HANDLE;
    m_singletonDescriptor = VK_NULL_HANDLE;

    m_imguiDescriptor = VK_NULL_HANDLE;

    m_texture.reset();

    m_sampler = VK_NULL_HANDLE;

    m_device = VK_NULL_HANDLE;
}
