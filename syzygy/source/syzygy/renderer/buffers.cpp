#include "buffers.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include <algorithm>

namespace syzygy
{
auto StagedBuffer::isDirty() const -> bool { return m_dirty; }

auto AllocatedBuffer::allocate(
    VkDevice const device,
    VmaAllocator const allocator,
    size_t const allocationSize,
    VkBufferUsageFlags const bufferUsage,
    VmaMemoryUsage const memoryUsage,
    VmaAllocationCreateFlags const createFlags
) -> AllocatedBuffer
{
    AllocatedBuffer result{};

    VkBufferCreateInfo const vkCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,

        .size = allocationSize,
        .usage = bufferUsage,
    };

    VmaAllocationCreateInfo const vmaCreateInfo{
        .flags = createFlags,
        .usage = memoryUsage,
    };

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    SZG_CHECK_VK(vmaCreateBuffer(
        allocator,
        &vkCreateInfo,
        &vmaCreateInfo,
        &buffer,
        &allocation,
        &allocationInfo
    ));

    VkDeviceAddress deviceAddress{0};
    if ((bufferUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0U)
    {
        VkBufferDeviceAddressInfo const addressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext = nullptr,

            .buffer = buffer,
        };
        deviceAddress = vkGetBufferDeviceAddress(device, &addressInfo);
    }

    result.m_vkCreateInfo = vkCreateInfo;
    result.m_vmaCreateInfo = vmaCreateInfo;
    result.m_allocation = std::make_shared<BufferAllocation>(
        allocator, allocation, buffer, deviceAddress
    );

    return result;
}

auto AllocatedBuffer::bufferSize() const -> VkDeviceSize
{
    return m_vkCreateInfo.size;
}

auto AllocatedBuffer::isMapped() const -> bool
{
    return m_allocation != VK_NULL_HANDLE
        && m_allocation->getMappedPointer() != nullptr;
}

void AllocatedBuffer::writeBytes(
    VkDeviceSize const offset, std::span<uint8_t const> const data
)
{
    assert(data.size_bytes() + offset <= bufferSize());

    uint8_t* const start{
        reinterpret_cast<uint8_t*>(m_allocation->getMappedPointer()) + offset
    };
    std::copy(data.begin(), data.end(), start);
}

auto AllocatedBuffer::readBytes() const -> std::span<uint8_t const>
{
    if (m_allocation == VK_NULL_HANDLE)
    {
        return {};
    }

    return {m_allocation->getMappedPointer(), bufferSize()};
}

auto AllocatedBuffer::mappedBytes() -> std::span<uint8_t>
{
    if (m_allocation == VK_NULL_HANDLE)
    {
        return {};
    }

    return {m_allocation->getMappedPointer(), bufferSize()};
}

