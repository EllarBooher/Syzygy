#pragma once

#include <imgui.h>
#include <optional>

namespace szg_ui
{
struct UIRectangle;
struct DockingLayout
{
    std::optional<ImGuiID> left{};
    std::optional<ImGuiID> right{};
    std::optional<ImGuiID> centerBottom{};
    std::optional<ImGuiID> centerTop{};
};

// Builds a hardcoded hierarchy of docking nodes from the passed parent.
// This also may break layouts, if windows have been moved or docked,
// since all new IDs are generated.
DockingLayout
buildDefaultMultiWindowLayout(szg_ui::UIRectangle workArea, ImGuiID parentNode);
} // namespace szg_ui