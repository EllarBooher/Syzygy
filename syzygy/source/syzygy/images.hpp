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