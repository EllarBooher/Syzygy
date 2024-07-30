#include "uiwindow.hpp"

#include <utility>

namespace
{
auto getWindowContent_imgui() -> ui::UIRectangle
{
    return ui::UIRectangle{
        .min{ImGui::GetWindowContentRegionMin()},
        .max{ImGui::GetWindowContentRegionMax()}
    };
};
} // namespace

auto ui::UIWindow::beginMaximized(
    std::string const& name, ui::UIRectangle const workArea
) -> UIWindow
{
    ImGui::SetNextWindowPos(workArea.pos());
    ImGui::SetNextWindowSize(workArea.size());

    ImGuiWindowFlags constexpr MAXIMIZED_WINDOW_FLAGS{
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoFocusOnAppearing
    };

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));

    bool const open{ImGui::Begin(name.c_str(), nullptr, MAXIMIZED_WINDOW_FLAGS)
    };

    uint16_t constexpr styleVariables{1};
    return {getWindowContent_imgui(), open, styleVariables};
}

auto ui::UIWindow::beginDockable(
    std::string const& name, std::optional<ImGuiID> const dockspace
) -> UIWindow
{
    if (dockspace.has_value())
    {
        ImGui::SetNextWindowDockID(dockspace.value());
    }

    ImGuiWindowFlags constexpr DOCKABLE_WINDOW_FLAGS{
        ImGuiWindowFlags_NoFocusOnAppearing
    };

    bool const open{ImGui::Begin(name.c_str(), nullptr, DOCKABLE_WINDOW_FLAGS)};

    uint16_t constexpr styleVariables{0};
    return {getWindowContent_imgui(), open, styleVariables};
}

ui::UIWindow::UIWindow(UIWindow&& other) noexcept
{
    screenRectangle = std::exchange(other.screenRectangle, ui::UIRectangle{});
    open = std::exchange(other.open, false);

    m_styleVariables = std::exchange(other.m_styleVariables, 0);
    m_initialized = std::exchange(other.m_initialized, false);
}

ui::UIWindow::~UIWindow()
{
    if (!m_initialized)
    {
        return;
    }

    ImGui::End();
    ImGui::PopStyleVar(m_styleVariables);
}