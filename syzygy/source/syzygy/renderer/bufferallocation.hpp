#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include <utility>

namespace syzygy
{
struct BufferAllocation
{
public:
    BufferAllocation(
        VmaAllocator allocator,
        VmaAllocation allocation,
        VkBuffer buffer,
        VkDeviceAddress address
    )
        : m_allocator{allocator}
        , m_allocation{allocation}
        , m_buffer{buffer}
        , m_address{address}
    {
    }

    BufferAllocation(BufferAllocation&&) noexcept;
    auto operator=(BufferAllocation&&) noexcept -> BufferAllocation&;

    BufferAllocation(BufferAllocation const&) = delete;
    auto operator=(BufferAllocation const&) -> BufferAllocation& = delete;

    ~BufferAllocation();

    // It would be preferable to have separate read/write interfaces, but it
    // takes a bit of work to separate read/write accesses engine-side. So this
    // method provides the raw VkBuffer handle.
    [[nodiscard]] auto buffer() const -> VkBuffer;
    [[nodiscard]] auto address() const -> VkDeviceAddress;

    [[nodiscard]] auto getMappedPointer() -> uint8_t*;
    [[nodiscard]] auto getMappedPointer() const -> uint8_t const*;

    auto flush() -> VkResult;

private:
    // These methods propagate const-correctness

    static auto getMappedPointer_impl(BufferAllocation&) -> uint8_t*;
    static auto getMappedPointer_impl(BufferAllocation const&)
        -> uint8_t const*;

    static auto flush_impl(BufferAllocation& buffer) -> VkResult;
    static auto allocationInfo_impl(BufferAllocation const& buffer)
        -> VmaAllocationInfo;

    VkDeviceAddress m_address{0};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VkBuffer m_buffer{VK_NULL_HANDLE};
};
} // namespace syzygy