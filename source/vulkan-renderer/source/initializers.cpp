#include "initializers.hpp"

VkFenceCreateInfo vkinit::fenceCreateInfo(VkFenceCreateFlags flags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
    };
}

VkSemaphoreCreateInfo vkinit::semaphoreCreateInfo(VkSemaphoreCreateFlags flags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
    };
}

VkCommandBufferBeginInfo vkinit::commandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = flags,
        .pInheritanceInfo = nullptr,
    };
}

VkImageSubresourceRange vkinit::imageSubresourceRange(VkImageAspectFlags aspectMask)
{
    return {
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
}

VkImageSubresourceLayers vkinit::imageSubresourceLayers(VkImageAspectFlags aspectMask, uint32_t mipLevel, uint32_t baseArrayLayer, uint32_t baseArrayCount)
{
    return {
      .aspectMask = aspectMask,
      .mipLevel = mipLevel,
      .baseArrayLayer = baseArrayLayer,
      .layerCount = baseArrayCount,
    };
}

VkSemaphoreSubmitInfo vkinit::semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stageMask,
        .deviceIndex = 0, // Assume single device, at index 0.
    };
}

VkCommandBufferSubmitInfo vkinit::commandBufferSubmitInfo(VkCommandBuffer cmd)
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmd,
        .deviceMask = 0,
    };
}

VkSubmitInfo2 vkinit::submitInfo(
    std::vector<VkCommandBufferSubmitInfo> const& cmdInfo, 
    std::vector<VkSemaphoreSubmitInfo> const& waitSemaphoreInfo,
    std::vector<VkSemaphoreSubmitInfo> const& signalSemaphoreInfo
)
{
    return {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,

        .flags = 0,

        .waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreInfo.size()),
        .pWaitSemaphoreInfos = waitSemaphoreInfo.data(),

        .commandBufferInfoCount = static_cast<uint32_t>(cmdInfo.size()),
        .pCommandBufferInfos = cmdInfo.data(),

        .signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfo.size()),
        .pSignalSemaphoreInfos = signalSemaphoreInfo.data(),
    };
}

VkImageCreateInfo vkinit::imageCreateInfo(VkFormat format, VkImageLayout initialLayout, VkImageUsageFlags usageMask, VkExtent3D extent)
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,

        .imageType = VK_IMAGE_TYPE_2D,
        
        .format = format,
        .extent = extent,

        .mipLevels = 1,
        .arrayLayers = 1,

        .samples = VK_SAMPLE_COUNT_1_BIT,

        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usageMask,

        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,

        .initialLayout = initialLayout,
    };
}

VkImageViewCreateInfo vkinit::imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,

        .flags = 0,

        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components{},
        .subresourceRange = vkinit::imageSubresourceRange(aspectFlags)
    };
}

