#include "immediate.hpp"
#include "deletionqueue.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/initializers.hpp"
#include <vector>

auto ImmediateSubmissionQueue::operator=(ImmediateSubmissionQueue&& other
) noexcept -> ImmediateSubmissionQueue&
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_busyFence = std::exchange(other.m_busyFence, VK_NULL_HANDLE);
    m_commandPool = std::exchange(other.m_commandPool, VK_NULL_HANDLE);
    m_commandBuffer = std::exchange(other.m_commandBuffer, VK_NULL_HANDLE);

    return *this;
}

auto ImmediateSubmissionQueue::create(
    VkDevice const device, uint32_t const queueFamilyIndex
) -> std::optional<ImmediateSubmissionQueue>
{
    DeletionQueue cleanupCallbacks{};

    VkCommandPoolCreateInfo const commandPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };

    VkCommandPool pool{VK_NULL_HANDLE};
    TRY_VK(
        vkCreateCommandPool(device, &commandPoolInfo, nullptr, &pool),
        "Failed to allocate command pool.",
        std::nullopt
    );
    cleanupCallbacks.pushFunction([&]()
    { vkDestroyCommandPool(device, pool, nullptr); });

    VkCommandBufferAllocateInfo const commandBufferInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,

        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd{VK_NULL_HANDLE};
    TRY_VK(
        vkAllocateCommandBuffers(device, &commandBufferInfo, &cmd),
        "Failed to allocate command buffers.",
        std::nullopt
    );

    VkFenceCreateInfo const fenceCreateInfo{
        vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT)
    };

    VkFence busyFence{VK_NULL_HANDLE};
    TRY_VK(
        vkCreateFence(device, &fenceCreateInfo, nullptr, &busyFence),
        "Failed to create fence.",
        std::nullopt
    );
    cleanupCallbacks.pushFunction([&]()
    { vkDestroyFence(device, busyFence, nullptr); });

    cleanupCallbacks.clear();

    return ImmediateSubmissionQueue{device, busyFence, pool, cmd};
}

auto ImmediateSubmissionQueue::immediateSubmit(
    VkQueue const queue,
    std::function<void(VkCommandBuffer cmd)>&& recordingCallback
) const -> SubmissionResult
{
    if (m_device == VK_NULL_HANDLE)
    {
        SZG_ERROR("Immediate submission queue not initialized.");
        return SubmissionResult::NOT_INITIALZIED;
    }

    TRY_VK(
        vkResetFences(m_device, 1, &m_busyFence),
        "Failed to reset fences",
        SubmissionResult::FAILED
    );
    TRY_VK(
        vkResetCommandBuffer(m_commandBuffer, 0),
        "Failed to reset command buffer",
        SubmissionResult::FAILED
    );

    VkCommandBufferBeginInfo const cmdBeginInfo{vkinit::commandBufferBeginInfo(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    )};

    TRY_VK(
        vkBeginCommandBuffer(m_commandBuffer, &cmdBeginInfo),
        "Failed to begin command buffer",
        SubmissionResult::FAILED
    );

    recordingCallback(m_commandBuffer);

    TRY_VK(
        vkEndCommandBuffer(m_commandBuffer),
        "Failed to end command buffer",
        SubmissionResult::FAILED
    );

    VkCommandBufferSubmitInfo const cmdSubmitInfo{
        vkinit::commandBufferSubmitInfo(m_commandBuffer)
    };
    std::vector<VkCommandBufferSubmitInfo> const cmdSubmitInfos{cmdSubmitInfo};
    VkSubmitInfo2 const submitInfo{vkinit::submitInfo(cmdSubmitInfos, {}, {})};

    TRY_VK(
        vkQueueSubmit2(queue, 1, &submitInfo, m_busyFence),
        "Failed to submit command buffer",
        SubmissionResult::FAILED
    );

    // 1 second timeout
    uint64_t constexpr SUBMIT_TIMEOUT_NANOSECONDS{1'000'000'000};
    VkBool32 constexpr WAIT_ALL{VK_TRUE};
    auto const waitResult{vkWaitForFences(
        m_device, 1, &m_busyFence, WAIT_ALL, SUBMIT_TIMEOUT_NANOSECONDS
    )};

    switch (waitResult)
    {
    case VK_SUCCESS:
        return SubmissionResult::SUCCESS;
        break;
    case VK_TIMEOUT:
        return SubmissionResult::TIMEOUT;
        break;
    default:
        LogVkResult(
            waitResult, "Failed to wait on fences with unexpected error"
        );
        return SubmissionResult::FAILED;
        break;
    }
}

void ImmediateSubmissionQueue::destroy()
{
    if (m_device == VK_NULL_HANDLE)
    {
        return;
    }

    vkDestroyFence(m_device, m_busyFence, nullptr);
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    m_device = VK_NULL_HANDLE;
    m_busyFence = VK_NULL_HANDLE;
    m_commandPool = VK_NULL_HANDLE;
    m_commandBuffer = VK_NULL_HANDLE;
}
