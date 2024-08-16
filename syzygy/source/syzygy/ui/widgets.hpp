#pragma once

#include "syzygy/renderer/scenetexture.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>
#include <optional>
#include <string>

namespace syzygy
{
struct Scene;
struct MeshAssetLibrary;
struct RingBuffer;
} // namespace syzygy

// A collection of syzygy widgets that are free standing functions.

namespace syzygy
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
    syzygy::Scene& scene,
    MeshAssetLibrary const& meshes
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
    syzygy::SceneTexture const& texture,
    bool focused
) -> WindowResult<std::optional<syzygy::SceneViewport>>;
} // namespace syzygy