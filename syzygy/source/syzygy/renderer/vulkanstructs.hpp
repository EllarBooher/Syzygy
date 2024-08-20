#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include <optional>
#include <span>
#include <string>
#include <vector>

// Shorthand factory methods for data-holding Vulkan structs, with reasonable
// defaults.

namespace syzygy
{
auto fenceCreateInfo(VkFenceCreateFlags flags = 0) -> VkFenceCreateInfo;
auto semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0)
    -> VkSemaphoreCreateInfo;
auto commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0)
    -> VkCommandBufferBeginInfo;

auto imageSubresourceRange(VkImageAspectFlags aspectMask)
    -> VkImageSubresourceRange;
auto imageSubresourceLayers(
    VkImageAspectFlags aspectMask,
    uint32_t mipLevel = 0,
    uint32_t baseArrayLayer = 0,
    uint32_t baseArrayCount = 1
) -> VkImageSubresourceLayers;

auto semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
    -> VkSemaphoreSubmitInfo;
auto commandBufferSubmitInfo(VkCommandBuffer cmd) -> VkCommandBufferSubmitInfo;
auto submitInfo(
    std::vector<VkCommandBufferSubmitInfo> const& cmdInfo,
    std::vector<VkSemaphoreSubmitInfo> const& waitSemaphoreInfo,
    std::vector<VkSemaphoreSubmitInfo> const& signalSemaphoreInfo
) -> VkSubmitInfo2;

auto imageCreateInfo(
    VkFormat format,
    VkImageLayout initialLayout,
    VkImageUsageFlags usageMask,
    VkExtent3D extent,
    VkImageTiling tiling
) -> VkImageCreateInfo;

auto samplerCreateInfo(
    VkSamplerCreateFlags flags,
    VkBorderColor borderColor,
    VkFilter filter,
    VkSamplerAddressMode addressMode
) -> VkSamplerCreateInfo;

auto imageViewCreateInfo(
    VkFormat format, VkImage image, VkImageAspectFlags aspectFlags
) -> VkImageViewCreateInfo;

auto renderingAttachmentInfo(
    VkImageView view,
    VkImageLayout layout,
    std::optional<VkClearValue> clearValue = {}
) -> VkRenderingAttachmentInfo;

auto renderingInfo(
    VkRect2D drawRect,
    std::span<VkRenderingAttachmentInfo const> colorAttachments,
    VkRenderingAttachmentInfo const* pDepthAttachment
) -> VkRenderingInfo;

auto pipelineShaderStageCreateInfo(
    VkShaderStageFlagBits stage,
    VkShaderModule module,
    std::string const& entryPoint
) -> VkPipelineShaderStageCreateInfo;

auto pipelineLayoutCreateInfo(
    VkPipelineLayoutCreateFlags flags,
    std::span<VkDescriptorSetLayout const> layouts,
    std::span<VkPushConstantRange const> ranges
) -> VkPipelineLayoutCreateInfo;

} // namespace syzygy