#include "engineui.hpp"
#include "imgui_internal.h"
#include "pipelineui.hpp"
#include "propertytable.hpp"
#include "syzygy/assets.hpp"
#include "syzygy/core/scene.hpp"
#include "syzygy/debuglines.hpp"
#include "syzygy/shaders.hpp"
#include "syzygy/shadowpass.hpp"
#include <array>
#include <fmt/format.h>
#include <implot.h>

void imguiPerformanceDisplay(PerformanceValues const values, float& targetFPS)
{
    ImGui::Text("%s", fmt::format("FPS: {:.1f}", values.averageFPS).c_str());
    float const minFPS{10.0};
    float const maxFPS{1000.0};
    ImGui::DragScalar(
        "Target FPS",
        ImGuiDataType_Float,
        &targetFPS,
        1.0,
        &minFPS,
        &maxFPS,
        nullptr,
        ImGuiSliderFlags_AlwaysClamp
    );

    ImVec2 const plotSize{-1, 200};

    if (ImPlot::BeginPlot("FPS", plotSize))
    {
        ImPlot::SetupAxes(
            "",
            "FPS",
            ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock,
            ImPlotAxisFlags_LockMin
        );

        double constexpr DISPLAYED_FPS_MIN{0.0};
        double constexpr DISPLAYED_FPS_MAX{320.0};

        ImPlot::SetupAxesLimits(
            0,
            static_cast<double>(values.samplesFPS.size()),
            DISPLAYED_FPS_MIN,
            DISPLAYED_FPS_MAX
        );

        ImPlot::PlotLine(
            "##fpsValues",
            values.samplesFPS.data(),
            static_cast<int32_t>(values.samplesFPS.size())
        );

        ImPlot::PlotInfLines("##current", &values.currentFrame, 1);

        ImPlot::EndPlot();
    }
}

namespace
{
void renderPreferences(bool& open, UIPreferences& preferences, HUDState& hud)
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

auto renderHUD(UIPreferences& preferences) -> HUDState
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

        bool const backgroundWindow{
            ImGui::Begin("BackgroundWindow", nullptr, WINDOW_INVISIBLE_FLAGS)
        };

        // Can this ever happen?
        assert(backgroundWindow && "Background Window was closed.");

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

auto buildDefaultMultiWindowLayout(
    UIRectangle const workArea, ImGuiID const parentNode
) -> DockingLayout
{
    ImGui::DockBuilderAddNode(parentNode);

    // Set the size and position:
    ImGui::DockBuilderSetNodeSize(parentNode, workArea.size());
    ImGui::DockBuilderSetNodePos(parentNode, workArea.pos());

    ImGuiID parentID{parentNode};

    ImGuiID const leftID{ImGui::DockBuilderSplitNode(
        parentID, ImGuiDir_Left, 3.0 / 10.0, nullptr, &parentID
    )};

    ImGuiID const rightID{ImGui::DockBuilderSplitNode(
        parentID, ImGuiDir_Right, 3.0 / 7.0, nullptr, &parentID
    )};

    ImGuiID const centerBottomID{ImGui::DockBuilderSplitNode(
        parentID, ImGuiDir_Down, 3.0 / 10.0, nullptr, &parentID
    )};

    ImGuiID const centerTopID{parentID};

    ImGui::DockBuilderFinish(parentNode);

    return DockingLayout{
        .left = leftID,
        .right = rightID,
        .centerBottom = centerBottomID,
        .centerTop = centerTopID,
    };
}

void imguiMeshInstanceControls(
    bool& shouldRender,
    std::span<std::shared_ptr<MeshAsset> const> const meshes,
    size_t& meshIndexSelected
)
{
    std::vector<std::string> meshNames{};
    for (std::shared_ptr<MeshAsset> const& asset : meshes)
    {
        MeshAsset const& mesh{*asset};

        meshNames.push_back(mesh.name);
    }

    PropertyTable::begin()
        .rowBoolean("Render Mesh Instances", shouldRender, true)
        .rowDropdown("Mesh", meshIndexSelected, 0, meshNames)
        .end();
}

void imguiRenderingSelection(RenderingPipelines& currentActivePipeline)
{
    auto const pipelineOrdering{std::to_array<RenderingPipelines>(
        {RenderingPipelines::DEFERRED, RenderingPipelines::COMPUTE_COLLECTION}
    )};
    auto const labels{
        std::to_array<std::string>({"Deferred", "Compute Collection"})
    };

    auto const selectedIt{std::find(
        pipelineOrdering.begin(), pipelineOrdering.end(), currentActivePipeline
    )};
    if (pipelineOrdering.end() == selectedIt)
    {
        // If we can't find what index this pipeline should be,
        // don't mess with it, since the engine may have set it.
        PropertyTable::begin()
            .rowReadOnlyText("", "Unknown pipeline selected")
            .end();
    }
    else
    {
        size_t const defaultIndex{0};

        auto selectedIndex{static_cast<size_t>(
            std::distance(pipelineOrdering.begin(), selectedIt)
        )};

        PropertyTable::begin()
            .rowDropdown(
                "Rendering Pipeline", selectedIndex, defaultIndex, labels
            )
            .end();

        currentActivePipeline = pipelineOrdering[selectedIndex];
    }
}

template <> void imguiStructureControls<DebugLines>(DebugLines& structure)
{
    bool const headerOpen{
        ImGui::CollapsingHeader("Debug Lines", ImGuiTreeNodeFlags_DefaultOpen)
    };

    if (!headerOpen)
    {
        return;
    }

    auto table{PropertyTable::begin()};

    table
        .rowReadOnlyText(
            "Pipeline",
            fmt::format(
                "0x{:x}", reinterpret_cast<uintptr_t>(structure.pipeline.get())
            )
        )
        .rowReadOnlyInteger(
            "Indices on GPU",
            static_cast<int32_t>(
                structure.indices != nullptr ? structure.indices->deviceSize()
                                             : 0
            )
        )
        .rowReadOnlyInteger(
            "Vertices on GPU",
            static_cast<int32_t>(
                structure.vertices != nullptr ? structure.vertices->deviceSize()
                                              : 0
            )
        );

    if (!structure.pipeline || !structure.indices || !structure.vertices)
    {
        table.rowReadOnlyBoolean("Enabled", structure.enabled);
    }
    else
    {
        table.rowBoolean("Enabled", structure.enabled, false);
    }

    table.rowFloat(
        "Line Width",
        structure.lineWidth,
        1.0F,
        PropertySliderBehavior{
            .bounds{0.0F, 100.0F},
        }
    );

    {
        DrawResultsGraphics const drawResults{structure.lastFrameDrawResults};

        table.rowChildPropertyBegin("Draw Results")
            .rowReadOnlyInteger(
                "Draw Calls", static_cast<int32_t>(drawResults.drawCalls)
            )
            .rowReadOnlyInteger(
                "Vertices Drawn",
                static_cast<int32_t>(drawResults.verticesDrawn)
            )
            .rowReadOnlyInteger(
                "Indices Drawn", static_cast<int32_t>(drawResults.indicesDrawn)
            )
            .childPropertyEnd();
    }

    table.end();
}

template <>
void imguiStructureControls(
    ShadowPassParameters& structure,
    ShadowPassParameters const& defaultStructure
)
{
    bool const headerOpen{ImGui::CollapsingHeader(
        "Shadow Pass Parameters", ImGuiTreeNodeFlags_DefaultOpen
    )};

    if (!headerOpen)
    {
        return;
    }

    float constexpr DEPTH_BIAS_SPEED{0.01F};

    PropertyTable::begin()
        .rowFloat(
            "Depth Bias Constant",
            structure.depthBiasConstant,
            defaultStructure.depthBiasConstant,
            PropertySliderBehavior{
                .speed = DEPTH_BIAS_SPEED,
            }
        )
        .rowReadOnlyBoolean("Depth Bias Clamp", false)
        .rowFloat(
            "Depth Bias Slope",
            structure.depthBiasSlope,
            defaultStructure.depthBiasSlope,
            PropertySliderBehavior{
                .speed = DEPTH_BIAS_SPEED,
            }
        )
        .end();
}

namespace
{
auto getWindowContent() -> UIRectangle
{
    return UIRectangle{
        .min{ImGui::GetWindowContentRegionMin()},
        .max{ImGui::GetWindowContentRegionMax()}
    };
};
} // namespace

auto UIWindow::beginMaximized(
    std::string const& name, UIRectangle const workArea
) -> UIWindow
{
    ImGui::SetNextWindowPos(workArea.pos());
    ImGui::SetNextWindowSize(workArea.size());

    ImGuiWindowFlags constexpr MAXIMIZED_WINDOW_FLAGS{
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus
    };

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));

    bool const open{ImGui::Begin(name.c_str(), nullptr, MAXIMIZED_WINDOW_FLAGS)
    };

    uint16_t constexpr styleVariables{1};
    return {getWindowContent(), open, styleVariables};
}

