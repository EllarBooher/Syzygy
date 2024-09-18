#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/ui/dockinglayout.hpp"
#include "syzygy/ui/hud.hpp"
#include <functional>
#include <imgui.h>
#include <memory>
#include <optional>
#include <string>

namespace syzygy
{
struct DescriptorAllocator;
struct PlatformWindow;
struct SceneTexture;
} // namespace syzygy

namespace syzygy
{
struct SceneViewport
{
    bool focused;
    std::reference_wrapper<SceneTexture> texture;
    VkRect2D renderedSubregion;
};

struct UIOutputImage
{
    std::reference_wrapper<syzygy::SceneTexture> texture;
    VkRect2D renderedSubregion;
};

struct UILayer
{
public:
    UILayer(UILayer const&) = delete;
    auto operator=(UILayer const&) -> UILayer& = delete;

    UILayer(UILayer&&) noexcept;
    auto operator=(UILayer&&) noexcept -> UILayer&;
    ~UILayer();

private:
    UILayer() = default;
    void destroy();

public:
    // GLFW Detail: the backend installs any callbacks, so this can be called
    // after window callbacks are set (Such as cursor position/key event
    // callbacks).
    static auto create(
        VkInstance,
        VkPhysicalDevice,
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        VkExtent2D textureCapacity,
        uint32_t graphicsQueueFamily,
        VkQueue graphicsQueue,
        PlatformWindow& mainWindow,
        UIPreferences defaultPreferences
    ) -> std::optional<UILayer>;

    auto begin() -> syzygy::DockingLayout const&;

    [[nodiscard]] auto
    HUDMenuItem(std::string const& menu, std::string const& item) const -> bool;

    [[nodiscard]] auto sceneTextureLayout() const
        -> std::optional<VkDescriptorSetLayout>;

    auto sceneViewport(bool forceFocus) -> std::optional<SceneViewport>;

    void setCursorEnabled(bool enabled, bool breakWindowFocus = true);

    void end();

    auto recordDraw(VkCommandBuffer) -> std::optional<UIOutputImage>;

private:
    bool m_backendInitialized{false};

    bool m_reloadNecessary{false};
    UIPreferences m_currentPreferences{};
    UIPreferences m_defaultPreferences{};

    VkDevice m_device{VK_NULL_HANDLE};

    VkDescriptorPool m_imguiPool{VK_NULL_HANDLE};

    bool m_open{false};
    HUDState m_currentHUD{};
    DockingLayout m_currentDockingLayout{};

    // A sub-texture used by the UI backend to render a scene viewport.
    std::unique_ptr<SceneTexture> m_sceneTexture;
    // An opaque handle from the Vulkan backend that contains the scene texture
    ImTextureID m_imguiSceneTextureHandle{nullptr};

    std::unique_ptr<SceneTexture> m_outputTexture;
};
} // namespace syzygy