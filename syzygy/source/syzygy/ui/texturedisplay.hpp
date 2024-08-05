#pragma once

#include "syzygy/assets.hpp"
#include "syzygy/core/uuid.hpp"
#include "syzygy/vulkanusage.hpp"
#include <imgui.h>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace szg_image
{
struct Image;
struct ImageView;
} // namespace szg_image
struct DescriptorAllocator;
struct ImmediateSubmissionQueue;

namespace ui
{
// A UI widget that displays the color aspect of an image
struct TextureDisplay
{
public:
    TextureDisplay(TextureDisplay&&) noexcept;
    ~TextureDisplay();

    TextureDisplay() = delete;
    TextureDisplay& operator=(TextureDisplay&&) = delete;

    TextureDisplay(TextureDisplay const&) = delete;
    TextureDisplay& operator=(TextureDisplay const&) = delete;

    // imageCapacity and imageFormat can be set to ensure compatibility with the
    // textures that will be copied later on
    static auto create(
        VkDevice,
        VmaAllocator,
        VkQueue transferQueue,
        ImmediateSubmissionQueue&,
        VkExtent2D displaySize,
        VkFormat imageFormat
    ) -> std::optional<TextureDisplay>;

    // Records a copy of the supplied image, and draws the UI window.
    void uiRender(
        std::string const& title,
        std::optional<ImGuiID> dockNode,
        VkCommandBuffer,
        std::span<szg_assets::AssetRef<szg_image::Image> const> textures
    );

private:
    void destroy();

    TextureDisplay(
        VkDevice device,
        std::unique_ptr<szg_image::ImageView> imageView,
        VkSampler sampler,
        VkDescriptorSet imguiDescriptor
    )
        : m_device{device}
        , m_image{std::move(imageView)}
        , m_sampler{sampler}
        , m_imguiDescriptor{imguiDescriptor}
    {
    }

    VkDevice m_device{VK_NULL_HANDLE};

    std::unique_ptr<szg_image::ImageView> m_image{};
    VkSampler m_sampler{VK_NULL_HANDLE};
    VkDescriptorSet m_imguiDescriptor{VK_NULL_HANDLE};

    std::optional<szg_assets::AssetMetadata> m_cachedMetadata{};
};
} // namespace ui