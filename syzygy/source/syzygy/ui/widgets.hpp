#pragma once

#include "syzygy/core/scene.hpp"
#include "syzygy/core/scenetexture.hpp"
#include "syzygy/enginetypes.hpp"
#include "uirectangle.hpp"
#include <imgui.h>

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

// The returned value indicates the extent from (0,0) to (x,y) that will be read
// from the the scene texture by ImGui when the final image is composited.
auto sceneViewportWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    std::optional<UIRectangle> maximizeArea,
    scene::SceneTexture const& texture
) -> std::optional<scene::SceneViewport>;
} // namespace ui