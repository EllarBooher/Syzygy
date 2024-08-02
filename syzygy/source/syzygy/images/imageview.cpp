#include "imageview.hpp"

#include "syzygy/helpers.hpp"
#include "syzygy/images/image.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <utility>

szg_image::ImageView::ImageView(ImageView&& other) noexcept
{
    m_image = std::move(other.m_image);
    m_memory = std::exchange(other.m_memory, ImageViewMemory{});
}

szg_image::ImageView::~ImageView() { destroy(); }

auto szg_image::ImageView::allocate(
    VkDevice const device,
    VmaAllocator const allocator,
    ImageAllocationParameters const& imageParameters,
    ImageViewAllocationParameters const& viewParameters
) -> std::optional<std::unique_ptr<ImageView>>
{
    if (device == VK_NULL_HANDLE || allocator == VK_NULL_HANDLE)
    {
        Error("Device or allocator were null.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<Image>> imageAllocationResult{
        Image::allocate(device, allocator, imageParameters)
    };
    if (!imageAllocationResult.has_value()
        || imageAllocationResult.value() == nullptr)
    {
        Error("Failed to allocate Image.");
        return std::nullopt;
    }

    ImageView finalView{};
    finalView.m_image = std::move(imageAllocationResult).value();

    Image const& image{*finalView.m_image};

    VkImageViewCreateInfo const imageViewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,

        .flags = viewParameters.flags,

        .image = image.image(),
        .viewType = viewParameters.viewType,
        .format = viewParameters.formatOverride.value_or(image.format()),
        .subresourceRange = viewParameters.subresourceRange,
    };

    VkImageView view;
    TRY_VK(
        vkCreateImageView(device, &imageViewInfo, nullptr, &view),
        "Failed to create VkImageView.",
        std::nullopt
    );

    finalView.m_memory = ImageViewMemory{
        .device = device,
        .viewCreateInfo = imageViewInfo,
        .view = view,
    };

    return std::make_unique<ImageView>(std::move(finalView));
}

auto szg_image::ImageView::view() const -> VkImageView { return m_memory.view; }

auto szg_image::ImageView::image() -> Image& { return *m_image; }

auto szg_image::ImageView::image() const -> Image const& { return *m_image; }

void szg_image::ImageView::recordTransitionBarriered(
    VkCommandBuffer const cmd, VkImageLayout const dst
)
{
    image().recordTransitionBarriered(
        cmd, dst, m_memory.viewCreateInfo.subresourceRange.aspectMask
    );
}

auto szg_image::ImageView::expectedLayout() const -> VkImageLayout
{
    return m_image != nullptr ? m_image->expectedLayout()
                              : VK_IMAGE_LAYOUT_UNDEFINED;
}

void szg_image::ImageView::destroy()
{
    bool leaked{false};
    if (m_memory.view != VK_NULL_HANDLE)
    {
        if (m_memory.device != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_memory.device, m_memory.view, nullptr);
        }
        else
        {
            leaked = true;
        }
    }

    if (leaked)
    {
        Warning(fmt::format(
            "Leak detected in image view. Device: {}. VkImageView: {}.",
            fmt::ptr(m_memory.device),
            fmt::ptr(m_memory.view)
        ));
    }

    m_image.reset();
    m_memory = ImageViewMemory{};
}
