#pragma once

#include "syzygy/core/scenetexture.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>
#include <optional>
#include <string>

namespace scene
{
struct Scene;
}
struct MeshAssetLibrary;
struct RingBuffer;

// A collection of ui widgets that are free standing functions.

namespace ui
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
    scene::Scene& scene,
    MeshAssetLibrary const& meshes
);

template <typename T> struct WindowResult
{
    bool focused;
    T payload;
};

// The returned value indicates the extent from (0,0) to (x,y) that will be read
// from the the scene texture by ImGui when the final image is composited.
auto sceneViewportWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    std::optional<UIRectangle> maximizeArea,
    scene::SceneTexture const& texture
) -> WindowResult<std::optional<scene::SceneViewport>>;
} // namespace ui