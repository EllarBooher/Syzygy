#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/image.hpp"
#include <memory>
#include <optional>

namespace syzygy
{
struct ImageViewAllocationParameters
{
    // Views use the image's format, or optionally an override that must be
    // compatible according to the compatibilities listed in chapter 48: Formats
    // of the Vulkan Spec.
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#formats-compatibility-classes
    // https://vkdoc.net/chapters/formats#formats-compatibility-classes
    std::optional<VkFormat> formatOverride;
    VkImageViewCreateFlags flags{0};
    VkImageViewType viewType{VK_IMAGE_VIEW_TYPE_2D};
    VkImageSubresourceRange subresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
    VkComponentMapping components{
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY
    };
};

struct ImageViewMemory
{
    VkDevice device{VK_NULL_HANDLE};

    VkImageViewCreateInfo viewCreateInfo{};
    VkImageView view{VK_NULL_HANDLE};
};

struct ImageView
{
public:
    auto operator=(ImageView&&) -> ImageView& = delete;

    ImageView(ImageView const&) = delete;
    auto operator=(ImageView const&) -> ImageView& = delete;

    ImageView(ImageView&&) noexcept;
    ~ImageView();

private:
    ImageView() = default;
    void destroy();

public:
    static auto allocate(
        VkDevice,
        VmaAllocator,
        ImageAllocationParameters const&,
        ImageViewAllocationParameters const& viewParameters
    ) -> std::optional<std::unique_ptr<ImageView>>;

    static auto allocate(
        VkDevice,
        VmaAllocator,
        Image&&,
        ImageViewAllocationParameters const& viewParameters
    ) -> std::optional<std::unique_ptr<ImageView>>;

    // WARNING: Do not destroy this image view.
    auto view() -> VkImageView;

    auto image() -> Image&;
    [[nodiscard]] auto image() const -> Image const&;

    // Transitions the underlying image, according to the aspect(s) of the view.
    void recordTransitionBarriered(VkCommandBuffer, VkImageLayout);

    [[nodiscard]] auto expectedLayout() const -> VkImageLayout;

private:
    // So far, images and views are 1 to 1. In the future this could be a
    // shared_ptr, or we make a new image view class.
    std::unique_ptr<Image> m_image{};
    ImageViewMemory m_memory{};
};
} // namespace syzygy