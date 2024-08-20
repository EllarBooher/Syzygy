#pragma once

#include "syzygy/platform/vulkanusage.hpp"
namespace syzygy
{
struct Image;
} // namespace syzygy

// All of these methods transition inputs, but not outputs
namespace syzygy
{
float constexpr DEPTH_FAR{0};
VkClearDepthStencilValue constexpr DEPTH_FAR_STENCIL_NONE{
    .depth = 0.0, .stencil = 0
};
VkClearColorValue constexpr COLOR_BLACK_OPAQUE{0.0, 0.0, 0.0, 1.0};

void recordClearDepthImage(VkCommandBuffer, Image&, VkClearDepthStencilValue);
void recordClearColorImage(VkCommandBuffer, Image&, VkClearColorValue);
} // namespace syzygy