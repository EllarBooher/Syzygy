#pragma once

#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>

namespace syzygy
{
struct UIPreferences
{
    float dpiScale{2.0F};
};

struct HUDState
{
    UIRectangle workArea{};

    // The background window that acts as the parent of all the laid out windows
    ImGuiID dockspaceID{};

    bool maximizeSceneViewport{false};

    bool rebuildLayoutRequested{false};

    bool resetPreferencesRequested{false};
    bool applyPreferencesRequested{false};
};

auto renderHUD(UIPreferences& preferences) -> HUDState;
} // namespace syzygy