auto UIWindow::beginDockable(
    std::string const& name, std::optional<ImGuiID> const dockspace
) -> UIWindow
{
    if (dockspace.has_value())
    {
        ImGui::SetNextWindowDockID(dockspace.value());
    }

    bool const open{ImGui::Begin(name.c_str())};

    uint16_t constexpr styleVariables{0};
    return {getWindowContent(), open, styleVariables};
}

UIWindow::UIWindow(UIWindow&& other) noexcept
{
    screenRectangle = std::exchange(other.screenRectangle, UIRectangle{});
    open = std::exchange(other.open, false);

    m_styleVariables = std::exchange(other.m_styleVariables, 0);
    m_initialized = std::exchange(other.m_initialized, false);
}

UIWindow::~UIWindow()
{
    if (!m_initialized)
    {
        return;
    }

    ImGui::End();
    ImGui::PopStyleVar(m_styleVariables);
}

auto ui::sceneViewport(
    VkDescriptorSet const sceneTexture,
    VkExtent2D const sceneTextureExtent,
    std::optional<UIRectangle> const maximizedArea,
    std::optional<ImGuiID> const dockspace
) -> std::optional<RenderTarget>
{
    char const* const WINDOW_TITLE{"Scene Viewport"};

    if (UIWindow const sceneViewport{
            maximizedArea.has_value()
                ? UIWindow::beginMaximized(WINDOW_TITLE, maximizedArea.value())
                : UIWindow::beginDockable(WINDOW_TITLE, dockspace)
        };
        sceneViewport.open)
    {
        glm::vec2 const contentExtent{sceneViewport.screenRectangle.size()};

        ImVec2 const uvMax{
            contentExtent.x / static_cast<float>(sceneTextureExtent.width),
            contentExtent.y / static_cast<float>(sceneTextureExtent.height)
        };

        ImGui::Image(
            reinterpret_cast<ImTextureID>(sceneTexture),
            contentExtent,
            ImVec2{0.0, 0.0},
            uvMax,
            ImVec4{1.0F, 1.0F, 1.0F, 1.0F},
            ImVec4{0.0F, 0.0F, 0.0F, 0.0F}
        );

        return RenderTarget{.extent = contentExtent};
    }

    return std::nullopt;
}
