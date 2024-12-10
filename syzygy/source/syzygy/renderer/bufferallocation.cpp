#include "bufferallocation.hpp"

#include "syzygy/core/log.hpp"

namespace syzygy
{
BufferAllocation::BufferAllocation(BufferAllocation&& other) noexcept
{
    *this = std::move(other);
}
auto BufferAllocation::operator=(BufferAllocation&& other) noexcept
    -> BufferAllocation&
{
    m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
    m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);

    m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);

    return *this;
}
BufferAllocation::~BufferAllocation()
{
    if (m_allocator == VK_NULL_HANDLE
        && (m_allocation != VK_NULL_HANDLE || m_buffer != VK_NULL_HANDLE))
    {
        SZG_WARNING(
            "Allocator was null when attempting to destroy buffer and/or "
            "memory."
        );
        return;
    }

    if (m_allocator == VK_NULL_HANDLE)
    {
        return;
    }

    // VMA handles the case of if one or the other is null
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
}

auto BufferAllocation::buffer() const -> VkBuffer { return m_buffer; }

auto BufferAllocation::address() const -> VkDeviceAddress { return m_address; }

auto BufferAllocation::getMappedPointer() -> uint8_t*
{
    return getMappedPointer_impl(*this);
}

auto BufferAllocation::getMappedPointer() const -> uint8_t const*
{
    return getMappedPointer_impl(*this);
}

auto BufferAllocation::flush() -> VkResult { return flush_impl(*this); }

auto BufferAllocation::getMappedPointer_impl(BufferAllocation& buffer)
    -> uint8_t*
{
    void* const rawPointer{allocationInfo_impl(buffer).pMappedData};

    return reinterpret_cast<uint8_t*>(rawPointer);
}

auto BufferAllocation::getMappedPointer_impl(BufferAllocation const& buffer)
    -> uint8_t const*
{
    void* const rawPointer{allocationInfo_impl(buffer).pMappedData};

    return reinterpret_cast<uint8_t const*>(rawPointer);
}

auto BufferAllocation::flush_impl(BufferAllocation& buffer) -> VkResult
{
    return vmaFlushAllocation(
        buffer.m_allocator, buffer.m_allocation, 0, VK_WHOLE_SIZE
    );
}

auto BufferAllocation::allocationInfo_impl(BufferAllocation const& buffer)
    -> VmaAllocationInfo
{
    VmaAllocationInfo allocationInfo;

    vmaGetAllocationInfo(
        buffer.m_allocator, buffer.m_allocation, &allocationInfo
    );

    return allocationInfo;
}
} // namespace syzygy