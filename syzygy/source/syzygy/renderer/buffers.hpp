#pragma once

#include "syzygy/core/integer.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/vulkanusage.hpp"
#include <cassert>
#include <memory>
#include <span>
#include <utility>

namespace szg_renderer
{
// A single VkBuffer alongside all of its allocation information.
// TODO: split this into two types: a host-side, mapped buffer, and a
// device-side buffer that has its address mapped.
struct AllocatedBuffer
{
public:
    AllocatedBuffer() = delete;

    AllocatedBuffer(AllocatedBuffer&& other) noexcept
    {
        *this = std::move(other);
    };
    AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept
    {
        destroy();

        m_vkCreateInfo = std::exchange(other.m_vkCreateInfo, {});
        m_vmaCreateInfo = std::exchange(other.m_vmaCreateInfo, {});

        m_deviceAddress = std::exchange(other.m_deviceAddress, 0);

        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);

        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);

        return *this;
    }

    AllocatedBuffer(AllocatedBuffer const& other) = delete;
    AllocatedBuffer& operator=(AllocatedBuffer const& other) = delete;

    ~AllocatedBuffer() noexcept { destroy(); }

public:
    static auto allocate(
        VkDevice device,
        VmaAllocator allocator,
        size_t allocationSize,
        VkBufferUsageFlags bufferUsage,
        VmaMemoryUsage memoryUsage,
        VmaAllocationCreateFlags createFlags
    ) -> AllocatedBuffer;

    auto bufferSize() const -> VkDeviceSize;

    auto isMapped() const -> bool;

    void writeBytes(VkDeviceSize offset, std::span<uint8_t const> data);
    auto readBytes() const -> std::span<uint8_t const>;
    auto mappedBytes() -> std::span<uint8_t>;

    auto deviceAddress() const -> VkDeviceAddress;
    auto buffer() const -> VkBuffer;

    VkResult flush();

private:
    void destroy() const;

    AllocatedBuffer(
        VkBufferCreateInfo vkCreateInfo,
        VmaAllocationCreateInfo vmaCreateInfo,
        VmaAllocator allocator,
        VmaAllocation allocation,
        VkDeviceAddress deviceAddress,
        VkBuffer buffer
    )
        : m_vkCreateInfo{vkCreateInfo}
        , m_vmaCreateInfo{vmaCreateInfo}
        , m_deviceAddress{deviceAddress}
        , m_allocator{allocator}
        , m_allocation{allocation}
        , m_buffer{buffer}
    {
    }

    static auto getMappedPointer_impl(AllocatedBuffer&) -> uint8_t*;
    static auto getMappedPointer_impl(AllocatedBuffer const&) -> uint8_t const*;

    static auto flush_impl(AllocatedBuffer& buffer) -> VkResult;
    static auto allocationInfo_impl(AllocatedBuffer const& buffer)
        -> VmaAllocationInfo;

    // For now we store all of this with each buffer to simplify management
    // at the cost of memory and speed.
    VkBufferCreateInfo m_vkCreateInfo{};
    VmaAllocationCreateInfo m_vmaCreateInfo{};

    VkDeviceAddress m_deviceAddress{};

    VmaAllocator m_allocator{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VkBuffer m_buffer{VK_NULL_HANDLE};
};

// Two linked buffers of the same capacity, one on host and one on device.
struct StagedBuffer
{
    StagedBuffer() = delete;

    StagedBuffer(StagedBuffer&& other) { *this = std::move(other); }

    StagedBuffer& operator=(StagedBuffer&& other)
    {
        m_dirty = std::exchange(other.m_dirty, false);

        m_deviceBuffer.reset(other.m_deviceBuffer.release());
        m_deviceSizeBytes = std::exchange(other.m_deviceSizeBytes, 0);

        m_stagingBuffer.reset(other.m_stagingBuffer.release());
        m_stagedSizeBytes = std::exchange(other.m_stagedSizeBytes, 0);

        return *this;
    }

    StagedBuffer(StagedBuffer const& other) = delete;
    StagedBuffer& operator=(StagedBuffer const& other) = delete;

