#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/imageview.hpp"
#include <memory>
#include <optional>
#include <utility>

namespace syzygy
{
struct DescriptorAllocator;
} // namespace syzygy

namespace syzygy
{
struct SceneTexture
{
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
        VkExtent2D textureMax,
        VkFormat colorFormat,
        VkFormat depthFormat
    ) -> std::optional<SceneTexture>;

    [[nodiscard]] auto sampler() const -> VkSampler;

    auto texture() -> ImageView&;
    [[nodiscard]] auto texture() const -> ImageView const&;

    // A descriptor set that contains just this image in binding 0 for compute
    // shaders.
    // layout(rgba16, binding = 0) uniform image2D image;
    [[nodiscard]] auto singletonDescriptor() const -> VkDescriptorSet;
    [[nodiscard]] auto singletonLayout() const -> VkDescriptorSetLayout;

    // layout(binding = 0) uniform image2D image;
    // layout(binding = 1) uniform sampler2D fragmentDepth;
    [[nodiscard]] auto combinedDescriptor() const -> VkDescriptorSet;
    [[nodiscard]] auto combinedDescriptorLayout() const
        -> VkDescriptorSetLayout;

private:
    SceneTexture() = default;

    void destroy() noexcept;

    // The device used to create this.
    VkDevice m_device{VK_NULL_HANDLE};

    std::unique_ptr<DescriptorAllocator> m_descriptorPool{};

    VkSampler m_colorSampler{VK_NULL_HANDLE};
    VkSampler m_depthSampler{VK_NULL_HANDLE};
    std::unique_ptr<syzygy::ImageView> m_color{};
    std::unique_ptr<syzygy::ImageView> m_depth{};

    VkDescriptorSetLayout m_singletonDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_singletonDescriptor{VK_NULL_HANDLE};

    VkDescriptorSetLayout m_combinedDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_combinedDescriptor{VK_NULL_HANDLE};
};
} // namespace syzygy