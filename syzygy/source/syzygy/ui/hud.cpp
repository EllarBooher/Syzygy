#include "hud.hpp"

#include "syzygy/ui/propertytable.hpp"

namespace
{
void renderPreferences(
    bool& open, szg_ui::UIPreferences& preferences, szg_ui::HUDState& hud
)
{
    if (ImGui::Begin("Preferences", &open))
    {
        float constexpr DPI_SPEED{0.05F};
        float constexpr DPI_MIN{0.5F};
        float constexpr DPI_MAX{4.0F};

        ImGui::DragFloat(
            "DPI Scale", &preferences.dpiScale, DPI_SPEED, DPI_MIN, DPI_MAX
        );

        ImGui::TextWrapped("Some DPI Scale values will produce blurry fonts, "
                           "so consider using an integer value.");

        if (ImGui::Button("Apply"))
        {
            hud.applyPreferencesRequested = true;
        }
        if (ImGui::Button("Reset"))
        {
            hud.resetPreferencesRequested = true;
        }
    }
    ImGui::End();
}
} // namespace

auto szg_ui::renderHUD(UIPreferences& preferences) -> szg_ui::HUDState
{
    HUDState hud{};

    ImGuiViewport const& viewport{*ImGui::GetMainViewport()};
    { // Create background windw, as a target for docking

        ImGuiWindowFlags constexpr WINDOW_INVISIBLE_FLAGS{
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus
        };

        ImGui::SetNextWindowPos(viewport.WorkPos);
        ImGui::SetNextWindowSize(viewport.WorkSize);
        ImGui::SetNextWindowViewport(viewport.ID);

        bool resetLayoutRequested{false};

        static bool maximizeSceneViewport{false};
        bool const maximizeSceneViewportLastValue{maximizeSceneViewport};

        static bool showPreferences{false};
        static bool showUIDemoWindow{false};

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));

        ImGui::Begin("BackgroundWindow", nullptr, WINDOW_INVISIBLE_FLAGS);

        ImGui::PopStyleVar(3);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Tools"))
            {
                ImGui::MenuItem("Preferences", nullptr, &showPreferences);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem(
                    "Maximize Scene Viewport", nullptr, &maximizeSceneViewport
                );
                ImGui::MenuItem("UI Demo Window", nullptr, &showUIDemoWindow);
                ImGui::MenuItem(
                    "Reset Window Layout", nullptr, &resetLayoutRequested
                );
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        bool const maximizeEnded{
            maximizeSceneViewportLastValue && !maximizeSceneViewport
        };

        if (resetLayoutRequested || maximizeEnded)
        {
            hud.rebuildLayoutRequested = true;

            maximizeSceneViewport = false;
        }

        hud.maximizeSceneViewport = maximizeSceneViewport;

        hud.workArea = UIRectangle::fromPosSize(
            ImGui::GetCursorPos(), ImGui::GetContentRegionAvail()
        );
        hud.dockspaceID = ImGui::DockSpace(ImGui::GetID("BackgroundDockSpace"));

        ImGui::End();

        if (showPreferences)
        {
            renderPreferences(showPreferences, preferences, hud);
        }

        if (showUIDemoWindow)
        {
            PropertyTable::demoWindow(showUIDemoWindow);
        }
    }

    static bool firstLoop{true};
    if (firstLoop)
    {
        hud.rebuildLayoutRequested = true;
        firstLoop = false;
    }

    return hud;
}
