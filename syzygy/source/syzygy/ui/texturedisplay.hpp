#pragma once

#include "syzygy/assets/assets.hpp"
#include "syzygy/core/uuid.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/imageview.hpp"
#include <imgui.h>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace syzygy
{
struct Image;
struct ImmediateSubmissionQueue;
} // namespace syzygy

namespace syzygy
{
// A UI widget that displays the color aspect of an image
struct TextureDisplay
{
public:
    TextureDisplay(TextureDisplay&&) noexcept;
    ~TextureDisplay();

    TextureDisplay() = delete;
    auto operator=(TextureDisplay&&) -> TextureDisplay& = delete;

    TextureDisplay(TextureDisplay const&) = delete;
    auto operator=(TextureDisplay const&) -> TextureDisplay& = delete;

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

    struct UIResult
    {
        bool loadTexturesRequested{false};
    };
    // Records a copy of the supplied image, and draws the UI window.
    auto uiRender(
        std::string const& title,
        std::optional<ImGuiID> dockNode,
        VkCommandBuffer,
        std::span<syzygy::AssetRef<syzygy::Image> const> textures
    ) -> UIResult;

private:
    void destroy();

    TextureDisplay(
        VkDevice device,
        std::unique_ptr<syzygy::ImageView> imageView,
        VkSampler sampler,
        VkDescriptorSet imguiDescriptor
    )
        : m_device{device}
        , m_displayImage{std::move(imageView)}
        , m_sampler{sampler}
        , m_imguiDescriptor{imguiDescriptor}
    {
    }

    VkDevice m_device{VK_NULL_HANDLE};

    std::unique_ptr<syzygy::ImageView> m_displayImage{};
    VkSampler m_sampler{VK_NULL_HANDLE};
    VkDescriptorSet m_imguiDescriptor{VK_NULL_HANDLE};

    std::string m_nameFilter{};
    std::optional<syzygy::AssetMetadata> m_cachedMetadata{};
};
} // namespace syzygy