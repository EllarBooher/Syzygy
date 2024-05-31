#pragma once

#include <vector>

#include <volk.h>
#include <string>
#include <span>
#include <optional>

/** Shorthand factory method for info structs, with reasonable defaults. */
namespace vkinit {
    VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);
    VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);
    VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
    
    VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);
    VkImageSubresourceLayers imageSubresourceLayers(
        VkImageAspectFlags aspectMask, 
        uint32_t mipLevel,
        uint32_t baseArrayLayer = 0,
        uint32_t baseArrayCount = 1
    );

    VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
    VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);
    VkSubmitInfo2 submitInfo(
        std::vector<VkCommandBufferSubmitInfo> const& cmdInfo,
        std::vector<VkSemaphoreSubmitInfo> const& waitSemaphoreInfo,
        std::vector<VkSemaphoreSubmitInfo> const& signalSemaphoreInfo
    );

    VkImageCreateInfo imageCreateInfo(
        VkFormat format, 
        VkImageLayout initialLayout,
        VkImageUsageFlags usageMask, 
        VkExtent3D extent
    );

    VkSamplerCreateInfo samplerCreateInfo(
        VkSamplerCreateFlags flags
        , VkBorderColor borderColor
        , VkFilter filter
        , VkSamplerAddressMode addressMode
    );

    VkImageViewCreateInfo imageViewCreateInfo(
        VkFormat format,
        VkImage image,
        VkImageAspectFlags aspectFlags
    );

    VkRenderingAttachmentInfo renderingAttachmentInfo(
        VkImageView view
        , VkImageLayout layout
        , std::optional<VkClearValue> clearValue = {}
    );

    VkRenderingInfo renderingInfo(
        VkExtent2D extent,
        std::span<VkRenderingAttachmentInfo const> colorAttachments,
        VkRenderingAttachmentInfo const* pDepthAttachment
    );

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(
        VkShaderStageFlagBits stage,
        VkShaderModule module,
        std::string const& entryPoint
    );

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo(
        VkPipelineLayoutCreateFlags flags
        , std::span<VkDescriptorSetLayout const> layouts
        , std::span<VkPushConstantRange const> ranges
    );
}