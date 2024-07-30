#pragma once

#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>
#include <optional>
#include <string>

namespace ui
{
// Opens the context for a Dear ImGui window. ImGui:: calls during the lifetime
// of the UIWindow object will occur within the context of the window.
struct UIWindow
{
    static auto
    beginMaximized(std::string const& name, ui::UIRectangle const workArea)
        -> UIWindow;

    static auto beginDockable(
        std::string const& name, std::optional<ImGuiID> const dockspace
    ) -> UIWindow;

    UIWindow& operator=(UIWindow const& other) = delete;
    UIWindow& operator=(UIWindow&& other) = delete;
    UIWindow(UIWindow&& other) noexcept;

    ~UIWindow();

    ui::UIRectangle screenRectangle{};
    bool open{false};

private:
    UIWindow(
        ui::UIRectangle screenRectangle, bool open, uint16_t styleVariables
    )
        : screenRectangle{screenRectangle}
        , open{open}
        , m_styleVariables{styleVariables}
        , m_initialized{true}
    {
    }

    uint16_t m_styleVariables{0};
    bool m_initialized{false};
};
} // namespace ui