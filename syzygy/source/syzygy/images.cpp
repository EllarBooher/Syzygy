#include "images.hpp"

#include "helpers.hpp"
#include "initializers.hpp"
#include "syzygy/core/integer.hpp"
#include <fmt/core.h>
#include <glm/gtx/compatibility.hpp>
#include <glm/vec2.hpp>

namespace
{
auto extentToOffset(VkExtent3D const extent) -> VkOffset3D
{
    auto const x = static_cast<int32_t>(extent.width);
    auto const y = static_cast<int32_t>(extent.height);
    auto const z = static_cast<int32_t>(extent.depth);

    return {x, y, z};
}
} // namespace

void vkutil::transitionImage(
    VkCommandBuffer const cmd,
    VkImage const image,
    VkImageLayout const oldLayout,
    VkImageLayout const newLayout,
    VkImageAspectFlags const aspects
)
{
    VkImageMemoryBarrier2 const imageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,

        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask =
            VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,

        .oldLayout = oldLayout,
        .newLayout = newLayout,

        .image = image,
        .subresourceRange = vkinit::imageSubresourceRange(aspects),
    };

    VkDependencyInfo const depInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier,
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void vkutil::recordCopyImageToImage(
    VkCommandBuffer const cmd,
    VkImage const source,
    VkImage const destination,
    VkExtent3D const srcSize,
    VkExtent3D const dstSize
)
{
    VkImageBlit2 const blitRegion{
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,
        .srcSubresource =
            vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0),
        .srcOffsets =
            {
                VkOffset3D{},
                extentToOffset(srcSize),
            },
        .dstSubresource =
            vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0),
        .dstOffsets =
            {
                VkOffset3D{},
                extentToOffset(dstSize),
            },
    };

    VkBlitImageInfo2 const blitInfo{
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcImage = source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blitRegion,
        .filter = VK_FILTER_NEAREST,
    };

    vkCmdBlitImage2(cmd, &blitInfo);
}

void vkutil::recordCopyImageToImage(
    VkCommandBuffer const cmd,
    VkImage const source,
    VkImage const destination,
    VkOffset3D const srcMin,
    VkOffset3D const srcMax,
    VkOffset3D const dstMin,
    VkOffset3D const dstMax
)
{
    VkImageBlit2 const blitRegion{
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,
        .srcSubresource =
            vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0),
        .srcOffsets =
            {
                srcMin,
                srcMax,
            },
        .dstSubresource =
            vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0),
        .dstOffsets =
            {
                dstMin,
                dstMax,
            },
    };

    VkBlitImageInfo2 const blitInfo{
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcImage = source,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = destination,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blitRegion,
        .filter = VK_FILTER_LINEAR,
    };

    vkCmdBlitImage2(cmd, &blitInfo);
}

void vkutil::recordCopyImageToImage(
    VkCommandBuffer const cmd,
    VkImage const source,
    VkImage const destination,
    VkExtent2D const srcSize,
    VkExtent2D const dstSize
)
{
    VkExtent3D const srcExtent{
        .width = srcSize.width,
        .height = srcSize.height,
        .depth = 1,
    };
    VkExtent3D const dstExtent{
        .width = dstSize.width,
        .height = dstSize.height,
        .depth = 1,
    };

    vkutil::recordCopyImageToImage(
        cmd, source, destination, srcExtent, dstExtent
    );
}

void vkutil::recordCopyImageToImage(
    VkCommandBuffer const cmd,
    VkImage const source,
    VkImage const destination,
    VkRect2D const srcSize,
    VkRect2D const dstSize
)
{
    VkOffset3D const srcMin{
        .x = static_cast<int32_t>(srcSize.offset.x),
        .y = static_cast<int32_t>(srcSize.offset.y),
        .z = 0,
    };
    VkOffset3D const srcMax{
        .x = static_cast<int32_t>(srcMin.x + srcSize.extent.width),
        .y = static_cast<int32_t>(srcMin.y + srcSize.extent.height),
        .z = 1,
    };
    VkOffset3D const dstMin{
        .x = static_cast<int32_t>(dstSize.offset.x),
        .y = static_cast<int32_t>(dstSize.offset.y),
        .z = 0,
    };
    VkOffset3D const dstMax{
        .x = static_cast<int32_t>(dstMin.x + dstSize.extent.width),
        .y = static_cast<int32_t>(dstMin.y + dstSize.extent.height),
        .z = 1,
    };

    vkutil::recordCopyImageToImage(
        cmd, source, destination, srcMin, srcMax, dstMin, dstMax
    );
}

auto vkutil::aspectRatio(VkExtent2D const extent) -> double
{
    auto const width{static_cast<float>(extent.width)};
    auto const height{static_cast<float>(extent.height)};

    return aspectRatio(glm::vec2{width, height});
}

auto vkutil::aspectRatio(glm::vec2 extent) -> double
{
    float const rawAspectRatio = extent.x / extent.y;

    return glm::isfinite(rawAspectRatio) ? rawAspectRatio : 1.0F;
}

AllocatedImage::~AllocatedImage() noexcept { destroy(); }

