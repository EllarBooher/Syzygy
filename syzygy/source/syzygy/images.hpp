#pragma once

#include "syzygy/vulkanusage.hpp"
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <span>
#include <utility>

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

auto aspectRatio(VkExtent2D extent) -> std::optional<double>;
auto aspectRatio(glm::vec2 extent) -> std::optional<double>;
} // namespace vkutil

// This image is very wasteful with memory, but stores everything it needs for
// operation locally.
// This type is old and messy, and is being slowly refactored into the szg_image
// namespace
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
        destroy();

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
        VkImageTiling tiling{VK_IMAGE_TILING_OPTIMAL};
        VmaMemoryUsage vmaUsage{VMA_MEMORY_USAGE_GPU_ONLY};
        VmaAllocationCreateFlags vmaFlags{};
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

    // As commands are recorded, this value is updated. As such, this is not
    // necessarily the layout the image is in at any given moment, just what
    // commands are recorded using the AllocatedImage API.
    auto expectedLayout() const -> VkImageLayout;

    auto mappedBytes() -> std::optional<std::span<uint8_t>>;

private:
    void destroy();

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

namespace szg_image
{
// Assumes images are in TRANSFER_[DST/SRC]_OPTIMAL
void recordCopyImageToImage(
    VkCommandBuffer,
    VkImage src,
    VkImage dst,
    VkImageAspectFlags aspectMask,
    VkOffset3D srcMin,
    VkOffset3D srcMax,
    VkOffset3D dstMin,
    VkOffset3D dstMax
);

// Assumes images are in TRANSFER_[DST/SRC]_OPTIMAL.
// Starts from offset 0 for both images
void recordCopyImageToImage(
    VkCommandBuffer,
    VkImage src,
    VkImage dst,
    VkImageAspectFlags aspectMask,
    VkExtent3D srcExtent,
    VkExtent3D dstExtent
);

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

struct ImageViewAllocationParameters
{
    // Views use the image's format, or optionally an override that must be
    // compatible according to the compatibilities listed in chapter 48: Formats
    // of the Vulkan Spec.
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#formats-compatibility-classes
    // https://vkdoc.net/chapters/formats#formats-compatibility-classes
    std::optional<VkFormat> formatOverride;
    VkImageViewCreateFlags flags{0};
    VkImageViewType viewType{VK_IMAGE_VIEW_TYPE_2D};
    VkImageSubresourceRange subresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
};

struct ImageViewMemory
{
    VkDevice device{VK_NULL_HANDLE};

    VkImageViewCreateInfo viewCreateInfo{};
    VkImageView view{VK_NULL_HANDLE};
};
struct ImageView
{
public:
    ImageView& operator=(ImageView&&) = delete;

    ImageView(ImageView const&) = delete;
    ImageView& operator=(ImageView const&) = delete;

    ImageView(ImageView&&) noexcept;
    ~ImageView();

private:
    ImageView() = default;
    void destroy();

public:
    static auto allocate(
        VkDevice,
        VmaAllocator,
        ImageAllocationParameters,
        ImageViewAllocationParameters
    ) -> std::optional<std::unique_ptr<ImageView>>;

    // WARNING: Do not destroy this image view.
    auto view() const -> VkImageView;

    auto image() -> Image&;
    auto image() const -> Image const&;

    // Transitions the underlying image, according to the aspect(s) of the view.
    void recordTransitionBarriered(VkCommandBuffer, VkImageLayout);

    auto expectedLayout() const -> VkImageLayout;

private:
    // So far, images and views are 1 to 1. In the future this could be a
    // shared_ptr, or we make a new image view class.
    std::unique_ptr<Image> m_image{};
    ImageViewMemory m_memory{};
};
} // namespace szg_image