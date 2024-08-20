#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include <memory>
#include <optional>

namespace syzygy
{
struct PlatformWindow;
} // namespace syzygy

namespace syzygy
{
// Holds the fundamental Vulkan resources.
struct GraphicsContext
{
public:
    auto operator=(GraphicsContext&&) -> GraphicsContext& = delete;
    GraphicsContext(GraphicsContext const&) = delete;
    auto operator=(GraphicsContext const&) -> GraphicsContext& = delete;

    GraphicsContext(GraphicsContext&&) noexcept;
    ~GraphicsContext();

    static auto create(PlatformWindow const&) -> std::optional<GraphicsContext>;

    auto instance() -> VkInstance;
    auto surface() -> VkSurfaceKHR;
    auto physicalDevice() -> VkPhysicalDevice;
    auto device() -> VkDevice;
    auto universalQueue() -> VkQueue;
    [[nodiscard]] auto universalQueueFamily() const -> uint32_t;

    auto allocator() -> VmaAllocator;
    auto descriptorAllocator() -> syzygy::DescriptorAllocator&;

private:
    GraphicsContext() = default;
    void destroy();

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};

    VkQueue m_universalQueue{VK_NULL_HANDLE};
    uint32_t m_universalQueueFamily{};

    VmaAllocator m_allocator{VK_NULL_HANDLE};
    std::unique_ptr<syzygy::DescriptorAllocator> m_descriptorAllocator{};
};
} // namespace syzygy