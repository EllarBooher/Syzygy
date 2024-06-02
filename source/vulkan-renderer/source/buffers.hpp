#pragma once

#include "enginetypes.hpp"

#include "helpers.hpp"

#include <vk_mem_alloc.h>

/** 
    A single VkBuffer alongside all of its allocation information. 
*/
struct AllocatedBuffer 
{
    AllocatedBuffer() {};

    AllocatedBuffer(AllocatedBuffer const& other) = delete;

    AllocatedBuffer(AllocatedBuffer&& other) noexcept
        : allocator(other.allocator)
        , allocation(other.allocation)
        , info(other.info)
        , deviceAddress(other.deviceAddress)
        , buffer(other.buffer)
    {
        other.allocator = VK_NULL_HANDLE;
        other.allocation = VK_NULL_HANDLE;
        other.info = {};
        other.deviceAddress = {};
        other.buffer = VK_NULL_HANDLE;
    };

    ~AllocatedBuffer() noexcept
    {
        if (allocator)
        {
            vmaDestroyBuffer(allocator, buffer, allocation);
        }
        else if (allocation)
        {
            Warning("Failed to destroy buffer with non-null allocation.");
        }
    }

    // For now we store all of this with each buffer to simplify management at the cost of memory and speed.
    VmaAllocator allocator{ VK_NULL_HANDLE };
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo info{};
    VkDeviceAddress deviceAddress{};

    VkBuffer buffer{ VK_NULL_HANDLE };

    static AllocatedBuffer allocate(
        VkDevice device
        , VmaAllocator allocator
        , size_t allocationSize
        , VkBufferUsageFlags bufferUsage
        , VmaMemoryUsage memoryUsage
        , VmaAllocationCreateFlags createFlags
    );
};

/*
* Manages a buffer on the host and a buffer on the device. Tracks how many bytes are valid on either side,
* based on what this structure copies to them. 
*/
struct StagedBuffer 
{
    StagedBuffer() = delete;
    
    StagedBuffer(StagedBuffer const& other) = delete;
    
    StagedBuffer(StagedBuffer&& other) = default;

    StagedBuffer& operator=(StagedBuffer const& other) = delete;

    StagedBuffer& operator=(StagedBuffer&& other) = default;

    static StagedBuffer allocate(
        VkDevice device
        , VmaAllocator allocator
        , VkDeviceSize allocationSize
        , VkBufferUsageFlags bufferUsage
    );

    /** Does not record any barriers. */
    void recordCopyToDevice(
        VkCommandBuffer cmd
        , VmaAllocator allocator
    );

    VkDeviceAddress deviceAddress() const;
    VkBuffer deviceBuffer() const { return m_deviceBuffer.buffer; };

    /** Copy an entire span of data into the staging buffer, and resets its size. */
    void stageBytes(std::span<uint8_t const> data);

    /** Pushes new data into the staging buffer. */
    void pushBytes(std::span<uint8_t const> data);

    void popBytes(size_t count);

    void clearStaged();
    void clearBoth();

    /*
    * This structure cannot know exactly how many bytes are up-to-date on the GPU-side buffer. 
    * Therefore this parameter is updated upon recording a copy, and poses a read-after-write hazard.
    */
    VkDeviceSize deviceSizeQueuedBytes() const { return m_deviceSizeBytes; };

    VkDeviceSize stagingCapacityBytes() const { return m_stagingBuffer.info.size; };
    /** The number of bytes that have been copied to the staging buffer. */
    VkDeviceSize stagedSizeBytes() const { return m_stagedSizeBytes; };

    /** Records a barrier with a source mask for transfer copies, and a destination map for all reads */
    void recordTotalCopyBarrier(
        VkCommandBuffer cmd
        , VkPipelineStageFlags2 destinationStage
        , VkAccessFlags2 destinationAccessFlags
    ) const;

    bool isDirty() const { return m_dirty; };

protected:
    StagedBuffer(
        AllocatedBuffer&& deviceBuffer
        , AllocatedBuffer&& stagingBuffer
    )
        : m_deviceBuffer(std::move(deviceBuffer))
        , m_stagingBuffer(std::move(stagingBuffer))
    {};

    void markDirty(bool dirty)
    {
        m_dirty = dirty;
    }

