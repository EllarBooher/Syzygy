#pragma once

#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>

namespace ui
{
struct UIPreferences
{
    float dpiScale{2.0f};
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

HUDState renderHUD(UIPreferences& preferences);
} // namespace ui