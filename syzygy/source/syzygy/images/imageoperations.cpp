#include "imageoperations.hpp"

#include "syzygy/initializers.hpp"
#include <glm/gtx/compatibility.hpp>

void szg_image::transitionImage(
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

void szg_image::recordCopyImageToImage(
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

void szg_image::recordCopyImageToImage(
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

    szg_image::recordCopyImageToImage(
        cmd, source, destination, srcMin, srcMax, dstMin, dstMax
    );
}

auto szg_image::aspectRatio(VkExtent2D const extent) -> std::optional<double>
{
    auto const width{static_cast<float>(extent.width)};
    auto const height{static_cast<float>(extent.height)};

    return aspectRatio(glm::vec2{width, height});
}

auto szg_image::aspectRatio(glm::vec2 extent) -> std::optional<double>
{
    double const rawAspectRatio = extent.x / extent.y;

    if (!glm::isfinite(rawAspectRatio))
    {
        return std::nullopt;
    }

    return rawAspectRatio;
}

void szg_image::recordCopyImageToImage(
    VkCommandBuffer const cmd,
    VkImage const src,
    VkImage const dst,
    VkImageAspectFlags const aspectMask,
    VkOffset3D const srcMin,
    VkOffset3D const srcMax,
    VkOffset3D const dstMin,
    VkOffset3D const dstMax
)
{
    VkImageBlit2 const blitRegion{
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,
        .srcSubresource = vkinit::imageSubresourceLayers(aspectMask),
        .srcOffsets = {srcMin, srcMax},
        .dstSubresource =
            vkinit::imageSubresourceLayers(VK_IMAGE_ASPECT_COLOR_BIT, 0),
        .dstOffsets = {dstMin, dstMax},
    };

    // TODO: support more filtering modes. Right now we pretty much only blit 1
    // to 1, but eventually scaling may be necessary if the appearance is bad
    VkBlitImageInfo2 const blitInfo{
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
        .srcImage = src,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = dst,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blitRegion,
        .filter = VK_FILTER_NEAREST,
    };

    vkCmdBlitImage2(cmd, &blitInfo);
}

void szg_image::recordCopyImageToImage(
    VkCommandBuffer const cmd,
    VkImage const src,
    VkImage const dst,
    VkImageAspectFlags const aspectMask,
    VkExtent3D const srcExtent,
    VkExtent3D const dstExtent
)
{
    szg_image::recordCopyImageToImage(
        cmd,
        src,
        dst,
        aspectMask,
        VkOffset3D{},
        VkOffset3D{
            .x = static_cast<int32_t>(srcExtent.width),
            .y = static_cast<int32_t>(srcExtent.height),
            .z = static_cast<int32_t>(srcExtent.depth),
        },
        VkOffset3D{},
        VkOffset3D{
            .x = static_cast<int32_t>(dstExtent.width),
            .y = static_cast<int32_t>(dstExtent.height),
            .z = static_cast<int32_t>(dstExtent.depth),
        }
    );
}
