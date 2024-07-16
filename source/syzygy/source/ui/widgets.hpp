#pragma once

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
}