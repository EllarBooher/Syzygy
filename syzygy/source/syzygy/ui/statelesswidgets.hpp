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
struct ImageView;
struct SceneTemplate;
struct InputSnapshot;
struct SceneNode;
} // namespace syzygy

namespace syzygy
{
// "Pure" widgets that require the function to be called every time it needs to
// be rendered.
//
// These aren't really stateless since they rely on a lot of
// internal ImGui state. They should be reworked into widgets that track a
// little bit of state, since it is hard to pack all the functionality into a
// single function signature.

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

// Returns the currently selected node
auto sceneHierarchyWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    syzygy::Scene& scene
) -> std::weak_ptr<SceneNode>;

void sceneControlsWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    syzygy::Scene& scene
);

void inputVisualizerWindow(
    std::optional<ImGuiID> dockNode, InputSnapshot const& snapshot
);

template <typename T> struct WindowResult
{
    bool focused;
    T payload;
};

// The returned value indicates the window pixels that the scene viewport takes
// up.
auto sceneViewportWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    std::optional<UIRectangle> maximizeArea,
    ImTextureID sceneTexture,
    ImVec2 sceneTextureMax,
    bool focused
) -> WindowResult<std::optional<UIRectangle>>;
} // namespace syzygy