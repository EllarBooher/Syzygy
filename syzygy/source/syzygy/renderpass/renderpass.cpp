#include "renderpass.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"

void renderpass::recordClearDepthImage(
    VkCommandBuffer const cmd,
    szg_renderer::Image& depth,
    VkClearDepthStencilValue const value
)
{
    depth.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT
    );

    VkImageSubresourceRange const range{
        szg_renderer::imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)
    };
    vkCmdClearDepthStencilImage(
        cmd, depth.image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range
    );
}

void renderpass::recordClearColorImage(
    VkCommandBuffer const cmd,
    szg_renderer::Image& color,
    VkClearColorValue const value
)
{
    color.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkImageSubresourceRange const range{
        szg_renderer::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
    };
    vkCmdClearColorImage(
        cmd, color.image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range
    );
}
