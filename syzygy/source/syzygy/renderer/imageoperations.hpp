#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include <glm/vec2.hpp>
#include <optional>

namespace syzygy
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

// Transitions the layout of an image, putting in a full memory barrier
// TODO: track image layout on images themselves, and make this automatic
void transitionImage(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkImageAspectFlags aspects
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

void recordCopyImageToImage(
    VkCommandBuffer cmd,
    VkImage source,
    VkImage destination,
    VkRect2D src,
    VkRect2D dst
);

auto aspectRatio(VkExtent2D) -> std::optional<double>;
auto aspectRatio(glm::vec2) -> std::optional<double>;
} // namespace syzygy