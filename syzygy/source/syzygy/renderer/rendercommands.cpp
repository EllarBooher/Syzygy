#include "rendercommands.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"

namespace syzygy
{
void recordClearDepthImage(
    VkCommandBuffer const cmd,
    Image& depth,
    VkClearDepthStencilValue const value
)
{
    depth.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT
    );

    VkImageSubresourceRange const range{
        imageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)
    };
    vkCmdClearDepthStencilImage(
        cmd, depth.image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range
    );
}

void recordClearColorImage(
    VkCommandBuffer const cmd, Image& color, VkClearColorValue const value
)
{
    color.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkImageSubresourceRange const range{
        imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
    };
    vkCmdClearColorImage(
        cmd, color.image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range
    );
}
} // namespace syzygy