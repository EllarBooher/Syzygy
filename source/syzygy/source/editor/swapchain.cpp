#include "swapchain.hpp"

#include "../core/deletionqueue.hpp"
#include "../helpers.hpp"

#include <VkBootstrap.h>

auto Swapchain::create(
    glm::u16vec2 const extent,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VkSurfaceKHR const surface,
    std::optional<VkSwapchainKHR> const old
) -> std::optional<Swapchain>
{
    DeletionQueue cleanupCallbacks{};

    VkFormat constexpr SWAPCHAIN_IMAGE_FORMAT{VK_FORMAT_B8G8R8A8_UNORM};

    VkSurfaceFormatKHR const surfaceFormat{
        .format = SWAPCHAIN_IMAGE_FORMAT,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    uint32_t const width{extent.x};
    uint32_t const height{extent.y};

    vkb::Result<vkb::Swapchain> const swapchainResult{
        vkb::SwapchainBuilder{physicalDevice, device, surface}
            .set_desired_format(surfaceFormat)
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_old_swapchain(old.value_or(VK_NULL_HANDLE))
            .build()
    };
    if (!swapchainResult.has_value())
    {
        LogVkbError(swapchainResult, "Failed to build VkbSwapchain.");
        return std::nullopt;
    }
    vkb::Swapchain swapchain{swapchainResult.value()};
    cleanupCallbacks.pushFunction([&]() { vkb::destroy_swapchain(swapchain); });

    vkb::Result<std::vector<VkImage>> imagesResult{swapchain.get_images()};
    if (!imagesResult.has_value())
    {
        LogVkbError(imagesResult, "Failed to get swapchain images.");
        return std::nullopt;
    }

    vkb::Result<std::vector<VkImageView>> viewsResult{swapchain.get_image_views(
    )};
    if (!viewsResult.has_value())
    {
        LogVkbError(viewsResult, "Failed to get swapchain image views.");
        return std::nullopt;
    }
    cleanupCallbacks.pushFunction([&]()
    { swapchain.destroy_image_views(viewsResult.value()); });

    cleanupCallbacks.clear();

    return Swapchain{
        device,
        swapchain.swapchain,
        SWAPCHAIN_IMAGE_FORMAT,
        std::move(imagesResult).value(),
        std::move(viewsResult).value(),
        swapchain.extent,
    };
}

auto Swapchain::swapchain() const -> VkSwapchainKHR { return m_swapchain; }

auto Swapchain::images() const -> std::span<VkImage const> { return m_images; }

auto Swapchain::imageViews() const -> std::span<VkImageView const>
{
    return m_imageViews;
}

auto Swapchain::extent() const -> VkExtent2D { return m_extent; }

void Swapchain::destroy()
{
    if (m_swapchain == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_device == VK_NULL_HANDLE)
    {
        Warning("Device was null when trying to destroy swapchain.");
        return;
    }

    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    for (VkImageView const view : m_imageViews)
    {
        vkDestroyImageView(m_device, view, nullptr);
    }
}
