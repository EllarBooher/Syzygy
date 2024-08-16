#pragma once

#include "syzygy/vulkanusage.hpp"
#include <memory>
#include <optional>

namespace syzygy
{
struct ImageMemory
{
    VkDevice device{VK_NULL_HANDLE};
    VmaAllocator allocator{VK_NULL_HANDLE};

    VmaAllocationCreateInfo allocationCreateInfo{};
    VmaAllocation allocation{VK_NULL_HANDLE};

    VkImageCreateInfo imageCreateInfo{};
    VkImage image{VK_NULL_HANDLE};
};

struct ImageAllocationParameters
{
    VkExtent2D extent{};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkImageUsageFlags usageFlags{0};
    VkImageLayout initialLayout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageTiling tiling{VK_IMAGE_TILING_OPTIMAL};
    VmaMemoryUsage vmaUsage{VMA_MEMORY_USAGE_GPU_ONLY};
    VmaAllocationCreateFlags vmaFlags{0};
};

struct Image
{
public:
    Image& operator=(Image&&) = delete;

    Image(Image const&) = delete;
    Image& operator=(Image const&) = delete;

    Image(Image&&) noexcept;
    ~Image();

private:
    Image() = default;
    void destroy();

public:
    static auto
    allocate(VkDevice, VmaAllocator, ImageAllocationParameters const&)
        -> std::optional<std::unique_ptr<Image>>;

    // For now, all images are 2D (depth of 1)
    auto extent3D() const -> VkExtent3D;
    auto extent2D() const -> VkExtent2D;

    auto aspectRatio() const -> std::optional<double>;
    auto format() const -> VkFormat;

    // WARNING: Do not destroy this image. Be careful of implicit layout
    // transitions, which may break the guarantee of Image::expectedLayout.
    auto image() -> VkImage;
    auto fetchAllocationInfo() -> std::optional<VmaAllocationInfo>;

    auto expectedLayout() const -> VkImageLayout;
    void recordTransitionBarriered(
        VkCommandBuffer, VkImageLayout dst, VkImageAspectFlags
    );

    // Assumes images are in TRANSFER_[DST/SRC]_OPTIMAL.
    static void recordCopyEntire(
        VkCommandBuffer, Image& src, Image& dst, VkImageAspectFlags
    );

    // Assumes images are in TRANSFER_[DST/SRC]_OPTIMAL.
    static void recordCopyRect(
        VkCommandBuffer,
        Image& src,
        Image& dst,
        VkImageAspectFlags,
        VkOffset3D srcMin,
        VkOffset3D srcMax,
        VkOffset3D dstMin,
        VkOffset3D dstMax
    );

private:
    ImageMemory m_memory{};
    VkImageLayout m_recordedLayout{};
};
} // namespace syzygy