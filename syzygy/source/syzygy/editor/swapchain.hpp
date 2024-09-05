#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include <glm/vec2.hpp>
#include <optional>
#include <span>
#include <vector>

namespace syzygy
{
struct Swapchain
{
public:
    Swapchain(Swapchain const&) = delete;
    auto operator=(Swapchain const&) -> Swapchain& = delete;

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

    [[nodiscard]] auto swapchain() const -> VkSwapchainKHR;
    [[nodiscard]] auto images() const -> std::span<VkImage const>;
    [[nodiscard]] auto imageViews() const -> std::span<VkImageView const>;
    [[nodiscard]] auto extent() const -> VkExtent2D;

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_imageFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent{};

    std::vector<VkImage> m_images{};
    std::vector<VkImageView> m_imageViews{};
};
} // namespace syzygy