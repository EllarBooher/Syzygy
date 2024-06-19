#include "framebuffer.hpp"

#include "../core/deletionqueue.hpp"
#include "../helpers.hpp"
#include "../initializers.hpp"

namespace
{
auto createFrame(VkDevice const device, uint32_t const queueFamilyIndex)
    -> VulkanResult<Frame>
{
    DeletionQueue cleanupCallbacks{};

    VkSemaphoreCreateInfo const semaphoreCreateInfo{vkinit::semaphoreCreateInfo(
    )};

    Frame frame{};

    {
        VkCommandPoolCreateInfo const commandPoolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndex,
        };

        VkResult const commandPoolResult{vkCreateCommandPool(
            device, &commandPoolInfo, nullptr, &frame.commandPool
        )};
        if (VK_SUCCESS != commandPoolResult)
        {
            LogVkResult(
                commandPoolResult, "Failed to allocate frame command buffer."
            );
            cleanupCallbacks.flush();
            return commandPoolResult;
        }
        cleanupCallbacks.pushFunction([&]
        { vkDestroyCommandPool(device, frame.commandPool, nullptr); });
    }

    {
        VkCommandBufferAllocateInfo const cmdAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = frame.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VkResult const commandBufferResult{vkAllocateCommandBuffers(
            device, &cmdAllocInfo, &frame.mainCommandBuffer
        )};
        if (VK_SUCCESS != commandBufferResult)
        {
            LogVkResult(
                commandBufferResult, "Failed to allocate frame command buffer."
            );
            cleanupCallbacks.flush();
            return commandBufferResult;
        }
    }

    {
        // Frames start signaled so they can be initially used
        VkFenceCreateInfo const fenceCreateInfo{
            vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT)
        };

        VkResult const fenceResult{
            vkCreateFence(device, &fenceCreateInfo, nullptr, &frame.renderFence)
        };
        if (VK_SUCCESS != fenceResult)
        {
            LogVkResult(fenceResult, "Failed to allocate frame in-use fence.");
            cleanupCallbacks.flush();
            return fenceResult;
        }
        cleanupCallbacks.pushFunction([&]
        { vkDestroyFence(device, frame.renderFence, nullptr); });
    }

    {

        VkResult const swapchainSemaphoreResult{vkCreateSemaphore(
            device, &semaphoreCreateInfo, nullptr, &frame.swapchainSemaphore
        )};
        if (VK_SUCCESS != swapchainSemaphoreResult)
        {
            LogVkResult(
                swapchainSemaphoreResult,
                "Failed to allocate frame swapchain semaphore."
            );
            cleanupCallbacks.flush();
            return swapchainSemaphoreResult;
        }
        cleanupCallbacks.pushFunction([&]
        { vkDestroySemaphore(device, frame.swapchainSemaphore, nullptr); });
    }

    {
        VkResult const renderSemaphoreResult{vkCreateSemaphore(
            device, &semaphoreCreateInfo, nullptr, &frame.renderSemaphore
        )};
        if (VK_SUCCESS != renderSemaphoreResult)
        {
            LogVkResult(
                renderSemaphoreResult,
                "Failed to allocate frame render semaphore."
            );
            cleanupCallbacks.flush();
            return renderSemaphoreResult;
        }
        cleanupCallbacks.pushFunction([&]
        { vkDestroySemaphore(device, frame.renderSemaphore, nullptr); });
    }

    return {frame, VK_SUCCESS};
}
} // namespace

auto FrameBuffer::create(VkDevice const device, uint32_t const queueFamilyIndex)
    -> VulkanResult<FrameBuffer>
{
    DeletionQueue cleanupCallbacks{};

    size_t constexpr FRAMES_IN_FLIGHT{2};
    std::vector<Frame> frames{};

    // TODO: investigate how to get clang-format to do lambdas nicer, with this
    // as an example
    cleanupCallbacks.pushFunction(
        [&]
    {
        for (Frame& frame : frames)
        {
            frame.destroy(device);
        }
    }
    );

    for (size_t i{0}; i < FRAMES_IN_FLIGHT; i++)
    {
        VulkanResult<Frame> const frameResult{
            createFrame(device, queueFamilyIndex)
        };
        if (!frameResult.has_value())
        {
            VkResult const result{frameResult.vk_result()};
            LogVkResult(result, "Failed to allocate frame for framebuffer.");
            cleanupCallbacks.flush();
            return result;
        }
        frames.push_back(frameResult.value());
    }

    return {FrameBuffer{std::move(frames)}, VK_SUCCESS};
}

auto FrameBuffer::currentFrame() const -> Frame const&
{
    size_t const index{m_frameNumber % m_frames.size()};
    return m_frames[index];
}

auto FrameBuffer::frameNumber() const -> size_t { return m_frameNumber; }

void FrameBuffer::increment() { m_frameNumber += 1; }

void FrameBuffer::destroy(VkDevice device)
{
    for (Frame& frame : m_frames)
    {
        frame.destroy(device);
    }

    *this = FrameBuffer{};
}

void Frame::destroy(VkDevice const device)
{
    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyFence(device, renderFence, nullptr);
    vkDestroySemaphore(device, renderSemaphore, nullptr);
    vkDestroySemaphore(device, swapchainSemaphore, nullptr);

    *this = Frame{};
}
