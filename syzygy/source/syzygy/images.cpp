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

auto vkutil::aspectRatio(VkExtent2D const extent) -> std::optional<double>
{
    auto const width{static_cast<float>(extent.width)};
    auto const height{static_cast<float>(extent.height)};

    return aspectRatio(glm::vec2{width, height});
}

auto vkutil::aspectRatio(glm::vec2 extent) -> std::optional<double>
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
    return vkutil::aspectRatio(extent2D());
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
    vkutil::transitionImage(
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

szg_image::ImageView::ImageView(ImageView&& other) noexcept
{
    m_image = std::move(other).m_image;

    m_memory = std::exchange(other.m_memory, ImageViewMemory{});
}

szg_image::ImageView::~ImageView() { destroy(); }

auto szg_image::ImageView::allocate(
    VkDevice const device,
    VmaAllocator const allocator,
    ImageAllocationParameters const imageParameters,
    ImageViewAllocationParameters const viewParameters
) -> std::optional<std::unique_ptr<ImageView>>
{
    if (device == VK_NULL_HANDLE || allocator == VK_NULL_HANDLE)
    {
        Error("Device or allocator were null.");
        return std::nullopt;
    }

    std::optional<std::unique_ptr<Image>> imageAllocationResult{
        Image::allocate(device, allocator, imageParameters)
    };
    if (!imageAllocationResult.has_value()
        || imageAllocationResult.value() == nullptr)
    {
        Error("Failed to allocate Image.");
        return std::nullopt;
    }

    ImageView finalView{};
    finalView.m_image = std::move(imageAllocationResult).value();

    Image& image{*finalView.m_image};

    VkImageViewCreateInfo const imageViewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,

        .flags = viewParameters.flags,

        .image = image.image(),
        .viewType = viewParameters.viewType,
        .format = viewParameters.formatOverride.value_or(image.format()),
        .subresourceRange = viewParameters.subresourceRange,
    };

    VkImageView view;
    TRY_VK(
        vkCreateImageView(device, &imageViewInfo, nullptr, &view),
        "Failed to create VkImageView.",
        std::nullopt
    );

    finalView.m_memory = ImageViewMemory{
        .device = device,
        .viewCreateInfo = imageViewInfo,
        .view = view,
    };

    return std::make_unique<ImageView>(std::move(finalView));
}

auto szg_image::ImageView::view() const -> VkImageView { return m_memory.view; }

auto szg_image::ImageView::image() -> Image& { return *m_image; }

auto szg_image::ImageView::image() const -> Image const& { return *m_image; }

void szg_image::ImageView::recordTransitionBarriered(
    VkCommandBuffer const cmd, VkImageLayout const dst
)
{
    image().recordTransitionBarriered(
        cmd, dst, m_memory.viewCreateInfo.subresourceRange.aspectMask
    );
}

auto szg_image::ImageView::expectedLayout() const -> VkImageLayout
{
    return m_image != nullptr ? m_image->expectedLayout()
                              : VK_IMAGE_LAYOUT_UNDEFINED;
}

void szg_image::ImageView::destroy()
{
    bool leaked{false};
    if (m_memory.view != VK_NULL_HANDLE)
    {
        if (m_memory.device != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_memory.device, m_memory.view, nullptr);
        }
        else
        {
            leaked = true;
        }
    }

    if (leaked)
    {
        Warning(fmt::format(
            "Leak detected in image view. Device: {}. VkImageView: {}.",
            fmt::ptr(m_memory.device),
            fmt::ptr(m_memory.view)
        ));
    }

    m_image.reset();
    m_memory = ImageViewMemory{};
}
