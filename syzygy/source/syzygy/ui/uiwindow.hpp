#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include <imgui.h>
#include <optional>
#include <string>

namespace syzygy
{
// Opens the context for a Dear ImGui window. ImGui:: calls during the lifetime
// of the UIWindow object will occur within the context of the window.
struct UIWindow
{
    static auto
    beginMaximized(std::string const& name, syzygy::UIRectangle workArea)
        -> UIWindow;

    static auto
    beginDockable(std::string const& name, std::optional<ImGuiID> dockspace)
        -> UIWindow;

    auto operator=(UIWindow const& other) -> UIWindow& = delete;
    auto operator=(UIWindow&& other) -> UIWindow& = delete;
    UIWindow(UIWindow&& other) noexcept;

    ~UIWindow();
    void end();

    syzygy::UIRectangle screenRectangle{};
    bool open{false};

private:
    UIWindow(
        syzygy::UIRectangle screenRectangle, bool open, uint16_t styleVariables
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
} // namespace syzygy