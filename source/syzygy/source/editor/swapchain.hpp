#pragma once

#include "../vulkanusage.hpp"
#include <glm/vec2.hpp>
#include <optional>
#include <span>
#include <vector>

struct Swapchain
{
public:
    Swapchain() = default;

    Swapchain(Swapchain&& other) { *this = std::move(other); }

    Swapchain& operator=(Swapchain&& other)
    {
        m_swapchain = std::exchange(other.m_swapchain, VK_NULL_HANDLE);
        m_imageFormat = std::exchange(other.m_imageFormat, VK_FORMAT_UNDEFINED);
        m_images = std::move(other.m_images);
        m_imageViews = std::move(other.m_imageViews);
        m_extent = std::exchange(other.m_extent, VkExtent2D{});

        return *this;
    }

    Swapchain(Swapchain const& other) = delete;
    Swapchain& operator=(Swapchain const& other) = delete;

    // TODO: return VkResult from swapchain creation
    static auto create(
        glm::u16vec2 extent,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkSurfaceKHR surface,
        std::optional<VkSwapchainKHR> old
    ) -> std::optional<Swapchain>;

    void destroy(VkDevice device);

    auto swapchain() const -> VkSwapchainKHR;
    auto images() const -> std::span<VkImage const>;
    auto imageViews() const -> std::span<VkImageView const>;
    auto extent() const -> VkExtent2D;

private:
    explicit Swapchain(
        VkSwapchainKHR swapchain,
        VkFormat format,
        std::vector<VkImage>&& images,
        std::vector<VkImageView>&& imageViews,
        VkExtent2D extent
    )
        : m_swapchain(swapchain)
        , m_imageFormat(format)
        , m_images(std::move(images))
        , m_imageViews(std::move(imageViews))
        , m_extent(extent)
    {
    }

    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_imageFormat{VK_FORMAT_UNDEFINED};

    std::vector<VkImage> m_images{};
    std::vector<VkImageView> m_imageViews{};

    VkExtent2D m_extent{};
};