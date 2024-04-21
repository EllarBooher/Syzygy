#pragma once

#include <volk.h>
#include "enginetypes.hpp"

namespace vkutil {
    /**
        Transitions the layout of an image, putting in a full memory barrier for all
    */
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    AllocatedImage allocateImage(
        VmaAllocator allocator,
        VkDevice device,
        VkExtent3D extent, 
        VkFormat format, 
        VkImageAspectFlags viewFlags,
        VkImageUsageFlags usageMask
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
}