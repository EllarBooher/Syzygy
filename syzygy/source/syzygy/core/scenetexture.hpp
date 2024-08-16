#pragma once

#include "syzygy/renderer/imageview.hpp"
#include "syzygy/vulkanusage.hpp"
#include <memory>
#include <optional>
#include <utility>

namespace syzygy
{
class DescriptorAllocator;
}

namespace syzygy
{
struct SceneTexture
{
    SceneTexture() = delete;

    SceneTexture(SceneTexture const&) = delete;
    SceneTexture& operator=(SceneTexture const&) = delete;

    SceneTexture(SceneTexture&&) noexcept;
    SceneTexture& operator=(SceneTexture&&) noexcept;

    ~SceneTexture();

    // The texture is allocated once. It is expected to render into a portion of
    // it, so windows can be resized without reallocation.
    // Thus the texture should be large enough to handle as large as the window
    // is expected to get.
    static auto create(
        VkDevice,
        VmaAllocator,
        syzygy::DescriptorAllocator&,
        VkExtent2D textureMax,
        VkFormat format
    ) -> std::optional<SceneTexture>;

    auto sampler() const -> VkSampler;
    auto texture() -> syzygy::ImageView&;
    auto texture() const -> syzygy::ImageView const&;

    // A descriptor set that contains just this image in binding 0 for compute
    // shaders.
    auto singletonDescriptor() const -> VkDescriptorSet;
    auto singletonLayout() const -> VkDescriptorSetLayout;

    // The descriptor set that ImGui's backend allocates, the layout is opaque.
    auto imguiDescriptor() const -> VkDescriptorSet;

private:
    SceneTexture(
        VkDevice device,
        VkSampler sampler,
        std::unique_ptr<syzygy::ImageView> texture,
        VkDescriptorSetLayout singletonLayout,
        VkDescriptorSet singletonSet,
        VkDescriptorSet imguiDescriptor
    )
        : m_device{device}
        , m_sampler{sampler}
        , m_texture{std::move(texture)}
        , m_singletonDescriptorLayout{singletonLayout}
        , m_singletonDescriptor{singletonSet}
        , m_imguiDescriptor{imguiDescriptor}
    {
    }

    void destroy() noexcept;

    // The device used to create this.
    VkDevice m_device{VK_NULL_HANDLE};

    VkSampler m_sampler{VK_NULL_HANDLE};
    std::unique_ptr<syzygy::ImageView> m_texture{};

    VkDescriptorSetLayout m_singletonDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_singletonDescriptor{VK_NULL_HANDLE};

    VkDescriptorSet m_imguiDescriptor{VK_NULL_HANDLE};
};

struct SceneViewport
{
    VkRect2D rect;
};
} // namespace syzygy