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

    return newBuffer;
}

void StagedBuffer::recordCopy(VkCommandBuffer cmd, VmaAllocator allocator)
{
    CheckVkResult(vmaFlushAllocation(allocator, stagingBuffer.allocation, 0, VK_WHOLE_SIZE));

    VkBufferCopy const copyInfo{
        .srcOffset{ 0 },
        .dstOffset{ 0 },
        .size{ stagingBuffer.info.size },
    };
    vkCmdCopyBuffer(cmd, stagingBuffer.buffer, deviceBuffer.buffer, 1, &copyInfo);
}

StagedBuffer StagedBuffer::allocate(VkDevice device, VmaAllocator allocator, size_t allocationSize, VkBufferUsageFlags bufferUsage)
{
    auto const deviceBuffer = AllocatedBuffer::allocate(
        device
        , allocator
        , allocationSize
        , bufferUsage 
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        , VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        , 0
    );

    auto const stagingBuffer = AllocatedBuffer::allocate(
        device
        , allocator
        , allocationSize
        , VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        , VMA_MEMORY_USAGE_AUTO_PREFER_HOST
        , VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );

    // Don't worry about where these buffers were actually allocated for now, even if this leads to wasteful copies.
    // If they allocated on the wrong host/device, oh well

    return StagedBuffer{
        .deviceBuffer{ deviceBuffer },
        .stagingBuffer{ stagingBuffer },
    };
}
