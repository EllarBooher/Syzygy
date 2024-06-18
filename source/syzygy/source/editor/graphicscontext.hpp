#pragma once

#include "../vulkanusage.hpp"

#include <optional>
#include <vector>

#include "window.hpp"

struct VulkanContext
{
    VkInstance instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger{VK_NULL_HANDLE};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};

    VkQueue graphicsQueue{VK_NULL_HANDLE};
    uint32_t graphicsQueueFamily{};

    static auto create(GLFWwindow* window) -> std::optional<VulkanContext>;
    void destroy() const;
};

struct Swapchain
{
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkFormat imageFormat{VK_FORMAT_UNDEFINED};

    std::vector<VkImage> images{};
    std::vector<VkImageView> imageViews{};

    VkExtent2D extent{};

    static auto create(
        glm::u16vec2 extent,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkSurfaceKHR surface
    ) -> std::optional<Swapchain>;

    void destroy(VkDevice device);
};

// Holds the fundamental Vulkan resources.
struct GraphicsContext
{
public:
    explicit GraphicsContext(
        VulkanContext const& vulkan,
        VmaAllocator const& allocator,
        Swapchain const& swapchain
    )
        : m_vulkan(vulkan)
        , m_allocator(allocator)
        , m_swapchain(swapchain)
    {
    }
    GraphicsContext() = default;

private:
    VulkanContext m_vulkan{};

    VmaAllocator m_allocator{VK_NULL_HANDLE};

    Swapchain m_swapchain{};

public:
    static auto create(PlatformWindow const& window)
        -> std::optional<GraphicsContext>;

    auto vulkanContext() -> VulkanContext& { return m_vulkan; }

    auto swapchain() -> Swapchain& { return m_swapchain; }

    auto allocator() -> VmaAllocator& { return m_allocator; }

    void destroy();
};