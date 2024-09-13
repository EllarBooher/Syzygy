#pragma once

#include "syzygy/editor/editorconfig.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/descriptors.hpp"
#include "syzygy/renderer/shaders.hpp"
#include <array>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace syzygy
{
struct Swapchain
{
public:
    Swapchain(Swapchain const&) = delete;
    auto operator=(Swapchain const&) -> Swapchain& = delete;

    auto operator=(Swapchain&&) noexcept -> Swapchain&;
    Swapchain(Swapchain&&) noexcept;
    ~Swapchain();

private:
    Swapchain() = default;
    void destroy();

public:
    static auto create(
        VkPhysicalDevice,
        VkDevice,
        VkSurfaceKHR,
        glm::u16vec2 extent,
        std::optional<VkSwapchainKHR> old
    ) -> std::optional<Swapchain>;

    [[nodiscard]] auto swapchain() const -> VkSwapchainKHR;
    [[nodiscard]] auto images() const -> std::span<VkImage const>;
    [[nodiscard]] auto imageViews() const -> std::span<VkImageView const>;
    [[nodiscard]] auto extent() const -> VkExtent2D;

    [[nodiscard]] auto imageDescriptors() const
        -> std::span<VkDescriptorSet const>;
    [[nodiscard]] auto eotfPipeline(GammaTransferFunction) const
        -> ShaderObjectReflected const&;
    [[nodiscard]] auto eotfPipelineLayout() const -> VkPipelineLayout;

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_imageFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D m_extent{};

    std::vector<VkImage> m_images{};
    std::vector<VkImageView> m_imageViews{};

    // Begin gamma correction transfer shaders

    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator{};

    // For each image, we store a descriptor containing a single binding as the
    // read-write image source
    std::vector<VkDescriptorSet> m_imageSingletonDescriptors{};
    VkDescriptorSetLayout m_imageSingletonLayout{VK_NULL_HANDLE};
    std::array<
        ShaderObjectReflected,
        static_cast<size_t>(GammaTransferFunction::MAX)>
        m_oetfPipelines{
            ShaderObjectReflected::makeInvalid(),
            ShaderObjectReflected::makeInvalid()
        };
    VkPipelineLayout m_oetfPipelineLayout{VK_NULL_HANDLE};

    // End gamma correction transfer shaders
};
} // namespace syzygy