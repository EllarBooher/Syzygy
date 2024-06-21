#pragma once

#include "enginetypes.hpp"
#include "vulkanusage.hpp"
#include <optional>

#include "helpers.hpp"

namespace vkutil
{
// Transitions the layout of an image, putting in a full memory barrier
// TODO: track image layout on images themselves, and make this automatic
void transitionImage(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspects
);

// Copies all RGBA of an image to another.
// Assumes source is VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
// and destination is VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
void recordCopyImageToImage(
    VkCommandBuffer cmd,
    VkImage source,
    VkImage destination,
    VkExtent3D srcSize,
    VkExtent3D dstSize
);

void recordCopyImageToImage(
    VkCommandBuffer cmd,
    VkImage source,
    VkImage destination,
    VkOffset3D srcMin,
    VkOffset3D srcMax,
    VkOffset3D dstMin,
    VkOffset3D dstMax
);

// Copies a color image, with an assumed depth of 1.
void recordCopyImageToImage(
    VkCommandBuffer cmd,
    VkImage source,
    VkImage destination,
    VkExtent2D srcSize,
    VkExtent2D dstSize
);
void recordCopyImageToImage(
    VkCommandBuffer cmd,
    VkImage source,
    VkImage destination,
    VkRect2D src,
    VkRect2D dst
);

double aspectRatio(VkExtent2D extent);
} // namespace vkutil

// This image is very wasteful with memory, but stores everything it needs for
// operation locally.
struct AllocatedImage
{
public:
    AllocatedImage() = delete;

    AllocatedImage(AllocatedImage&& other) noexcept
    {
        *this = std::move(other);
    };
    AllocatedImage& operator=(AllocatedImage&& other) noexcept
    {
        m_imageCreateInfo = std::exchange(other.m_imageCreateInfo, {});
        m_viewCreateInfo = std::exchange(other.m_viewCreateInfo, {});
        m_vmaCreateInfo = std::exchange(other.m_vmaCreateInfo, {});

        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);

        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);

        m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
        m_view = std::exchange(other.m_view, VK_NULL_HANDLE);

        m_expectedLayout =
            std::exchange(other.m_expectedLayout, VK_IMAGE_LAYOUT_UNDEFINED);

        return *this;
    }

    AllocatedImage(AllocatedImage const& other) = delete;
    AllocatedImage& operator=(AllocatedImage const& other) = delete;

    ~AllocatedImage() noexcept;

public:
    struct AllocationParameters
    {
        VkExtent2D extent{};
        VkFormat format{VK_FORMAT_UNDEFINED};
        VkImageUsageFlags usageFlags{0};
        VkImageAspectFlags viewFlags{VK_IMAGE_ASPECT_NONE};
        VkImageLayout initialLayout{VK_IMAGE_LAYOUT_UNDEFINED};
    };

    static auto allocate(
        VmaAllocator allocator, VkDevice device, AllocationParameters parameters
    ) -> std::optional<AllocatedImage>;

    void recordTransitionBarriered(VkCommandBuffer, VkImageLayout dstLayout);

    // Blits an entire image into the full extent of another image.
    // Does not apply any memory barriers.
    // Expects the images to be in VK_LAYOUT_TRANSFER_SOURCE/DST_OPTIMAL
    static void recordCopyEntire(
        VkCommandBuffer, AllocatedImage& srcImage, AllocatedImage& dstImage
    );
    static void recordCopySubregion(
        VkCommandBuffer,
        AllocatedImage& srcImage,
        VkRect2D srcRegion,
        AllocatedImage& dstImage,
        VkRect2D dstRegion
    );

    auto extent2D() const -> VkExtent2D;
    auto format() const -> VkFormat;

    // The value will be 0.0/inf/NaN for an image without valid bounds.
    auto aspectRatio() const -> double;

    // TODO: deprecate this, since it allows desyncing the layout easily
    auto image() -> VkImage;
    auto view() -> VkImageView;

private:
    AllocatedImage(
        VkImageCreateInfo imageCreateInfo,
        VkImageViewCreateInfo imageViewCreateInfo,
        VmaAllocationCreateInfo vmaCreateInfo,
        VkDevice device,
        VmaAllocator allocator,
        VmaAllocation allocation,
        VkImage image,
        VkImageView imageView,
        VkImageLayout imageLayout
    )
        : m_imageCreateInfo{imageCreateInfo}
        , m_viewCreateInfo{imageViewCreateInfo}
        , m_vmaCreateInfo{vmaCreateInfo}
        , m_device{device}
        , m_allocator{allocator}
        , m_allocation{allocation}
        , m_image{image}
        , m_view{imageView}
        , m_expectedLayout{imageLayout}
    {
    }

    static auto image_impl(AllocatedImage&) -> VkImage;
    static auto view_impl(AllocatedImage&) -> VkImageView;

    VkImageCreateInfo m_imageCreateInfo{};
    VkImageViewCreateInfo m_viewCreateInfo{};
    VmaAllocationCreateInfo m_vmaCreateInfo{};

    VkDevice m_device{VK_NULL_HANDLE};

    VmaAllocator m_allocator{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};

    VkImage m_image{VK_NULL_HANDLE};
    VkImageView m_view{VK_NULL_HANDLE};

    VkImageLayout m_expectedLayout{VK_IMAGE_LAYOUT_UNDEFINED};
};