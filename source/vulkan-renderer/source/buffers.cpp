#include "buffers.hpp"
#include "helpers.h"

AllocatedBuffer AllocatedBuffer::allocate(
    VkDevice device,
    VmaAllocator allocator,
    size_t allocationSize,
    VkBufferUsageFlags bufferUsage,
    VmaMemoryUsage memoryUsage,
    VmaAllocationCreateFlags createFlags
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
        vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo,
            &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info)
    );

    newBuffer.address = 0;
    if (bufferUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo const addressInfo{
            .sType{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO },
            .pNext{ nullptr },

            .buffer{ newBuffer.buffer },
        };
        newBuffer.address = vkGetBufferDeviceAddress(device, &addressInfo);
    }

    newBuffer.allocator = allocator;

    return newBuffer;
}


/** 
    Requires the allocator to possibly flush the staging buffer. 
    This updates the assumed size of data on the device, which may not be accurate until the gpu copies the data.
    Use a barrier before trying to access the data on the GPU.
*/
void StagedBuffer::recordCopyToDevice(VkCommandBuffer cmd, VmaAllocator allocator)
{
    CheckVkResult(vmaFlushAllocation(allocator, m_stagingBuffer.allocation, 0, VK_WHOLE_SIZE));

    VkBufferCopy const copyInfo{
        .srcOffset{ 0 },
        .dstOffset{ 0 },
        .size{ m_stagedSizeBytes },
    };
    vkCmdCopyBuffer(cmd, m_stagingBuffer.buffer, m_deviceBuffer.buffer, 1, &copyInfo);

    m_deviceSizeBytes = m_stagedSizeBytes;
}

VkDeviceAddress StagedBuffer::address() const
{
    return m_deviceBuffer.address;
}

void StagedBuffer::stage(std::span<uint8_t const> data)
{
    assert(data.size_bytes() <= m_stagingBuffer.info.size);
    m_stagedSizeBytes = data.size_bytes();
    memcpy(m_stagingBuffer.info.pMappedData, data.data(), data.size_bytes());
}

StagedBuffer StagedBuffer::allocate(
    VkDevice device,
    VmaAllocator allocator, 
    VkDeviceSize allocationSize, 
    VkBufferUsageFlags bufferUsage
)
{
    AllocatedBuffer deviceBuffer{ AllocatedBuffer::allocate(
        device
        , allocator
        , allocationSize
        , bufferUsage
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        , VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        , 0
    ) };

    AllocatedBuffer stagingBuffer{ AllocatedBuffer::allocate(
        device
        , allocator
        , allocationSize
        , VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        , VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        , VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
    ) };

    // Don't worry about where these buffers were actually allocated for now, even if this leads to wasteful copies.
    // If they allocated on the wrong host/device, oh well

    return StagedBuffer(
        std::move(deviceBuffer)
        , std::move(stagingBuffer)
    );
}

VkDeviceSize StagedBuffer::stagingCapacityBytes() const
{
    return m_stagingBuffer.info.size;
}
