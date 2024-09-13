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
struct SceneTexture;
struct RingBuffer;
struct MeshAsset;
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
    std::span<AssetRef<MeshAsset> const> meshes
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
    ImTextureID const sceneTexture,
    ImVec2 const sceneTextureMax,
    bool focused
) -> WindowResult<std::optional<VkRect2D>>;
} // namespace syzygy