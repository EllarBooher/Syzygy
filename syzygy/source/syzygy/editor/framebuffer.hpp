#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include <optional>
#include <vector>

namespace syzygy
{
struct Frame
{
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer mainCommandBuffer{VK_NULL_HANDLE};

    // The semaphore that the swapchain signals when its
    // image is ready to be written to.
    VkSemaphore swapchainSemaphore{VK_NULL_HANDLE};

    // The semaphore that the swapchain waits on before presenting.
    VkSemaphore renderSemaphore{VK_NULL_HANDLE};

    // The fence that the CPU waits on to ensure the frame is not in use.
    VkFence renderFence{VK_NULL_HANDLE};

    void destroy(VkDevice);
};

struct FrameBuffer
{
public:
    auto operator=(FrameBuffer&&) -> FrameBuffer& = delete;
    FrameBuffer(FrameBuffer const&) = delete;
    auto operator=(FrameBuffer const&) -> FrameBuffer& = delete;

    FrameBuffer(FrameBuffer&&) noexcept;
    ~FrameBuffer();

private:
    FrameBuffer() = default;
    void destroy();

public:
    // QueueFamilyIndex should be capable of graphics/compute/transfer/present.
    static auto create(VkDevice, uint32_t queueFamilyIndex)
        -> std::optional<FrameBuffer>;

    [[nodiscard]] auto currentFrame() const -> Frame const&;
    [[nodiscard]] auto frameNumber() const -> size_t;

    void increment();

private:
    VkDevice m_device{VK_NULL_HANDLE};
    std::vector<Frame> m_frames{};
    size_t m_frameNumber{0};
};
} // namespace syzygy