    static auto allocate(
        VkDevice device,
        VmaAllocator allocator,
        VkDeviceSize allocationSize,
        VkBufferUsageFlags bufferUsage
    ) -> StagedBuffer;

    ~StagedBuffer() noexcept = default;

    auto deviceAddress() const -> VkDeviceAddress;
    auto deviceBuffer() const -> VkBuffer;

    void clearStaged();
    void clearStagedAndDevice();

    // Does not record any barriers. See StagedBuffer::recordTotalCopyBarrier.
    // This creates the assumption that the memory on the device is a snapshot
    // of the staged memory at this point, even if a barrier has not been
    // recorded yet.
    void recordCopyToDevice(VkCommandBuffer cmd);

    // Records a barrier to compliment StagedBuffer::recordCopyToDevice.
    void recordTotalCopyBarrier(
        VkCommandBuffer cmd,
        VkPipelineStageFlags2 destinationStage,
        VkAccessFlags2 destinationAccessFlags
    ) const;

protected:
    void overwriteStagedBytes(std::span<uint8_t const> data);
    void pushStagedBytes(std::span<uint8_t const> data);
    void popStagedBytes(size_t count);

    // This structure cannot know exactly how many bytes are up-to-date on the
    // device side. This value is updated upon recording a copy, and assumes
    // correct barrier usage so that the staged bytes in the staged amount are
    // visible when further read accesses are executed.
    // Thus, this represents a read after write hazard that the caller must be
    // careful of.
    auto deviceSizeQueuedBytes() const -> VkDeviceSize;

    auto stagedCapacityBytes() const -> VkDeviceSize;
    auto stagedSizeBytes() const -> VkDeviceSize;

    auto mapStagedBytes() -> std::span<uint8_t>;
    auto readStagedBytes() const -> std::span<uint8_t const>;

    // The buffer is dirtied when the the staged bytes are write accessed, and
    // cleaned when a copy is recorded.
    auto isDirty() const -> bool;

private:
    StagedBuffer(
        AllocatedBuffer&& deviceBuffer, AllocatedBuffer&& stagingBuffer
    )
        : m_deviceBuffer{std::make_unique<AllocatedBuffer>(
            std::move(deviceBuffer)
        )}
        , m_stagingBuffer{
              std::make_unique<AllocatedBuffer>(std::move(stagingBuffer))
          } {};

    void markDirty(bool dirty) { m_dirty = dirty; }

    // Often we want to read the staged values from the host assuming they are
    // the values that will be on the device during command execution.
    //
    // This flag marks if staged memory is possibly not in sync with
    // device memory.
    bool m_dirty{false};

    std::unique_ptr<AllocatedBuffer> m_deviceBuffer;
    VkDeviceSize m_deviceSizeBytes{0};

    std::unique_ptr<AllocatedBuffer> m_stagingBuffer;
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
        std::span<uint8_t> byteSpan{mapStagedBytes()};

        assert(byteSpan.size_bytes() % sizeof(T) == 0);

        return std::span<T>{
            reinterpret_cast<T*>(byteSpan.data()),
            byteSpan.size_bytes() / sizeof(T)
        };
    }

    // This can be used as a proxy for values on the device,
    // as long as the only writes are from the host.
    std::span<T const> readValidStaged() const
    {
        if (isDirty())
        {
            SZG_WARNING(
                "Dirty buffer was accessed with a read, these are not the "
                "values from the last recorded copy."
            );
        }

        std::span<uint8_t const> byteSpan{readStagedBytes()};

        assert(byteSpan.size_bytes() % sizeof(T) == 0);

        return std::span<T const>{
            reinterpret_cast<T const*>(byteSpan.data()),
            byteSpan.size_bytes() / sizeof(T)
        };
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

    VkDeviceAddress indexAddress() { return m_indexBuffer.deviceAddress(); }
    VkBuffer indexBuffer() { return m_indexBuffer.buffer(); }

    VkDeviceAddress vertexAddress() { return m_vertexBuffer.deviceAddress(); }
    VkBuffer vertexBuffer() { return m_vertexBuffer.buffer(); }

private:
    AllocatedBuffer m_indexBuffer;
    AllocatedBuffer m_vertexBuffer;
};
} // namespace szg_renderer