#pragma once

#include "syzygy/core/scenetexture.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>
#include <optional>
#include <string>

namespace szg_scene
{
struct Scene;
}
struct MeshAssetLibrary;
struct RingBuffer;

// A collection of szg_ui widgets that are free standing functions.

namespace szg_ui
{
void performanceWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    RingBuffer const& values,
    float& targetFPS
);
void sceneControlsWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    szg_scene::Scene& szg_scene,
    MeshAssetLibrary const& meshes
);

template <typename T> struct WindowResult
{
    bool focused;
    T payload;
};

// The returned value indicates the extent from (0,0) to (x,y) that will be read
// from the the szg_scene texture by ImGui when the final image is composited.
auto sceneViewportWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    std::optional<UIRectangle> maximizeArea,
    szg_scene::SceneTexture const& texture,
    bool focused
) -> WindowResult<std::optional<szg_scene::SceneViewport>>;
} // namespace szg_ui