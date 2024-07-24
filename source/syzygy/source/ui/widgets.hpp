#pragma once

#include "../core/scene.hpp"
#include "../enginetypes.hpp"
#include <imgui.h>

namespace ui
{
void performanceWindow(
    std::string const& title,
    std::optional<ImGuiID> dockNode,
    RingBuffer const& values,
    float& targetFPS
);
void sceneControlsWindow(std::string const& title, std::optional<ImGuiID> dockNode, scene::Scene& scene, MeshAssetLibrary const&);
} // namespace ui