    /*
    * Often we want to read the staged values (on the host) as if they are the values 
    * that will be on the device during command execution.
    * 
    * This flag marks if the staged values have changed and if this invariant no longer holds, 
    * even if only one value changed.
    */
    bool m_dirty{};

    AllocatedBuffer m_deviceBuffer{};
    VkDeviceSize m_deviceSizeBytes{ 0 };

    AllocatedBuffer m_stagingBuffer{};
    VkDeviceSize m_stagedSizeBytes{ 0 };
};

template<typename T>
struct TStagedBuffer : public StagedBuffer
{
    void stage(std::span<T const> data)
    {
        std::span<uint8_t const> const bytes(
            reinterpret_cast<uint8_t const*>(data.data())
            , data.size_bytes()
        );
        StagedBuffer::stageBytes(bytes);
    }
    void push(std::span<T const> data)
    {
        std::span<uint8_t const> const bytes(
            reinterpret_cast<uint8_t const*>(data.data())
            , data.size_bytes()
        );
        StagedBuffer::pushBytes(bytes);
    }
    void push(T const& data)
    {
        std::span<uint8_t const> const bytes(
            reinterpret_cast<uint8_t const*>(&data)
            , sizeof(T)
        );
        StagedBuffer::pushBytes(bytes);
    }
    void pop(size_t count)
    {
        StagedBuffer::popBytes(count * sizeof(T));
    }

    // These values may be out of date, and not the values used by the GPU upon command execution.
    // Use this only as a convenient interface for modifying the staged values.
    // TODO: get rid of this and have a write-only interface instead
    std::span<T> mapValidStaged()
    {
        return std::span<T>(
            reinterpret_cast<T*>(m_stagingBuffer.info.pMappedData)
            , stagedSize()
        );
    }

    // This can be used as a proxy for values on the device, as long as the only writes are from the host.
    std::span<T const> readValidStaged() const
    {
        if (isDirty())
        {
            Warning("Dirty buffer was accessed with a read, these are not the values last recorded onto the GPU.");
        }

        return std::span<T const>(
            reinterpret_cast<T const*>(m_stagingBuffer.info.pMappedData)
            , stagedSize()
        );
    }

    static TStagedBuffer<T> allocate(
        VkDevice device
        , VmaAllocator allocator
        , VkDeviceSize capacity
        , VkBufferUsageFlags bufferUsage
    )
    {
        VkDeviceSize const allocationSizeBytes{ capacity * sizeof(T) };
        return TStagedBuffer<T>(
            StagedBuffer::allocate(
                device
                , allocator
                , allocationSizeBytes
                , bufferUsage
            )
        );
    }

    VkDeviceSize deviceSize() const
    {
        return StagedBuffer::deviceSizeQueuedBytes() / sizeof(T);
    }

    VkDeviceSize stagingCapacity() const
    {
        return StagedBuffer::stagingCapacityBytes() / sizeof(T);
    }

    VkDeviceSize stagedSize() const 
    { 
        return StagedBuffer::stagedSizeBytes() / sizeof(T);
    }
};

struct GPUMeshBuffers 
{
    GPUMeshBuffers() = delete;

    explicit GPUMeshBuffers(
        AllocatedBuffer&& indexBuffer
        , AllocatedBuffer&& vertexBuffer
    )
        : m_indexBuffer(std::move(indexBuffer))
        , m_vertexBuffer(std::move(vertexBuffer))
    {}

    GPUMeshBuffers(GPUMeshBuffers const& other) = delete;

    GPUMeshBuffers(GPUMeshBuffers&& other) = default;

    GPUMeshBuffers& operator=(GPUMeshBuffers const& other) = delete;

    GPUMeshBuffers& operator=(GPUMeshBuffers&& other) = default;

    VkDeviceAddress indexAddress() { return m_indexBuffer.deviceAddress; }
    VkBuffer indexBuffer() { return m_indexBuffer.buffer; }

    VkDeviceAddress vertexAddress() { return m_vertexBuffer.deviceAddress; }
    VkBuffer vertexBuffer() { return m_vertexBuffer.buffer; }

private:
    AllocatedBuffer m_indexBuffer{};
    AllocatedBuffer m_vertexBuffer{};
};
