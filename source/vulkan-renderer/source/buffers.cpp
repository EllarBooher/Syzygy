#include "buffers.hpp"
#include "helpers.hpp"

AllocatedBuffer AllocatedBuffer::allocate(
    VkDevice device
    , VmaAllocator allocator
    , size_t allocationSize
    , VkBufferUsageFlags bufferUsage
    , VmaMemoryUsage memoryUsage
    , VmaAllocationCreateFlags createFlags
)
{
    VkBufferCreateInfo const bufferInfo{
        .sType{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO },
        .pNext{ nullptr },

        .size{ allocationSize },
        .usage{ bufferUsage },
    };

    VmaAllocationCreateInfo const vmaAllocInfo{
        .flags{ createFlags },
        .usage{ memoryUsage },
    };

    AllocatedBuffer newBuffer{};
    CheckVkResult(
        vmaCreateBuffer(
            allocator
            , &bufferInfo
            , &vmaAllocInfo
            , &newBuffer.buffer
            , &newBuffer.allocation
            , &newBuffer.info
        )
    );

    newBuffer.deviceAddress = 0;
    if (bufferUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo const addressInfo{
            .sType{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO },
            .pNext{ nullptr },

            .buffer{ newBuffer.buffer },
        };
        newBuffer.deviceAddress = vkGetBufferDeviceAddress(device, &addressInfo);
    }

    newBuffer.allocator = allocator;

    return newBuffer;
}

void StagedBuffer::recordCopyToDevice(
    VkCommandBuffer cmd
    , VmaAllocator allocator
)
{
    CheckVkResult(
        vmaFlushAllocation(
            allocator
            , m_stagingBuffer.allocation
            , 0
            , VK_WHOLE_SIZE
        )
    );

    markDirty(false);

    VkBufferCopy const copyInfo{
        .srcOffset{ 0 },
        .dstOffset{ 0 },
        .size{ m_stagedSizeBytes },
    };
    vkCmdCopyBuffer(
        cmd
        , m_stagingBuffer.buffer
        , m_deviceBuffer.buffer
        , 1, &copyInfo
    );

    m_deviceSizeBytes = m_stagedSizeBytes;
}

VkDeviceAddress StagedBuffer::deviceAddress() const
{
    if (isDirty())
    {
        Warning(
            "Dirty buffer's device address was accessed, "
            "the buffer may have unexpected values at command execution."
        );
    }

    return m_deviceBuffer.deviceAddress;
}

void StagedBuffer::overwriteStagedBytes(std::span<uint8_t const> data)
{
    clearStaged();
    markDirty(true);
    pushStagedBytes(data);
}

void StagedBuffer::pushStagedBytes(std::span<uint8_t const> data)
{
    assert(data.size_bytes() + m_stagedSizeBytes <= m_stagingBuffer.info.size);

    markDirty(true);
    uint8_t* const start{ 
        reinterpret_cast<uint8_t*>(m_stagingBuffer.info.pMappedData) 
        + m_stagedSizeBytes 
    };
    memcpy(start, data.data(), data.size_bytes());
    m_stagedSizeBytes += data.size_bytes();
}

void StagedBuffer::popStagedBytes(size_t count)
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

StagedBuffer StagedBuffer::allocate(
    VkDevice device
    , VmaAllocator allocator
    , VkDeviceSize allocationSize
    , VkBufferUsageFlags bufferUsage
)
{
    AllocatedBuffer deviceBuffer{ 
        AllocatedBuffer::allocate(
            device
            , allocator
            , allocationSize
            , bufferUsage
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            , VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            , 0
        ) 
    };

    AllocatedBuffer stagingBuffer{ 
        AllocatedBuffer::allocate(
            device
            , allocator
            , allocationSize
            , VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            , VMA_MEMORY_USAGE_AUTO_PREFER_HOST
            , VMA_ALLOCATION_CREATE_MAPPED_BIT
            | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
        ) 
    };

    // We assume the allocation went correctly.
    // TODO: verify where these buffers allocated, and handle if they fail

    return StagedBuffer(
        std::move(deviceBuffer)
        , std::move(stagingBuffer)
    );
}

void StagedBuffer::recordTotalCopyBarrier(
    VkCommandBuffer cmd
    , VkPipelineStageFlags2 destinationStage
    , VkAccessFlags2 destinationAccessFlags
) const
{
    VkBufferMemoryBarrier2 const bufferMemoryBarrier{
        .sType{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 },
        .pNext{ nullptr },

        .srcStageMask{ VK_PIPELINE_STAGE_2_COPY_BIT },
        .srcAccessMask{ VK_ACCESS_2_TRANSFER_WRITE_BIT },

        .dstStageMask{ destinationStage },
        .dstAccessMask{ destinationAccessFlags },

        .srcQueueFamilyIndex{ VK_QUEUE_FAMILY_IGNORED },
        .dstQueueFamilyIndex{ VK_QUEUE_FAMILY_IGNORED },

        .buffer{ deviceBuffer() },
        .offset{ 0 },
        .size{ deviceSizeQueuedBytes() },
    };

    VkDependencyInfo const transformsDependency{
        .sType{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO },
        .pNext{ nullptr },

        .dependencyFlags{ 0 },

        .memoryBarrierCount{ 0 },
        .pMemoryBarriers{ nullptr },

        .bufferMemoryBarrierCount{ 1 },
        .pBufferMemoryBarriers{ &bufferMemoryBarrier },

        .imageMemoryBarrierCount{ 0 },
        .pImageMemoryBarriers{ nullptr },
    };

    vkCmdPipelineBarrier2(cmd, &transformsDependency);
}
