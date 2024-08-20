#pragma once

#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>

namespace syzygy
{
struct UIPreferences
{
    static float constexpr DEFAULT_DPI_SCALE{2.0F};

    float dpiScale{DEFAULT_DPI_SCALE};
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