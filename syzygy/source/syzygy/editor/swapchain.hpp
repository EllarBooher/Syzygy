#pragma once

#include "syzygy/vulkanusage.hpp"
#include <glm/vec2.hpp>
#include <optional>
#include <span>
#include <utility>
#include <vector>

struct Swapchain
{
public:
    Swapchain(Swapchain const&) = delete;
    Swapchain& operator=(Swapchain const&) = delete;

    auto operator=(Swapchain&&) noexcept -> Swapchain&;
    Swapchain(Swapchain&&) noexcept;
    ~Swapchain();

private:
    Swapchain() = default;
    void destroy();

public:
    static auto create(
        VkPhysicalDevice,
        VkDevice,
        VkSurfaceKHR,
        glm::u16vec2 extent,
        std::optional<VkSwapchainKHR> old
    ) -> std::optional<Swapchain>;

    auto swapchain() const -> VkSwapchainKHR;
    auto images() const -> std::span<VkImage const>;
    auto imageViews() const -> std::span<VkImageView const>;
    auto extent() const -> VkExtent2D;

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_imageFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent{};

    std::vector<VkImage> m_images{};
    std::vector<VkImageView> m_imageViews{};
};