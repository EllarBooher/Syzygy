#include "commandbuffer.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"

#include <cassert>

auto syzygy::CommandBuffer::handle() const -> VkCommandBuffer { return m_cmd; }

void syzygy::CommandBuffer::bindBuffer(
    std::shared_ptr<BufferAllocation> const buffer
)
{
    m_boundBuffers.push_back(buffer);
}

auto syzygy::CommandBuffer::create(VkCommandBuffer const cmd) -> CommandBuffer
{
    assert(cmd != VK_NULL_HANDLE && "Cannot utilize null command buffer.");

    CommandBuffer result{};
    result.m_cmd = cmd;

    return result;
}

auto syzygy::CommandBuffer::restart() -> VkResult
{
    m_boundBuffers.clear();
    if (VkResult const resetCmdResult{vkResetCommandBuffer(m_cmd, 0)};
        resetCmdResult != VK_SUCCESS)
    {
        SZG_LOG_VK(resetCmdResult, "Failed to reset frame command buffer.");
        return resetCmdResult;
    }

    VkCommandBufferBeginInfo const cmdBeginInfo{syzygy::commandBufferBeginInfo(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    )};
    if (VkResult const beginCmdResult{vkBeginCommandBuffer(m_cmd, &cmdBeginInfo)
        };
        beginCmdResult != VK_SUCCESS)
    {
        SZG_LOG_VK(beginCmdResult, "Failed to begin frame command buffer.");
        return beginCmdResult;
    }

    return VK_SUCCESS;
}
