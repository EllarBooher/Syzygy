#pragma once

#include <volk.h>
#include <optional>
#include "enginetypes.hpp"

namespace vkutil {
    // Transitions the layout of an image, putting in a full memory barrier
    //TODO: track image layout on images themselves, and make this automatic
    void transitionImage(
        VkCommandBuffer cmd
        , VkImage image
        , VkImageLayout oldLayout
        , VkImageLayout newLayout
        , VkImageAspectFlags aspects
    );

    /**
        Copies all RGBA of an image to another.
        Assumes source is VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL and destination is VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
    */
    void recordCopyImageToImage(
        VkCommandBuffer cmd,
        VkImage source,
        VkImage destination,
        VkExtent3D srcSize,
        VkExtent3D dstSize
    );

    // Copies a color image, with an assumed depth of 1.
    void recordCopyImageToImage(
        VkCommandBuffer cmd,
        VkImage source,
        VkImage destination,
        VkExtent2D srcSize,
        VkExtent2D dstSize
    );

    double aspectRatio(VkExtent2D extent);
}

struct AllocatedImage {
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VkImage image{ VK_NULL_HANDLE };
    VkImageView imageView{ VK_NULL_HANDLE };
    VkExtent3D imageExtent{};
    VkFormat imageFormat{ VK_FORMAT_UNDEFINED };

    void cleanup(VkDevice device, VmaAllocator allocator)
    {
        vkDestroyImageView(device, imageView, nullptr);
        vmaDestroyImage(allocator, image, allocation);
    }

    VkExtent2D extent2D() const
    {
        return VkExtent2D{
            .width{ imageExtent.width },
            .height{ imageExtent.height },
        };
    }

    // The value will be 0.0/inf/NaN for an image without valid bounds.
    double aspectRatio() const
    {
        return vkutil::aspectRatio(extent2D());
    }

    static std::optional<AllocatedImage> allocate(
        VmaAllocator allocator,
        VkDevice device,
        VkExtent3D extent,
        VkFormat format,
        VkImageAspectFlags viewFlags,
        VkImageUsageFlags usageMask
    );
};