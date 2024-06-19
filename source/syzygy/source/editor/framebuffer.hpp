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

    void destroy(VkDevice);
};

class FrameBuffer
{
public:
    FrameBuffer() = default;

    FrameBuffer(FrameBuffer&& other) { *this = std::move(other); }

    FrameBuffer& operator=(FrameBuffer&& other)
    {
        m_frames = std::move(other.m_frames);
        m_frameNumber = std::exchange(other.m_frameNumber, 0);

        return *this;
    }

    FrameBuffer(FrameBuffer const& other) = delete;
    FrameBuffer& operator=(FrameBuffer const& other) = delete;

    // QueueFamilyIndex should be capable of graphics/compute/transfer/present.
    static auto create(VkDevice, uint32_t const queueFamilyIndex)
        -> VulkanResult<FrameBuffer>;

    void destroy(VkDevice device);

    auto currentFrame() const -> Frame const&;
    auto frameNumber() const -> size_t;

    void increment();

private:
    explicit FrameBuffer(std::vector<Frame>&& frames)
        : m_frames{std::move(frames)}
    {
    }

    std::vector<Frame> m_frames{};
    size_t m_frameNumber{0};
};