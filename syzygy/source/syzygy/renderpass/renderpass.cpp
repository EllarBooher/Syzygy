#include "renderpass.hpp"
#include "syzygy/images.hpp"
#include "syzygy/initializers.hpp"

void renderpass::recordClearDepthImage(
    VkCommandBuffer const cmd,
    AllocatedImage& depth,
    VkClearDepthStencilValue const value
)
{
    depth.recordTransitionBarriered(cmd, VK_IMAGE_LAYOUT_GENERAL);

    VkImageSubresourceRange const range{
        vkinit::imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)
    };
    vkCmdClearDepthStencilImage(
        cmd, depth.image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range
    );
}

void renderpass::recordClearColorImage(
    VkCommandBuffer const cmd,
    AllocatedImage& color,
    VkClearColorValue const value
)
{
    color.recordTransitionBarriered(cmd, VK_IMAGE_LAYOUT_GENERAL);

    VkImageSubresourceRange const range{
        vkinit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
    };
    vkCmdClearColorImage(
        cmd, color.image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range
    );
}
