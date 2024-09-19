#include "uiwindowscope.hpp"

#include <utility>

namespace
{
auto getWindowContent_imgui() -> syzygy::UIRectangle
{
    return syzygy::UIRectangle{
        .min{ImGui::GetWindowContentRegionMin()},
        .max{ImGui::GetWindowContentRegionMax()}
    };
};
} // namespace

namespace syzygy
{
auto UIWindowScope::beginMaximized(
    std::string const& name, UIRectangle const workArea
) -> UIWindowScope
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

auto UIWindowScope::beginDockable(
    std::string const& name, std::optional<ImGuiID> const dockspace
) -> UIWindowScope
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

UIWindowScope::UIWindowScope(UIWindowScope&& other) noexcept
{
    m_screenRectangle = std::exchange(other.m_screenRectangle, UIRectangle{});
    m_open = std::exchange(other.m_open, false);

    m_styleVariables = std::exchange(other.m_styleVariables, 0);
    m_initialized = std::exchange(other.m_initialized, false);
}

UIWindowScope::~UIWindowScope() { end(); }

void UIWindowScope::end()
{
    if (!m_initialized)
    {
        return;
    }
    ImGui::End();
    ImGui::PopStyleVar(m_styleVariables);
    m_initialized = false;
    m_styleVariables = 0;
}
auto UIWindowScope::isOpen() const -> bool { return m_open; }
auto UIWindowScope::screenRectangle() const -> UIRectangle const&
{
    return m_screenRectangle;
}
} // namespace syzygy