#pragma once

#include <vector>

#include "../core/result.hpp"
#include "../vulkanusage.hpp"

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
};

class FrameBuffer
{
public:
    FrameBuffer() = default;

private:
    explicit FrameBuffer(size_t framesInFlight)
        : m_frames{framesInFlight}
    {
    }

    std::vector<Frame> m_frames{};
    size_t m_frameNumber{0};

public:
    // QueueFamilyIndex should be capable of graphics/compute/transfer/present.
    static auto create(VkDevice, uint32_t const queueFamilyIndex)
        -> VulkanResult<FrameBuffer>;

    auto currentFrame() -> Frame&;
    auto frameNumber() const -> size_t;
    void increment();
    void destroy(VkDevice device);
};