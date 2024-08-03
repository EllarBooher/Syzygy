#include "framebuffer.hpp"

#include "syzygy/core/deletionqueue.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/initializers.hpp"
#include <functional>

namespace
{
auto createFrame(VkDevice const device, uint32_t const queueFamilyIndex)
    -> std::optional<Frame>
{
    std::optional<Frame> frameResult{std::in_place};
    Frame& frame{frameResult.value()};

    DeletionQueue cleanupCallbacks{};
    cleanupCallbacks.pushFunction([&]() { frame.destroy(device); });

    VkCommandPoolCreateInfo const commandPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };

    if (VkResult const result{vkCreateCommandPool(
            device, &commandPoolInfo, nullptr, &frame.commandPool
        )};
        result != VK_SUCCESS)
    {
        LogVkResult(result, "Failed to allocate frame command pool.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }

    VkCommandBufferAllocateInfo const cmdAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = frame.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    if (VkResult const result{vkAllocateCommandBuffers(
            device, &cmdAllocInfo, &frame.mainCommandBuffer
        )};
        result != VK_SUCCESS)
    {
        LogVkResult(result, "Failed to allocate frame command buffer.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }

    // Frames start signaled so they can be initially used
    VkFenceCreateInfo const fenceCreateInfo{
        vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT)
    };

    if (VkResult const result{
            vkCreateFence(device, &fenceCreateInfo, nullptr, &frame.renderFence)
        };
        result != VK_SUCCESS)
    {
        LogVkResult(result, "Failed to allocate frame in-use fence.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }

    VkSemaphoreCreateInfo const semaphoreCreateInfo{vkinit::semaphoreCreateInfo(
    )};

    if (VkResult const result{vkCreateSemaphore(
            device, &semaphoreCreateInfo, nullptr, &frame.swapchainSemaphore
        )};
        result != VK_SUCCESS)
    {
        LogVkResult(result, "Failed to allocate frame swapchain semaphore.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }

    if (VkResult const result{vkCreateSemaphore(
            device, &semaphoreCreateInfo, nullptr, &frame.renderSemaphore
        )};
        result != VK_SUCCESS)
    {
        LogVkResult(result, "Failed to allocate frame render semaphore.");
        cleanupCallbacks.flush();
        return std::nullopt;
    }

    cleanupCallbacks.clear();
    return frameResult;
}
} // namespace

void Frame::destroy(VkDevice const device)
{
    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyFence(device, renderFence, nullptr);
    vkDestroySemaphore(device, renderSemaphore, nullptr);
    vkDestroySemaphore(device, swapchainSemaphore, nullptr);

    *this = Frame{};
}

FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
{
    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_frames = std::move(other.m_frames);
    m_frameNumber = std::exchange(other.m_frameNumber, 0);
}

FrameBuffer::~FrameBuffer() { destroy(); }

auto FrameBuffer::create(VkDevice const device, uint32_t const queueFamilyIndex)
    -> std::optional<FrameBuffer>
{
    if (device == VK_NULL_HANDLE)
    {
        Error("Device is null.");
        return std::nullopt;
    }

    std::optional<FrameBuffer> frameBufferResult{std::in_place, FrameBuffer{}};
    FrameBuffer& frameBuffer{frameBufferResult.value()};
    frameBuffer.m_device = device;

    size_t constexpr FRAMES_IN_FLIGHT{2};

    for (size_t i{0}; i < FRAMES_IN_FLIGHT; i++)
    {
        std::optional<Frame> const frameResult{
            createFrame(device, queueFamilyIndex)
        };
        if (!frameResult.has_value())
        {
            Error("Failed to allocate frame for framebuffer.");
            return std::nullopt;
        }
        frameBuffer.m_frames.push_back(frameResult.value());
    }

    return frameBufferResult;
}

auto FrameBuffer::currentFrame() const -> Frame const&
{
    size_t const index{m_frameNumber % m_frames.size()};
    return m_frames[index];
}

auto FrameBuffer::frameNumber() const -> size_t { return m_frameNumber; }

void FrameBuffer::increment() { m_frameNumber += 1; }

void FrameBuffer::destroy()
{
    if (m_device == VK_NULL_HANDLE)
    {
        if (!m_frames.empty())
        {
            Warning("FrameBuffer destroyed with no device, but allocated "
                    "frames. Memory was maybe leaked.");
        }
        return;
    }

    for (Frame& frame : m_frames)
    {
        frame.destroy(m_device);
    }

    m_device = VK_NULL_HANDLE;
    m_frames.clear();
    m_frameNumber = 0;
}