#pragma once

#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>
#include <optional>
#include <span>
#include <string>

#include "syzygy/assets/assetsfwd.hpp"

namespace syzygy
{
struct Scene;
struct EditorConfiguration;
struct RingBuffer;
struct Mesh;
} // namespace syzygy

namespace syzygy
{
// "Pure" widgets that require the function to be called every time it needs to
// be rendered

void editorConfigurationWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    EditorConfiguration& value,
    EditorConfiguration const& defaults
);

void performanceWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    RingBuffer const& values,
    float& targetFPS
);
void sceneControlsWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    syzygy::Scene& scene,
    std::span<AssetPtr<Mesh> const> meshes
);

template <typename T> struct WindowResult
{
    bool focused;
    T payload;
};

// The returned value indicates the extent from (0,0) to (x,y) that will be read
// from the the syzygy texture by ImGui when the final image is composited.
auto sceneViewportWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    std::optional<UIRectangle> maximizeArea,
    ImTextureID sceneTexture,
    ImVec2 sceneTextureMax,
    bool focused
) -> WindowResult<std::optional<VkRect2D>>;
} // namespace syzygy