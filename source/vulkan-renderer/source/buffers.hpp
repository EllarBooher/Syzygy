#pragma once

#include "enginetypes.hpp"

#include "helpers.hpp"

#include <vk_mem_alloc.h>

// A single VkBuffer alongside all of its allocation information.
struct AllocatedBuffer
{
    AllocatedBuffer(){};

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
    AllocatedBuffer& operator=(AllocatedBuffer const& other) = delete;
    AllocatedBuffer& operator=(AllocatedBuffer&& other) = default;

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

    // For now we store all of this with each buffer to simplify management
    // at the cost of memory and speed.
    VmaAllocator allocator{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo info{};
    VkDeviceAddress deviceAddress{};

    VkBuffer buffer{VK_NULL_HANDLE};

    static AllocatedBuffer allocate(
        VkDevice device,
        VmaAllocator allocator,
        size_t allocationSize,
        VkBufferUsageFlags bufferUsage,
        VmaMemoryUsage memoryUsage,
        VmaAllocationCreateFlags createFlags
    );
};

// Two linked buffers of the same capacity, one on host and one on device.
struct StagedBuffer
{
    StagedBuffer() = delete;

    StagedBuffer(StagedBuffer const& other) = delete;

    StagedBuffer(StagedBuffer&& other) = default;

    StagedBuffer& operator=(StagedBuffer const& other) = delete;

    StagedBuffer& operator=(StagedBuffer&& other) = default;

    static StagedBuffer allocate(
        VkDevice device,
        VmaAllocator allocator,
        VkDeviceSize allocationSize,
        VkBufferUsageFlags bufferUsage
    );

    VkDeviceAddress deviceAddress() const;
    VkBuffer deviceBuffer() const { return m_deviceBuffer.buffer; };

    void overwriteStagedBytes(std::span<uint8_t const> data);
    void pushStagedBytes(std::span<uint8_t const> data);
    void popStagedBytes(size_t count);

    // This zeroes out the size flags, and leaves the memory as-is.
    void clearStaged();
    // This zeroes out the size flags, and leaves the memory as-is.
    void clearStagedAndDevice();

    // This structure cannot know exactly how many bytes are up-to-date on the
    // device side. This value is updated upon recording a copy, and assumes
    // correct barrier usage so that the staged bytes in the staged amount are
    // present when queueing further commands with read accesses.
    // Thus, this is a read after write hazard that the host must be careful of.
    VkDeviceSize deviceSizeQueuedBytes() const { return m_deviceSizeBytes; };

    VkDeviceSize stagedCapacityBytes() const
    {
        return m_stagingBuffer.info.size;
    };
    VkDeviceSize stagedSizeBytes() const { return m_stagedSizeBytes; };

    // Does not record any barriers. See StagedBuffer::recordTotalCopyBarrier.
    // This creates the assumption that the memory on the device is a snapshot
    // of the staged memory at this point, even if a barrier has not been
    // recorded yet.
    void recordCopyToDevice(VkCommandBuffer cmd, VmaAllocator allocator);

    // Records a barrier to compliment StagedBuffer::recordCopyToDevice.
    void recordTotalCopyBarrier(
        VkCommandBuffer cmd,
        VkPipelineStageFlags2 destinationStage,
        VkAccessFlags2 destinationAccessFlags
    ) const;

    bool isDirty() const { return m_dirty; };

protected:
    StagedBuffer(
        AllocatedBuffer&& deviceBuffer, AllocatedBuffer&& stagingBuffer
    )
        : m_deviceBuffer(std::move(deviceBuffer))
        , m_stagingBuffer(std::move(stagingBuffer)){};

    void markDirty(bool dirty) { m_dirty = dirty; }

    // Often we want to read the staged values from the host assuming they are
    // the values that will be on the device during command execution.
    //
    // This flag marks if staged memory is possibly not in sync with
    // device memory.
    bool m_dirty{};

    AllocatedBuffer m_deviceBuffer{};
    VkDeviceSize m_deviceSizeBytes{0};

    AllocatedBuffer m_stagingBuffer{};
    VkDeviceSize m_stagedSizeBytes{0};
};

template <typename T> struct TStagedBuffer : public StagedBuffer
{
    void stage(std::span<T const> data)
    {
        std::span<uint8_t const> const bytes(
            reinterpret_cast<uint8_t const*>(data.data()), data.size_bytes()
        );
        StagedBuffer::overwriteStagedBytes(bytes);
    }
    void push(std::span<T const> data)
    {
        std::span<uint8_t const> const bytes(
            reinterpret_cast<uint8_t const*>(data.data()), data.size_bytes()
        );
        StagedBuffer::pushStagedBytes(bytes);
    }
    void push(T const& data)
    {
        std::span<uint8_t const> const bytes(
            reinterpret_cast<uint8_t const*>(&data), sizeof(T)
        );
        StagedBuffer::pushStagedBytes(bytes);
    }
    void pop(size_t count) { StagedBuffer::popStagedBytes(count * sizeof(T)); }

    // These values may be out of date, and not the values used by the GPU
    // upon command execution.
    // Use this only as a convenient interface for modifying the staged values.
    // TODO: get rid of this and have a write-only interface instead
    std::span<T> mapValidStaged()
    {
        return std::span<T>(
            reinterpret_cast<T*>(m_stagingBuffer.info.pMappedData), stagedSize()
        );
    }

    // This can be used as a proxy for values on the device,
    // as long as the only writes are from the host.
    std::span<T const> readValidStaged() const
    {
        if (isDirty())
        {
            Warning("Dirty buffer was accessed with a read, "
                    "these are not the values last recorded onto the GPU.");
        }

        return std::span<T const>(
            reinterpret_cast<T const*>(m_stagingBuffer.info.pMappedData),
            stagedSize()
        );
    }

    static TStagedBuffer<T> allocate(
        VkDevice const device,
        VmaAllocator const allocator,
        VkDeviceSize const capacity,
        VkBufferUsageFlags const bufferUsage
    )
    {
        VkDeviceSize const allocationSizeBytes{capacity * sizeof(T)};
        return TStagedBuffer<T>(StagedBuffer::allocate(
            device, allocator, allocationSizeBytes, bufferUsage
        ));
    }

    VkDeviceSize deviceSize() const
    {
        return StagedBuffer::deviceSizeQueuedBytes() / sizeof(T);
    }

    VkDeviceSize stagingCapacity() const
    {
        return StagedBuffer::stagedCapacityBytes() / sizeof(T);
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
        AllocatedBuffer&& indexBuffer, AllocatedBuffer&& vertexBuffer
    )
        : m_indexBuffer(std::move(indexBuffer))
        , m_vertexBuffer(std::move(vertexBuffer))
    {
    }

    GPUMeshBuffers(GPUMeshBuffers const& other) = delete;

    GPUMeshBuffers(GPUMeshBuffers&& other) = default;

    GPUMeshBuffers& operator=(GPUMeshBuffers const& other) = delete;

    GPUMeshBuffers& operator=(GPUMeshBuffers&& other) = default;

    // These are not const since they give access to the underlying memory.

    VkDeviceAddress indexAddress() { return m_indexBuffer.deviceAddress; }
    VkBuffer indexBuffer() { return m_indexBuffer.buffer; }

    VkDeviceAddress vertexAddress() { return m_vertexBuffer.deviceAddress; }
    VkBuffer vertexBuffer() { return m_vertexBuffer.buffer; }

private:
    AllocatedBuffer m_indexBuffer{};
    AllocatedBuffer m_vertexBuffer{};
};
