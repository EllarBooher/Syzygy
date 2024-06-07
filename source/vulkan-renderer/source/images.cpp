#include "images.hpp"

#include "helpers.hpp"
#include "initializers.hpp"

static auto extentToOffset(VkExtent3D const extent) -> VkOffset3D
{
    auto const x = static_cast<int32_t>(extent.width);
    auto const y = static_cast<int32_t>(extent.height);
    auto const z = static_cast<int32_t>(extent.depth);

    return { x, y, z };
}

void vkutil::transitionImage(
    VkCommandBuffer const cmd
    , VkImage const image
    , VkImageLayout const oldLayout
    , VkImageLayout const newLayout
    , VkImageAspectFlags const aspects
)
{
    VkImageMemoryBarrier2 const imageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,

        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .dstAccessMask =
            VK_ACCESS_2_MEMORY_WRITE_BIT
            | VK_ACCESS_2_MEMORY_READ_BIT
        ,

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
    VkCommandBuffer const cmd
    , VkImage const source
    , VkImage const destination
    , VkExtent3D const srcSize
    , VkExtent3D const dstSize
)
{
    VkImageBlit2 const blitRegion{
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,
        .srcSubresource = vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0), 
        .srcOffsets = {
            VkOffset3D{},
            extentToOffset(srcSize),
        },
        .dstSubresource = vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0), 
        .dstOffsets = {
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
        .filter = VK_FILTER_LINEAR,
    };

    vkCmdBlitImage2(cmd, &blitInfo);
}

void vkutil::recordCopyImageToImage(
    VkCommandBuffer const cmd
    , VkImage const source
    , VkImage const destination
    , VkOffset3D const srcMin
    , VkOffset3D const srcMax
    , VkOffset3D const dstMin
    , VkOffset3D const dstMax
)
{
    VkImageBlit2 const blitRegion{
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,
        .srcSubresource = vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0),
        .srcOffsets = {
            srcMin,
            srcMax,
        },
        .dstSubresource = vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0),
        .dstOffsets = {
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
    VkCommandBuffer const cmd
    , VkImage const source
    , VkImage const destination
    , VkExtent2D const srcSize
    , VkExtent2D const dstSize
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
        cmd
        , source
        , destination
        , srcExtent
        , dstExtent
    );
}

void vkutil::recordCopyImageToImage(
    VkCommandBuffer const cmd
    , VkImage const source
    , VkImage const destination
    , VkRect2D const srcSize
    , VkRect2D const dstSize
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
        cmd
        , source
        , destination
        , srcMin, srcMax
        , dstMin, dstMax
    );
}

auto vkutil::aspectRatio(VkExtent2D const extent) -> double
{
    auto const width{ static_cast<float>(extent.width) };
    auto const height{ static_cast<float>(extent.height) };

    float const rawAspectRatio = width / height;

    return std::isfinite(rawAspectRatio) ? rawAspectRatio : 1.0f;
}

auto AllocatedImage::allocate(
    VmaAllocator const allocator,
    VkDevice const device,
    VkExtent3D const extent,
    VkFormat const format,
    VkImageAspectFlags const viewFlags,
    VkImageUsageFlags const usageMask
) -> std::optional<AllocatedImage>
{
    AllocatedImage image{
        .allocation = VK_NULL_HANDLE,
        .image = VK_NULL_HANDLE,
        .imageView = VK_NULL_HANDLE,

        .imageExtent = extent,
        .imageFormat = format
    };

    VkImageCreateInfo const imageInfo{ 
        vkinit::imageCreateInfo(
            image.imageFormat
            , VK_IMAGE_LAYOUT_UNDEFINED
            , usageMask
            , image.imageExtent
        ) 
    };

    VmaAllocationCreateInfo const imageAllocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VkResult const createImageResult{
        vmaCreateImage(
            allocator
            , &imageInfo
            , &imageAllocInfo
            , &image.image
            , &image.allocation
            , nullptr
        )
    };
    if (createImageResult != VK_SUCCESS)
    {
        LogVkResult(createImageResult, "VMA Allocation failed");
        return {};
    }

    VkImageViewCreateInfo const imageViewInfo{
        vkinit::imageViewCreateInfo(
            image.imageFormat
            , image.image
            , viewFlags
        )
    };

    VkResult const createViewResult{
        vkCreateImageView(device, &imageViewInfo, nullptr, &image.imageView)
    };
    if (createViewResult != VK_SUCCESS)
    {
        LogVkResult(createViewResult, "vkCreateImageView failed");
        return {};
    }

    return image;
}