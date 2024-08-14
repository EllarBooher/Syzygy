#pragma once

#include "syzygy/vulkanusage.hpp"
namespace szg_renderer
{
struct Image;
}

// All of these methods transition inputs, but not outputs
namespace renderpass
{
float constexpr DEPTH_FAR{0};
VkClearDepthStencilValue constexpr DEPTH_FAR_STENCIL_NONE{
    .depth = 0.0, .stencil = 0
};
VkClearColorValue constexpr COLOR_BLACK_OPAQUE{0.0, 0.0, 0.0, 1.0};

void recordClearDepthImage(
    VkCommandBuffer, szg_renderer::Image&, VkClearDepthStencilValue
);
void recordClearColorImage(
    VkCommandBuffer, szg_renderer::Image&, VkClearColorValue
);
} // namespace renderpass