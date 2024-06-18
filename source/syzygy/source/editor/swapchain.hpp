#pragma once

#include "../vulkanusage.hpp"
#include <glm/vec2.hpp>
#include <optional>
#include <vector>

struct Swapchain
{
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkFormat imageFormat{VK_FORMAT_UNDEFINED};

    std::vector<VkImage> images{};
    std::vector<VkImageView> imageViews{};

    VkExtent2D extent{};

    // TODO: return VkResult from swapchain creation
    static auto create(
        glm::u16vec2 extent,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkSurfaceKHR surface,
        std::optional<VkSwapchainKHR> old
    ) -> std::optional<Swapchain>;

    void destroy(VkDevice device);
};