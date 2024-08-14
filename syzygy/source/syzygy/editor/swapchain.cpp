#include "swapchain.hpp"

#include "syzygy/core/integer.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/initializers.hpp"
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
    VkExtent2D const swapchainExtent{.width = width, .height = height};

    VkSwapchainCreateInfoKHR const swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,

        .flags = 0,
        .surface = surface,

        .minImageCount = 3,
        .imageFormat = SWAPCHAIN_IMAGE_FORMAT,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,

        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = 1,
        .oldSwapchain = old.value_or(VK_NULL_HANDLE),
    };

    if (VkResult const swapchainResult{vkCreateSwapchainKHR(
            device, &swapchainCreateInfo, nullptr, &swapchain.m_swapchain
        )})
    {
        SZG_ERROR("Failed to create swapchain.");
        return std::nullopt;
    }

    swapchain.m_imageFormat = SWAPCHAIN_IMAGE_FORMAT;
    swapchain.m_extent = swapchainExtent;

    uint32_t swapchainImageCount{0};
    if (vkGetSwapchainImagesKHR(
            device, swapchain.m_swapchain, &swapchainImageCount, nullptr
        ) != VK_SUCCESS
        || swapchainImageCount == 0)
    {
        SZG_ERROR("Failed to get swapchain images.");
        return std::nullopt;
    }

    swapchain.m_images.resize(swapchainImageCount);
    if (vkGetSwapchainImagesKHR(
            device,
            swapchain.m_swapchain,
            &swapchainImageCount,
            swapchain.m_images.data()
        )
        != VK_SUCCESS)
    {
        SZG_ERROR("Failed to get swapchain images.");
        return std::nullopt;
    }

    for (size_t index{0}; index < swapchain.m_images.size(); index++)
    {
        VkImage const image{swapchain.m_images[index]};

        VkImageViewCreateInfo const viewInfo{vkinit::imageViewCreateInfo(
            swapchain.m_imageFormat, image, VK_IMAGE_ASPECT_COLOR_BIT
        )};

        VkImageView view;
        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS)
        {
            SZG_ERROR("Failed to create swapchain image view.");
            return std::nullopt;
        }
        swapchain.m_imageViews.push_back(view);
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