auto AllocatedBuffer::deviceAddress() const -> VkDeviceAddress
{
    if ((m_vkCreateInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == 0)
    {
        SZG_WARNING(
            "Accessed device address of buffer that was not created with "
            "address flag set."
        );
    }

    return m_allocation->address();
}

auto AllocatedBuffer::buffer() const -> VkBuffer
{
    return m_allocation->buffer();
}

void AllocatedBuffer::bind(CommandBuffer& cmd) { cmd.bindBuffer(m_allocation); }

auto AllocatedBuffer::flush() -> VkResult { return m_allocation->flush(); }

void StagedBuffer::recordCopyToDevice(CommandBuffer& cmd)
{
    SZG_CHECK_VK(m_stagingBuffer->flush());

    markDirty(false);

    VkBufferCopy const copyInfo{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = m_stagedSizeBytes,
    };

    // Binding this way could lead to duplicate binding. But that seems okay for
    // how we are using his.
    m_stagingBuffer->bind(cmd);
    m_deviceBuffer->bind(cmd);

    vkCmdCopyBuffer(
        cmd.handle(),
        m_stagingBuffer->buffer(),
        m_deviceBuffer->buffer(),
        1,
        &copyInfo
    );

    m_deviceSizeBytes = m_stagedSizeBytes;
}

auto StagedBuffer::deviceAddress() const -> VkDeviceAddress
{
    if (isDirty())
    {
        SZG_WARNING(
            "Dirty buffer's device address was accessed, the buffer's "
            "binding is possibly not tracked and may have unexpected values at "
            "command execution."
        );
    }

    return m_deviceBuffer->deviceAddress();
}

auto StagedBuffer::deviceBuffer() const -> VkBuffer
{
    if (isDirty())
    {
        SZG_WARNING(
            "Dirty buffer's handle was accessed, the buffer's binding is "
            "possibly not tracked and may have unexpected values at command "
            "execution."
        );
    }

    return m_deviceBuffer->buffer();
}

void StagedBuffer::overwriteStagedBytes(std::span<uint8_t const> const data)
{
    clearStaged();
    markDirty(true);
    pushStagedBytes(data);
}

void StagedBuffer::pushStagedBytes(std::span<uint8_t const> const data)
{
    m_stagingBuffer->writeBytes(m_stagedSizeBytes, data);

    markDirty(true);
    m_stagedSizeBytes += data.size_bytes();
}

void StagedBuffer::resizeStagedBytes(size_t const count)
{
    // TODO: resize/reallocate buffers
    assert(count <= m_stagingBuffer->bufferSize());

    markDirty(true);
    m_stagedSizeBytes = count;
}

void StagedBuffer::popStagedBytes(size_t const count)
{
    markDirty(true);

    if (count > m_stagedSizeBytes)
    {
        m_stagedSizeBytes = 0;
        return;
    }

    m_stagedSizeBytes -= count;
}

void StagedBuffer::clearStaged()
{
    markDirty(true);

    m_stagedSizeBytes = 0;
}

void StagedBuffer::clearStagedAndDevice()
{
    m_stagedSizeBytes = 0;
    m_deviceSizeBytes = 0;
}

auto StagedBuffer::deviceSizeQueuedBytes() const -> VkDeviceSize
{
    return m_deviceSizeBytes;
}

auto StagedBuffer::stagedCapacityBytes() const -> VkDeviceSize
{
    return m_stagingBuffer->bufferSize();
}

auto StagedBuffer::stagedSizeBytes() const -> VkDeviceSize
{
    return m_stagedSizeBytes;
}

auto StagedBuffer::mapFullCapacityBytes() -> std::span<uint8_t>
{
    return m_stagingBuffer->mappedBytes();
}

auto StagedBuffer::mapStagedBytes() -> std::span<uint8_t>
{
    std::span<uint8_t> const bufferBytes{m_stagingBuffer->mappedBytes()};

    assert(m_stagedSizeBytes <= bufferBytes.size());

    return {bufferBytes.data(), m_stagedSizeBytes};
}

auto StagedBuffer::readStagedBytes() const -> std::span<uint8_t const>
{
    std::span<uint8_t const> const bufferBytes{m_stagingBuffer->readBytes()};

    assert(m_stagedSizeBytes <= bufferBytes.size());

    return {bufferBytes.data(), m_stagedSizeBytes};
}

auto StagedBuffer::allocate(
    VkDevice const device,
    VmaAllocator const allocator,
    VkDeviceSize const allocationSize,
    VkBufferUsageFlags const bufferUsage
) -> StagedBuffer
{
    AllocatedBuffer deviceBuffer{AllocatedBuffer::allocate(
        device,
        allocator,
        allocationSize,
        bufferUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0
    )};

    AllocatedBuffer stagingBuffer{AllocatedBuffer::allocate(
        device,
        allocator,
        allocationSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        VMA_ALLOCATION_CREATE_MAPPED_BIT
            | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
    )};

    // We assume the allocation went correctly.
    // TODO: verify where these buffers allocated, and handle if they fail

    return {std::move(deviceBuffer), std::move(stagingBuffer)};
}

void StagedBuffer::recordTotalCopyBarrier(
    CommandBuffer& cmd,
    VkPipelineStageFlags2 const destinationStage,
    VkAccessFlags2 const destinationAccessFlags
) const
{
    VkBufferMemoryBarrier2 const bufferMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = nullptr,

        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

        .dstStageMask = destinationStage,
        .dstAccessMask = destinationAccessFlags,

        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .buffer = deviceBuffer(),
        .offset = 0,
        .size = deviceSizeQueuedBytes(),
    };

    VkDependencyInfo const transformsDependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,

        .dependencyFlags = 0,

        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,

        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &bufferMemoryBarrier,

        .imageMemoryBarrierCount = 0,
        .pImageMemoryBarriers = nullptr,
    };

    m_deviceBuffer->bind(cmd);
    vkCmdPipelineBarrier2(cmd.handle(), &transformsDependency);
}
} // namespace syzygy