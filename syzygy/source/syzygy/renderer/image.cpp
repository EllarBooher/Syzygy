#include "image.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/renderer/imageoperations.hpp"
#include <utility>

syzygy::Image::Image(Image&& other) noexcept
{
    m_memory = std::exchange(other.m_memory, ImageMemory{});
}

syzygy::Image::~Image() { destroy(); }

void syzygy::Image::destroy()
{
    bool leaked{false};
    if (m_memory.allocation != VK_NULL_HANDLE)
    {
        if (m_memory.allocator != VK_NULL_HANDLE)
        {
            vmaDestroyImage(
                m_memory.allocator, m_memory.image, m_memory.allocation
            );
        }
        else
        {
            leaked = true;
        }
    }
    else if (m_memory.image != VK_NULL_HANDLE)
    {
        if (m_memory.device == VK_NULL_HANDLE)
        {
            vkDestroyImage(m_memory.device, m_memory.image, nullptr);
        }
        else
        {
            leaked = true;
        }
    }

    if (leaked)
    {
        SZG_WARNING(fmt::format(
            "Leak detected in image. Allocator: {}. "
            "Allocation: {}. Device: {}. VkImage: {}.",
            fmt::ptr(m_memory.allocator),
            fmt::ptr(m_memory.allocation),
            fmt::ptr(m_memory.device),
            fmt::ptr(m_memory.image)
        ));
    }

    m_memory = ImageMemory{};
    m_recordedLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

auto syzygy::Image::allocate(
    VkDevice const device,
    VmaAllocator const allocator,
    ImageAllocationParameters const& parameters
) -> std::optional<std::unique_ptr<Image>>
{
    VkExtent3D const extent3D{
        .width = parameters.extent.width,
        .height = parameters.extent.height,
        .depth = 1,
    };

    VkImageCreateInfo const imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,

        .imageType = VK_IMAGE_TYPE_2D,

        .format = parameters.format,
        .extent = extent3D,

        .mipLevels = 1,
        .arrayLayers = 1,

        .samples = VK_SAMPLE_COUNT_1_BIT,

        .tiling = parameters.tiling,
        .usage = parameters.usageFlags,

        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,

        .initialLayout = parameters.initialLayout,
    };

    VmaAllocationCreateInfo const imageAllocInfo{
        .flags = parameters.vmaFlags,
        .usage = parameters.vmaUsage,
    };

    std::optional<std::unique_ptr<Image>> imageResult{
        std::in_place, std::make_unique<Image>(Image{})
    };
    Image& image{*imageResult.value()};

    VkImage imageHandle;
    VmaAllocation allocation;
    VkResult const createImageResult{vmaCreateImage(
        allocator,
        &imageInfo,
        &imageAllocInfo,
        &imageHandle,
        &allocation,
        nullptr
    )};
    if (createImageResult != VK_SUCCESS)
    {
        SZG_LOG_VK(createImageResult, "VMA Allocation for image failed.");
        return std::nullopt;
    }

    image.m_memory = ImageMemory{
        .device = device,
        .allocator = allocator,
        .allocationCreateInfo = imageAllocInfo,
        .allocation = allocation,
        .imageCreateInfo = imageInfo,
        .image = imageHandle,
    };

    image.m_recordedLayout = imageInfo.initialLayout;

    return imageResult;
}

auto syzygy::Image::extent3D() const -> VkExtent3D
{
    return m_memory.imageCreateInfo.extent;
}

auto syzygy::Image::extent2D() const -> VkExtent2D
{
    return VkExtent2D{.width = extent3D().width, .height = extent3D().height};
}

auto syzygy::Image::aspectRatio() const -> std::optional<double>
{
    return syzygy::aspectRatio(extent2D());
}

auto syzygy::Image::format() const -> VkFormat
{
    return m_memory.imageCreateInfo.format;
}

// NOLINTNEXTLINE(readability-make-member-function-const)
auto syzygy::Image::image() -> VkImage { return m_memory.image; }

// NOLINTNEXTLINE(readability-make-member-function-const)
auto syzygy::Image::fetchAllocationInfo() -> std::optional<VmaAllocationInfo>
{
    if (m_memory.allocator == VK_NULL_HANDLE)
    {
        return std::nullopt;
    }
    VmaAllocationInfo allocationInfo;
    vmaGetAllocationInfo(
        m_memory.allocator, m_memory.allocation, &allocationInfo
    );
    return allocationInfo;
}

auto syzygy::Image::expectedLayout() const -> VkImageLayout
{
    return m_recordedLayout;
}

void syzygy::Image::recordTransitionBarriered(
    VkCommandBuffer const cmd,
    VkImageLayout const dst,
    VkImageAspectFlags const aspectMask
)
{
    syzygy::transitionImage(
        cmd, m_memory.image, m_recordedLayout, dst, aspectMask
    );

    m_recordedLayout = dst;
}

void syzygy::Image::recordCopyEntire(
    VkCommandBuffer const cmd,
    Image& src,
    Image& dst,
    VkImageAspectFlags const aspectMask
)
{
    syzygy::recordCopyImageToImage(
        cmd,
        src.image(),
        dst.image(),
        aspectMask,
        src.extent3D(),
        dst.extent3D()
    );
}

void syzygy::Image::recordCopyRect(
    VkCommandBuffer const cmd,
    Image& src,
    Image& dst,
    VkImageAspectFlags const aspectMask,
    VkOffset3D const srcMin,
    VkOffset3D const srcMax,
    VkOffset3D const dstMin,
    VkOffset3D const dstMax
)
{
    syzygy::recordCopyImageToImage(
        cmd,
        src.image(),
        dst.image(),
        aspectMask,
        srcMin,
        srcMax,
        dstMin,
        dstMax
    );
}
