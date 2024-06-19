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

// Holds the fundamental Vulkan resources.
struct GraphicsContext
{
public:
    GraphicsContext() = default;

    GraphicsContext(GraphicsContext&& other) { *this = std::move(other); }

    GraphicsContext& operator=(GraphicsContext&& other)
    {
        m_vulkan = std::exchange(other.m_vulkan, VulkanContext{});
        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);

        return *this;
    }

    GraphicsContext(GraphicsContext const& other) = delete;
    GraphicsContext& operator=(GraphicsContext const& other) = delete;

    static auto create(PlatformWindow const& window)
        -> std::optional<GraphicsContext>;

    void destroy();

    auto vulkanContext() const -> VulkanContext const&;
    auto allocator() const -> VmaAllocator const&;

private:
    explicit GraphicsContext(
        VulkanContext const& vulkan, VmaAllocator allocator
    )
        : m_vulkan(vulkan)
        , m_allocator(allocator)
    {
    }

    VulkanContext m_vulkan{};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
};