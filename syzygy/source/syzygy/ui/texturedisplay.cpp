#include "texturedisplay.hpp"

#include "syzygy/core/deletionqueue.hpp"
#include "syzygy/descriptors.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/images/image.hpp"
#include "syzygy/images/imageoperations.hpp"
#include "syzygy/images/imageview.hpp"
#include "syzygy/initializers.hpp"
#include "syzygy/ui/uiwindow.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <utility>

ui::TextureDisplay::TextureDisplay(TextureDisplay&& other) noexcept
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_image = std::move(other.m_image);
    m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    m_imguiDescriptor = std::exchange(other.m_imguiDescriptor, VK_NULL_HANDLE);
}

ui::TextureDisplay::~TextureDisplay() { destroy(); }

auto ui::TextureDisplay::create(
    VkDevice const device,
    VmaAllocator const allocator,
    VkExtent2D const textureMax,
    VkFormat const format
) -> std::optional<TextureDisplay>
{
    if (ImGui::GetIO().BackendRendererUserData == nullptr)
    {
        Error("ImGui backend not initialized.");
        return std::nullopt;
    }

    // This image is used in
    // 1) ImGui graphics shaders as a descriptor
    // 2) Copied into from the textures we wish to draw
    VkImageUsageFlags const colorUsage{
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
    };

    std::optional<std::unique_ptr<szg_image::ImageView>> textureResult{
        szg_image::ImageView::allocate(
            device,
            allocator,
            szg_image::ImageAllocationParameters{
                .extent = textureMax,
                .format = format,
                .usageFlags = colorUsage,
            },
            szg_image::ImageViewAllocationParameters{
                .subresourceRange =
                    vkinit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
            }
        )
    };

    if (!textureResult.has_value())
    {
        Error("Failed to allocate image.");
        return std::nullopt;
    }
    szg_image::ImageView& texture{*textureResult.value()};

    VkSamplerCreateInfo const samplerInfo{vkinit::samplerCreateInfo(
        0,
        VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        VK_FILTER_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
    )};

    VkSampler sampler{VK_NULL_HANDLE};
    TRY_VK(
        vkCreateSampler(device, &samplerInfo, nullptr, &sampler),
        "Failed to allocate sampler.",
        std::nullopt
    );

    VkDescriptorSet const imguiDescriptor = ImGui_ImplVulkan_AddTexture(
        sampler, texture.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    return TextureDisplay{
        device, std::move(textureResult).value(), sampler, imguiDescriptor
    };
}

void ui::TextureDisplay::uiRender(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    VkCommandBuffer const cmd,
    szg_image::Image& sourceTexture
)
{
    ui::UIWindow const sceneViewport{
        ui::UIWindow::beginDockable(title, dockNode)
    };

    if (!sceneViewport.open)
    {
        return;
    }

    m_image->recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    sourceTexture.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
    );

    szg_image::Image::recordCopyEntire(
        cmd, sourceTexture, m_image->image(), VK_IMAGE_ASPECT_COLOR_BIT
    );

    m_image->recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    glm::vec2 const contentExtent{sceneViewport.screenRectangle.size()};
    double const imageHeight{
        m_image->image().aspectRatio().value_or(1.0) * contentExtent.x
    };

    ImGui::Image(
        reinterpret_cast<ImTextureID>(m_imguiDescriptor),
        ImVec2{contentExtent.x, static_cast<float>(imageHeight)},
        ImVec2{0.0, 0.0},
        ImVec2{1.0, 1.0},
        ImVec4{1.0F, 1.0F, 1.0F, 1.0F},
        ImVec4{0.0F, 0.0F, 0.0F, 0.0F}
    );

    return;
}

void ui::TextureDisplay::destroy()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device, m_sampler, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_image.reset();
    m_sampler = VK_NULL_HANDLE;
    m_imguiDescriptor = VK_NULL_HANDLE;
}
