#include "texturedisplay.hpp"

#include "syzygy/assets.hpp"
#include "syzygy/core/immediate.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/images/image.hpp"
#include "syzygy/images/imageview.hpp"
#include "syzygy/initializers.hpp"
#include "syzygy/renderpass/renderpass.hpp"
#include "syzygy/ui/propertytable.hpp"
#include "syzygy/ui/uiwindow.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <misc/cpp/imgui_stdlib.h>
#include <regex>
#include <utility>

ui::TextureDisplay::TextureDisplay(TextureDisplay&& other) noexcept
{
    destroy();

    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_displayImage = std::move(other.m_displayImage);
    m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    m_imguiDescriptor = std::exchange(other.m_imguiDescriptor, VK_NULL_HANDLE);
}

ui::TextureDisplay::~TextureDisplay() { destroy(); }

auto ui::TextureDisplay::create(
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const transferQueue,
    ImmediateSubmissionQueue& submissionQueue,
    VkExtent2D const displaySize,
    VkFormat const format
) -> std::optional<TextureDisplay>
{
    if (ImGui::GetIO().BackendRendererUserData == nullptr)
    {
        SZG_ERROR("ImGui backend not initialized.");
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
                .extent = displaySize,
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
        SZG_ERROR("Failed to allocate image.");
        return std::nullopt;
    }
    szg_image::ImageView& texture{*textureResult.value()};

    submissionQueue.immediateSubmit(
        transferQueue,
        [&](VkCommandBuffer const cmd)
    {
        renderpass::recordClearColorImage(
            cmd, texture.image(), renderpass::COLOR_BLACK_OPAQUE
        );

        texture.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }
    );

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

auto ui::TextureDisplay::uiRender(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    VkCommandBuffer const cmd,
    std::span<szg_assets::AssetRef<szg_image::Image> const> const textures
) -> TextureDisplay::UIResult
{
    ui::UIWindow const sceneViewport{
        ui::UIWindow::beginDockable(title, dockNode)
    };

    TextureDisplay::UIResult result{};

    if (!sceneViewport.open)
    {
        return result;
    }

    result.loadTexturesRequested = ImGui::Button("Open Files to Load Textures");

    auto clearImageCallback = [&]()
    {
        if (m_displayImage == nullptr)
        {
            return;
        }
        szg_image::ImageView& displayImage{*m_displayImage};

        renderpass::recordClearColorImage(
            cmd, displayImage.image(), renderpass::COLOR_BLACK_OPAQUE
        );

        displayImage.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    };

    auto copyIntoImageCallback = [&](szg_image::Image& other)
    {
        if (m_displayImage == nullptr)
        {
            return;
        }
        szg_image::ImageView& displayImage{*m_displayImage};

        displayImage.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        other.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
        );

        szg_image::Image::recordCopyEntire(
            cmd, other, displayImage.image(), VK_IMAGE_ASPECT_COLOR_BIT
        );

        displayImage.recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    };

    {
        std::string const& defaultLabel{"None"};
        std::string const& previewLabel{
            m_cachedMetadata.has_value()
                ? m_cachedMetadata.value().displayName.c_str()
                : defaultLabel.c_str()
        };

        {
            ImGui::BeginDisabled(textures.empty());

            if (ImGui::BeginListBox(
                    "##textureSelection", ImVec2{-FLT_MIN, 0.0}
                ))
            {
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputTextWithHint(
                    "##searchBar", "Search", &m_nameFilter
                );
                std::regex const searchPattern{
                    m_nameFilter, std::regex_constants::icase
                };

                if (ImGui::Selectable(
                        defaultLabel.c_str(), !m_cachedMetadata.has_value()
                    ))
                {
                    clearImageCallback();

                    m_cachedMetadata = std::nullopt;
                }

                for (szg_assets::Asset<szg_image::Image> const& texture :
                     textures)
                {
                    szg_assets::AssetMetadata const& metaData{texture.metadata};

                    if (!std::regex_search(metaData.displayName, searchPattern))
                    {
                        continue;
                    }

                    bool const selected{
                        m_cachedMetadata.has_value()
                        && metaData.id == m_cachedMetadata.value().id
                    };

                    if (ImGui::Selectable(
                            fmt::format(
                                "{}##{}",
                                metaData.displayName,
                                static_cast<uint64_t>(metaData.id)
                            )
                                .c_str(),
                            selected
                        ))
                    {
                        if (texture.data != nullptr)
                        {
                            copyIntoImageCallback(*texture.data);
                        }

                        m_cachedMetadata = metaData;
                    }
                }
                ImGui::EndListBox();
            }

            ImGui::EndDisabled();
        }
    }
    {
        if (m_cachedMetadata.has_value())
        {
            PropertyTable table{PropertyTable::begin()};
            szg_assets::AssetMetadata const& metadata{m_cachedMetadata.value()};

            table.rowReadOnlyTextInput(
                "Display Name", metadata.displayName, false
            );
            table.rowReadOnlyTextInput(
                "Global Identifier",
                fmt::format("{:x}", static_cast<uint64_t>(metadata.id)),
                false
            );
            table.rowReadOnlyTextInput(
                "Local Path on Disk", metadata.fileLocalPath, false
            );

            table.end();
        }
    }

    {
        glm::vec2 const contentExtent{sceneViewport.screenRectangle.size()};
        double const imageHeight{
            m_displayImage->image().aspectRatio().value_or(1.0)
            * contentExtent.x
        };

        ImGui::Image(
            reinterpret_cast<ImTextureID>(m_imguiDescriptor),
            ImVec2{contentExtent.x, static_cast<float>(imageHeight)},
            ImVec2{0.0, 0.0},
            ImVec2{1.0, 1.0},
            ImVec4{1.0F, 1.0F, 1.0F, 1.0F},
            ImVec4{0.0F, 0.0F, 0.0F, 0.0F}
        );
    }

    return result;
}

void ui::TextureDisplay::destroy()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device, m_sampler, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_displayImage.reset();
    m_sampler = VK_NULL_HANDLE;
    m_imguiDescriptor = VK_NULL_HANDLE;
}