auto AllocatedImage::allocate(
    VmaAllocator const allocator,
    VkDevice const device,
    AllocationParameters const parameters
) -> std::optional<AllocatedImage>
{
    if (parameters.extent.width == 0 || parameters.extent.height == 0
        || parameters.format == VK_FORMAT_UNDEFINED
        || parameters.viewFlags == VK_IMAGE_ASPECT_NONE)
    {
        Warning("Image is being allocated with one or more likely invalid "
                "parameters. ");
    }

    VkExtent3D const extent3D{
        .width = parameters.extent.width,
        .height = parameters.extent.height,
        .depth = 1,
    };

    VkImageCreateInfo const imageInfo{vkinit::imageCreateInfo(
        parameters.format,
        parameters.initialLayout,
        parameters.usageFlags,
        extent3D,
        parameters.tiling
    )};

    VmaAllocationCreateInfo const imageAllocInfo{
        .flags = parameters.vmaFlags,
        .usage = parameters.vmaUsage,
    };

    VkImage image;
    VmaAllocation allocation;
    VkResult const createImageResult{vmaCreateImage(
        allocator, &imageInfo, &imageAllocInfo, &image, &allocation, nullptr
    )};
    if (createImageResult != VK_SUCCESS)
    {
        LogVkResult(createImageResult, "VMA Allocation for image failed.");
        return {};
    }

    VkImageViewCreateInfo const imageViewInfo{vkinit::imageViewCreateInfo(
        parameters.format, image, parameters.viewFlags
    )};

    VkImageView imageView;
    VkResult const createViewResult{
        vkCreateImageView(device, &imageViewInfo, nullptr, &imageView)
    };
    if (createViewResult != VK_SUCCESS)
    {
        LogVkResult(createViewResult, "vkCreateImageView failed");

        vmaDestroyImage(allocator, image, allocation);

        return {};
    }

    return AllocatedImage{
        imageInfo,
        imageViewInfo,
        imageAllocInfo,
        device,
        allocator,
        allocation,
        image,
        imageView,
        imageInfo.initialLayout
    };
}

void AllocatedImage::recordTransitionBarriered(
    VkCommandBuffer const cmd, VkImageLayout const dstLayout
)
{
    vkutil::transitionImage(
        cmd,
        m_image,
        m_expectedLayout,
        dstLayout,
        m_viewCreateInfo.subresourceRange.aspectMask
    );

    m_expectedLayout = dstLayout;
}

void AllocatedImage::recordCopyEntire(
    VkCommandBuffer const cmd,
    AllocatedImage& srcImage,
    AllocatedImage& dstImage
)
{
    vkutil::recordCopyImageToImage(
        cmd,
        srcImage.m_image,
        dstImage.m_image,
        srcImage.m_imageCreateInfo.extent,
        dstImage.m_imageCreateInfo.extent
    );
}

void AllocatedImage::recordCopySubregion(
    VkCommandBuffer const cmd,
    AllocatedImage& srcImage,
    VkRect2D const srcRegion,
    AllocatedImage& dstImage,
    VkRect2D const dstRegion
)
{
    vkutil::recordCopyImageToImage(
        cmd, srcImage.m_image, dstImage.m_image, srcRegion, dstRegion
    );
}

auto AllocatedImage::extent2D() const -> VkExtent2D
{
    VkExtent3D const extent{m_imageCreateInfo.extent};

    return {
        .width = extent.width,
        .height = extent.height,
    };
}

auto AllocatedImage::format() const -> VkFormat
{
    return m_imageCreateInfo.format;
}

auto AllocatedImage::aspectRatio() const -> double
{
    return vkutil::aspectRatio(extent2D());
}

auto AllocatedImage::view() -> VkImageView { return view_impl(*this); }

auto AllocatedImage::expectedLayout() const -> VkImageLayout
{
    return m_expectedLayout;
}

namespace
{
auto imageAllocationInfo(
    AllocatedImage&,
    VmaAllocator const allocator,
    VmaAllocation const allocation
)
{
    VmaAllocationInfo allocationInfo;

    vmaGetAllocationInfo(allocator, allocation, &allocationInfo);

    return allocationInfo;
}
} // namespace

auto AllocatedImage::mappedBytes() -> std::optional<std::span<uint8_t>>
{
    if (m_allocation == VK_NULL_HANDLE)
    {
        return std::nullopt;
    }

    VmaAllocationInfo const info{
        imageAllocationInfo(*this, m_allocator, m_allocation)
    };

    auto* const data{reinterpret_cast<uint8_t*>(info.pMappedData)};

    if (data == nullptr)
    {
        return std::nullopt;
    }

    return std::span<uint8_t>{data, info.size};
}

void AllocatedImage::destroy()
{
    if (m_allocation == VK_NULL_HANDLE && m_view == VK_NULL_HANDLE)
    {
        return;
    }

    if (m_allocation == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE
        || m_view == VK_NULL_HANDLE || m_image == VK_NULL_HANDLE)
    {
        Warning(fmt::format(
            "One of but not all image handles were null upon destruction. "
            "This has resulted in a leak. Allocation: {}. Device: {}. "
            "View: {}. Image: {}.",
            reinterpret_cast<uint64_t>(m_allocation),
            reinterpret_cast<uint64_t>(m_device),
            reinterpret_cast<uint64_t>(m_view),
            reinterpret_cast<uint64_t>(m_image)
        ));
        return;
    }

    vkDestroyImageView(m_device, m_view, nullptr);
    vmaDestroyImage(m_allocator, m_image, m_allocation);
}

auto AllocatedImage::image() -> VkImage { return image_impl(*this); }

auto AllocatedImage::view_impl(AllocatedImage& image) -> VkImageView
{
    return image.m_view;
}

auto AllocatedImage::image_impl(AllocatedImage& image) -> VkImage
{
    return image.m_image;
}
