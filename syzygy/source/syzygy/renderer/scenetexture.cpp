#include "scenetexture.hpp"

#include "syzygy/core/deletionqueue.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
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

    m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    m_texture.swap(other.m_texture);

    m_singletonDescriptorLayout =
        std::exchange(other.m_singletonDescriptorLayout, VK_NULL_HANDLE);
    m_singletonDescriptor =
        std::exchange(other.m_singletonDescriptor, VK_NULL_HANDLE);

    return *this;
}

SceneTexture::~SceneTexture() { destroy(); }

auto SceneTexture::create(
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

    std::optional<std::unique_ptr<ImageView>> textureResult{ImageView::allocate(
        device,
        allocator,
        ImageAllocationParameters{
            .extent = textureMax,
            .format = format,
            .usageFlags = colorUsage,
        },
        ImageViewAllocationParameters{}
    )};

    if (!textureResult.has_value() || textureResult.value() == nullptr)
    {
        SZG_ERROR("Failed to allocate image.");
        return std::nullopt;
    }
    cleanupCallbacks.pushFunction([&]() { textureResult.reset(); });
    ImageView& texture{*textureResult.value()};

    VkSamplerCreateInfo const samplerInfo{samplerCreateInfo(
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
    };
}

auto SceneTexture::sampler() const -> VkSampler { return m_sampler; }

auto SceneTexture::texture() -> ImageView& { return *m_texture; }

auto SceneTexture::texture() const -> ImageView const& { return *m_texture; }

auto SceneTexture::singletonDescriptor() const -> VkDescriptorSet
{
    return m_singletonDescriptor;
}

auto SceneTexture::singletonLayout() const -> VkDescriptorSetLayout
{
    return m_singletonDescriptorLayout;
}

void SceneTexture::destroy() noexcept
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

    m_texture.reset();

    m_sampler = VK_NULL_HANDLE;

    m_device = VK_NULL_HANDLE;
}
} // namespace syzygy