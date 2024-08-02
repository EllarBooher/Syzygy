#pragma once

#include "syzygy/core/integer.hpp"
#include "syzygy/vulkanusage.hpp"
#include <optional>
#include <utility>
#include <vector>

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

class FrameBuffer
{
public:
    FrameBuffer& operator=(FrameBuffer&&) = delete;
    FrameBuffer(FrameBuffer const&) = delete;
    FrameBuffer& operator=(FrameBuffer const&) = delete;

    FrameBuffer(FrameBuffer&&) noexcept;
    ~FrameBuffer();

private:
    FrameBuffer() = default;
    void destroy();

public:
    // QueueFamilyIndex should be capable of graphics/compute/transfer/present.
    static auto create(VkDevice, uint32_t const queueFamilyIndex)
        -> std::optional<FrameBuffer>;

    auto currentFrame() const -> Frame const&;
    auto frameNumber() const -> size_t;

    void increment();

private:
    VkDevice m_device{VK_NULL_HANDLE};
    std::vector<Frame> m_frames{};
    size_t m_frameNumber{0};
};