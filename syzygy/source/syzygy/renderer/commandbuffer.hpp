#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/bufferallocation.hpp"
#include <memory>
#include <vector>

namespace syzygy
{
struct CommandBuffer
{
    [[nodiscard]] auto handle() const -> VkCommandBuffer;

    // TODO: record binding of images

    void bindBuffer(std::shared_ptr<BufferAllocation>);

    static auto create(VkCommandBuffer) -> CommandBuffer;

    // Reset and begin recording again
    auto restart() -> VkResult;

private:
    VkCommandBuffer m_cmd{VK_NULL_HANDLE};

    // Hold onto allocations that are in use so they are not freed.
    std::vector<std::shared_ptr<BufferAllocation>> m_boundBuffers{};
};
} // namespace syzygy