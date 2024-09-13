#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/imageview.hpp"
#include <memory>
#include <optional>
#include <utility>

namespace syzygy
{
class DescriptorAllocator;
} // namespace syzygy

namespace syzygy
{
struct SceneTexture
{
    SceneTexture() = delete;

    SceneTexture(SceneTexture const&) = delete;
    auto operator=(SceneTexture const&) -> SceneTexture& = delete;

    SceneTexture(SceneTexture&&) noexcept;
    auto operator=(SceneTexture&&) noexcept -> SceneTexture&;

    ~SceneTexture();

    // The texture is allocated once. It is expected to render into a portion of
    // it, so windows can be resized without reallocation.
    // Thus the texture should be large enough to handle as large as the window
    // is expected to get.
    static auto create(
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        VkExtent2D textureMax,
        VkFormat format
    ) -> std::optional<SceneTexture>;

    [[nodiscard]] auto sampler() const -> VkSampler;

    auto texture() -> ImageView&;
    [[nodiscard]] auto texture() const -> ImageView const&;

    // A descriptor set that contains just this image in binding 0 for compute
    // shaders.
    [[nodiscard]] auto singletonDescriptor() const -> VkDescriptorSet;
    [[nodiscard]] auto singletonLayout() const -> VkDescriptorSetLayout;

private:
    SceneTexture(
        VkDevice device,
        VkSampler sampler,
        std::unique_ptr<ImageView> texture,
        VkDescriptorSetLayout singletonLayout,
        VkDescriptorSet singletonSet
    )
        : m_device{device}
        , m_sampler{sampler}
        , m_texture{std::move(texture)}
        , m_singletonDescriptorLayout{singletonLayout}
        , m_singletonDescriptor{singletonSet}
    {
    }

    void destroy() noexcept;

    // The device used to create this.
    VkDevice m_device{VK_NULL_HANDLE};

    VkSampler m_sampler{VK_NULL_HANDLE};
    std::unique_ptr<syzygy::ImageView> m_texture{};

    VkDescriptorSetLayout m_singletonDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_singletonDescriptor{VK_NULL_HANDLE};
};
} // namespace syzygy