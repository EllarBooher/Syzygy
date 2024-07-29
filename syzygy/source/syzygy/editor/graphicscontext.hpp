#pragma once

#include "syzygy/core/integer.hpp"
#include "syzygy/descriptors.hpp"
#include "syzygy/vulkanusage.hpp"
#include <memory>
#include <optional>
#include <utility>

struct GLFWwindow;
struct PlatformWindow;

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
        destroy();

        m_vulkan = std::exchange(other.m_vulkan, VulkanContext{});
        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        m_descriptorAllocator = std::move(other.m_descriptorAllocator);

        return *this;
    }

    ~GraphicsContext() noexcept { destroy(); }

    GraphicsContext(GraphicsContext const& other) = delete;
    GraphicsContext& operator=(GraphicsContext const& other) = delete;

    static auto create(PlatformWindow const& window)
        -> std::optional<GraphicsContext>;

    auto vulkanContext() const -> VulkanContext const&;
    auto allocator() const -> VmaAllocator const&;
    auto descriptorAllocator() -> DescriptorAllocator&;

private:
    void destroy();

    explicit GraphicsContext(
        VulkanContext const& vulkan,
        VmaAllocator allocator,
        DescriptorAllocator&& descriptorAllocator
    )
        : m_vulkan(vulkan)
        , m_allocator(allocator)
        , m_descriptorAllocator{std::make_unique<DescriptorAllocator>(
              std::move(descriptorAllocator)
          )}
    {
    }

    VulkanContext m_vulkan{};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator{};
};