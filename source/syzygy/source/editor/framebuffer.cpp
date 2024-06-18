#include "framebuffer.hpp"

#include "../core/deletionqueue.hpp"
#include "../helpers.hpp"
#include "../initializers.hpp"

auto FrameBuffer::create(VkDevice const device, uint32_t const queueFamilyIndex)
    -> VulkanResult<FrameBuffer>
{
    DeletionQueue cleanupCallbacks{};

    VkCommandPoolCreateInfo const commandPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };

    // Frames start signaled so they can be initially used
    VkFenceCreateInfo const fenceCreateInfo{
        vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT)
    };
    VkSemaphoreCreateInfo const semaphoreCreateInfo{vkinit::semaphoreCreateInfo(
    )};

    size_t constexpr FRAMES_IN_FLIGHT{2};
    FrameBuffer buffer{FRAMES_IN_FLIGHT};

    for (Frame& frameData : buffer.m_frames)
    {
        {
            VkResult const commandPoolResult{vkCreateCommandPool(
                device, &commandPoolInfo, nullptr, &frameData.commandPool
            )};
            if (VK_SUCCESS != commandPoolResult)
            {
                LogVkResult(
                    commandPoolResult,
                    "Failed to allocate frame command buffer."
                );
                cleanupCallbacks.flush();
                return commandPoolResult;
            }
            cleanupCallbacks.pushFunction([&]
            { vkDestroyCommandPool(device, frameData.commandPool, nullptr); });
        }

        {
            VkCommandBufferAllocateInfo const cmdAllocInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = frameData.commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };

            VkResult const commandBufferResult{vkAllocateCommandBuffers(
                device, &cmdAllocInfo, &frameData.mainCommandBuffer
            )};
            if (VK_SUCCESS != commandBufferResult)
            {
                LogVkResult(
                    commandBufferResult,
                    "Failed to allocate frame command buffer."
                );
                cleanupCallbacks.flush();
                return commandBufferResult;
            }
        }

        {
            VkResult const fenceResult{vkCreateFence(
                device, &fenceCreateInfo, nullptr, &frameData.renderFence
            )};
            if (VK_SUCCESS != fenceResult)
            {
                LogVkResult(
                    fenceResult, "Failed to allocate frame in-use fence."
                );
                cleanupCallbacks.flush();
                return fenceResult;
            }
            cleanupCallbacks.pushFunction([&]
            { vkDestroyFence(device, frameData.renderFence, nullptr); });
        }

        {
            VkResult const swapchainSemaphoreResult{vkCreateSemaphore(
                device,
                &semaphoreCreateInfo,
                nullptr,
                &frameData.swapchainSemaphore
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
            cleanupCallbacks.pushFunction(
                [&] {
                vkDestroySemaphore(
                    device, frameData.swapchainSemaphore, nullptr
                );
            }
            );
        }

        {
            VkResult const renderSemaphoreResult{vkCreateSemaphore(
                device,
                &semaphoreCreateInfo,
                nullptr,
                &frameData.renderSemaphore
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
            { vkDestroySemaphore(device, frameData.renderSemaphore, nullptr); }
            );
        }
    }

    return {buffer, VK_SUCCESS};
}

auto FrameBuffer::currentFrame() -> Frame&
{
    size_t const index{m_frameNumber % m_frames.size()};
    return m_frames[index];
}

auto FrameBuffer::frameNumber() const -> size_t { return m_frameNumber; }

void FrameBuffer::increment() { m_frameNumber += 1; }

void FrameBuffer::destroy(VkDevice device)
{
    for (Frame const& frame : m_frames)
    {
        vkDestroyCommandPool(device, frame.commandPool, nullptr);

        vkDestroyFence(device, frame.renderFence, nullptr);
        vkDestroySemaphore(device, frame.renderSemaphore, nullptr);
        vkDestroySemaphore(device, frame.swapchainSemaphore, nullptr);
    }
}
