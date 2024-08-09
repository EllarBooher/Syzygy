#include "swapchain.hpp"

#include "syzygy/core/integer.hpp"
#include "syzygy/helpers.hpp"
#include <VkBootstrap.h>
#include <fmt/core.h>
#include <utility>

auto Swapchain::operator=(Swapchain&& other) noexcept -> Swapchain&
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_swapchain = std::exchange(other.m_swapchain, VK_NULL_HANDLE);
    m_imageFormat = std::exchange(other.m_imageFormat, VK_FORMAT_UNDEFINED);
    m_images = std::move(other.m_images);
    m_imageViews = std::move(other.m_imageViews);
    m_extent = std::exchange(other.m_extent, VkExtent2D{});

    return *this;
}
Swapchain::Swapchain(Swapchain&& other) noexcept { *this = std::move(other); }

Swapchain::~Swapchain() { destroy(); }

void Swapchain::destroy()
{
    if (m_device == VK_NULL_HANDLE)
    {
        if (m_swapchain != VK_NULL_HANDLE || !m_imageViews.empty())
        {
            SZG_WARNING(
                "Swapchain had allocations, but device was null. Memory "
                "was possibly leaked."
            );
        }
        return;
    }

    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    for (VkImageView const view : m_imageViews)
    {
        vkDestroyImageView(m_device, view, nullptr);
    }
}

auto Swapchain::create(
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VkSurfaceKHR const surface,
    glm::u16vec2 const extent,
    std::optional<VkSwapchainKHR> const old
) -> std::optional<Swapchain>
{
    if (physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE
        || surface == VK_NULL_HANDLE)
    {
        SZG_ERROR("One or more necessary handles were null.");
        return std::nullopt;
    }

    std::optional<Swapchain> swapchainResult{std::in_place, Swapchain{}};
    Swapchain& swapchain{swapchainResult.value()};
    swapchain.m_device = device;

    VkFormat constexpr SWAPCHAIN_IMAGE_FORMAT{VK_FORMAT_B8G8R8A8_UNORM};

    VkSurfaceFormatKHR const surfaceFormat{
        .format = SWAPCHAIN_IMAGE_FORMAT,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    uint32_t const width{extent.x};
    uint32_t const height{extent.y};

    vkb::Result<vkb::Swapchain> vkbResult{
        vkb::SwapchainBuilder{physicalDevice, device, surface}
            .set_desired_format(surfaceFormat)
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_old_swapchain(old.value_or(VK_NULL_HANDLE))
            .build()
    };
    if (!vkbResult.has_value())
    {
        SZG_LOG_VKB(vkbResult, "Failed to build VkbSwapchain.");
        return std::nullopt;
    }
    vkb::Swapchain& vkbSwapchain{vkbResult.value()};

    swapchain.m_swapchain = vkbSwapchain.swapchain;
    swapchain.m_imageFormat = vkbSwapchain.image_format;
    swapchain.m_extent = vkbSwapchain.extent;

    if (vkb::Result<std::vector<VkImage>> imagesResult{vkbSwapchain.get_images()
        };
        imagesResult.has_value())
    {
        swapchain.m_images = std::move(imagesResult).value();
    }
    else
    {
        SZG_LOG_VKB(imagesResult, "Failed to get swapchain images.");
        return std::nullopt;
    }

    if (vkb::Result<std::vector<VkImageView>> viewsResult{
            vkbSwapchain.get_image_views()
        };
        viewsResult.has_value())
    {
        swapchain.m_imageViews = std::move(viewsResult).value();
    }
    else
    {
        SZG_LOG_VKB(viewsResult, "Failed to get swapchain image views.");
        return std::nullopt;
    }

    return swapchainResult;
}

auto Swapchain::swapchain() const -> VkSwapchainKHR { return m_swapchain; }

auto Swapchain::images() const -> std::span<VkImage const> { return m_images; }

auto Swapchain::imageViews() const -> std::span<VkImageView const>
{
    return m_imageViews;
}

auto Swapchain::extent() const -> VkExtent2D { return m_extent; }
