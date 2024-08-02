#include "image.hpp"

#include "syzygy/helpers.hpp"
#include "syzygy/images/imageoperations.hpp"
#include <fmt/core.h>

szg_image::Image::Image(Image&& other) noexcept
{
    m_memory = std::exchange(other.m_memory, ImageMemory{});
}

szg_image::Image::~Image() { destroy(); }

void szg_image::Image::destroy()
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
        Warning(fmt::format(
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

auto szg_image::Image::allocate(
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

    Image finalImage{};

    VkImage image;
    VmaAllocation allocation;
    VkResult const createImageResult{vmaCreateImage(
        allocator, &imageInfo, &imageAllocInfo, &image, &allocation, nullptr
    )};
    if (createImageResult != VK_SUCCESS)
    {
        LogVkResult(createImageResult, "VMA Allocation for image failed.");
        return std::nullopt;
    }

    finalImage.m_memory = ImageMemory{
        .device = device,
        .allocator = allocator,
        .allocationCreateInfo = imageAllocInfo,
        .allocation = allocation,
        .imageCreateInfo = imageInfo,
        .image = image,
    };

    finalImage.m_recordedLayout = imageInfo.initialLayout;

    return std::make_unique<Image>(std::move(finalImage));
}

auto szg_image::Image::extent3D() const -> VkExtent3D
{
    return m_memory.imageCreateInfo.extent;
}

auto szg_image::Image::extent2D() const -> VkExtent2D
{
    return VkExtent2D{.width = extent3D().width, .height = extent3D().height};
}

auto szg_image::Image::aspectRatio() const -> std::optional<double>
{
    return szg_image::aspectRatio(extent2D());
}

auto szg_image::Image::format() const -> VkFormat
{
    return m_memory.imageCreateInfo.format;
}

auto szg_image::Image::image() -> VkImage { return m_memory.image; }

auto szg_image::Image::expectedLayout() const -> VkImageLayout
{
    return m_recordedLayout;
}

void szg_image::Image::recordTransitionBarriered(
    VkCommandBuffer const cmd,
    VkImageLayout const dst,
    VkImageAspectFlags const aspectMask
)
{
    szg_image::transitionImage(
        cmd, m_memory.image, m_recordedLayout, dst, aspectMask
    );

    m_recordedLayout = dst;
}

void szg_image::Image::recordCopyEntire(
    VkCommandBuffer const cmd,
    Image& src,
    Image& dst,
    VkImageAspectFlags const aspectMask
)
{
    szg_image::recordCopyImageToImage(
        cmd,
        src.image(),
        dst.image(),
        aspectMask,
        src.extent3D(),
        dst.extent3D()
    );
}

void szg_image::Image::recordCopyRect(
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
    szg_image::recordCopyImageToImage(
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
