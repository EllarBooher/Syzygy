#pragma once

#include "engine_types.h"

#include <vk_mem_alloc.h>

/** A single VkBuffer alongside its allocation info. */
struct AllocatedBuffer {
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo info{};
    VkDeviceAddress address{};

    VkBuffer buffer{ VK_NULL_HANDLE };

    template<typename T>
    std::span<T> map()
    {
        assert(info.pMappedData != nullptr);

        size_t const count{ info.size / sizeof(T) };
        return std::span<T>(reinterpret_cast<T*>(info.pMappedData), count);
    }

    void cleanup(VmaAllocator allocator)
    {
        vmaDestroyBuffer(allocator, buffer, allocation);
    }

    static AllocatedBuffer allocate(
        VkDevice device,
        VmaAllocator allocator,
        size_t allocationSize,
        VkBufferUsageFlags bufferUsage,
        VmaMemoryUsage memoryUsage,
        VmaAllocationCreateFlags createFlags
    );
};

/** Two buffers*/
struct StagedBuffer {
    AllocatedBuffer deviceBuffer{ VK_NULL_HANDLE };
    AllocatedBuffer stagingBuffer{ VK_NULL_HANDLE };

    template<typename T>
    std::span<T> map()
    {
        return stagingBuffer.map<T>();
    }

    /** Does not record any barriers. Requires the allocator to possibly flush the staging buffer. */
    void recordCopy(VkCommandBuffer cmd, VmaAllocator allocator);

    VkDeviceAddress address() const
    {
        return deviceBuffer.address;
    }

    void cleanup(VmaAllocator allocator)
    {
        stagingBuffer.cleanup(allocator);
        deviceBuffer.cleanup(allocator);
    }

    static StagedBuffer allocate(
        VkDevice device,
        VmaAllocator allocator,
        size_t allocationSize,
        VkBufferUsageFlags bufferUsage
    );
};

struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;

    void cleanup(VmaAllocator allocator)
    {
        indexBuffer.cleanup(allocator);
        vertexBuffer.cleanup(allocator);
    }
};