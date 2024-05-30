#include "engineui.hpp"

#include <implot.h>
#include <fmt/format.h>

#include "pipelineui.hpp"

#include "../debuglines.hpp"
#include "../assets.hpp"
#include "../shaders.hpp"
#include "../engineparams.hpp"
#include "../shadowpass.hpp"

#include "imgui_internal.h"

#include "propertytable.hpp"

void imguiPerformanceWindow(
    std::span<double const> fpsValues
    , double averageFPS
    , size_t currentFrame
    , float& targetFPS)
{
    if (ImGui::Begin("Performance Information"))
    {
        ImGui::Text(fmt::format("FPS: {:.1f}", averageFPS).c_str());
        float const minFPS{ 10.0 };
        float const maxFPS{ 1000.0 };
        ImGui::DragScalar("Target FPS", ImGuiDataType_Float, &targetFPS, 1.0, &minFPS, &maxFPS, nullptr, ImGuiSliderFlags_AlwaysClamp);
        if (ImPlot::BeginPlot("FPS", ImVec2(-1,200)))
        {
            ImPlot::SetupAxes("", "FPS", ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock, ImPlotAxisFlags_LockMin);
            ImPlot::SetupAxesLimits(0, fpsValues.size(), 0.0f, 320.0f);
            ImPlot::PlotLine("##fpsValues", fpsValues.data(), fpsValues.size());
            ImPlot::PlotInfLines("##current", &currentFrame, 1);
            ImPlot::EndPlot();
        }
    }
    ImGui::End();
}

static void renderPreferences(
    bool& open
    , UIPreferences& preferences
    , HUDState& hud
)
{
    if (ImGui::Begin("Preferences", &open))
    {
        ImGui::DragFloat("DPI Scale", &preferences.dpiScale, 0.05f, 0.5f, 4.0f);
        ImGui::TextWrapped("Some DPI Scale values will produce blurry fonts, so consider using an integer value.");

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

HUDState renderHUD(UIPreferences& preferences)
{
    HUDState hud{};

    ImGuiViewport& viewport{ *ImGui::GetMainViewport() };
    { // Create background windw, as a target for docking
        // These flags ensure the dockspace window is uninteractable and static
        ImGuiWindowFlags constexpr WINDOW_FLAGS = ImGuiWindowFlags_None
            | ImGuiWindowFlags_MenuBar
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoDecoration 
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus 
            | ImGuiWindowFlags_NoNavFocus;
        
        ImGui::SetNextWindowPos(viewport.WorkPos);
        ImGui::SetNextWindowSize(viewport.WorkSize);
        ImGui::SetNextWindowViewport(viewport.ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        bool resetLayoutRequested{ false };

        static bool maximizeSceneViewport{ false };
        static bool showPreferences{ false };
        static bool showUIDemoWindow{ false };

        if (ImGui::Begin("BackgroundWindow", nullptr, WINDOW_FLAGS))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Tools"))
                {
                    ImGui::MenuItem("Preferences", nullptr, &showPreferences);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Window"))
                {
                    ImGui::MenuItem("Maximize Scene Viewport", nullptr, &maximizeSceneViewport);
                    ImGui::MenuItem("UI Demo Window", nullptr, &showUIDemoWindow);
                    ImGui::MenuItem("Reset Window Layout", nullptr, &resetLayoutRequested);
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
        }

        if (resetLayoutRequested)
        {
            hud.resetLayoutRequested = true;

            maximizeSceneViewport = false;
        }

        hud.maximizeSceneViewport = maximizeSceneViewport;

        hud.workArea = UIRectangle::fromPosSize(
            ImGui::GetCursorPos()
            , ImGui::GetContentRegionAvail()
        );
        hud.dockspaceID = ImGui::DockSpace(ImGui::GetID("BackgroundDockSpace"));

        ImGui::End();

        ImGui::PopStyleVar(3);

        if (showPreferences) renderPreferences(showPreferences, preferences, hud);
        if (showUIDemoWindow) PropertyTable::demoWindow();
    }

    static bool firstLoop{ true };
    if (firstLoop)
    {
        hud.resetLayoutRequested = true;
        firstLoop = false;
    }

    return hud;
}

DockingLayout buildDefaultMultiWindowLayout(
    ImVec2 const pos
    , ImVec2 const size
    , ImGuiID const parentNode
)
{
    ImGui::DockBuilderAddNode(parentNode);

    // Set the size and position:
    ImGui::DockBuilderSetNodeSize(parentNode, size);
    ImGui::DockBuilderSetNodePos(parentNode, pos);

    ImGuiID parentID{ parentNode };

    ImGuiID const leftID{ ImGui::DockBuilderSplitNode(parentID, ImGuiDir_Left, 0.5f, nullptr, &parentID) };
    ImGuiID const rightID{ ImGui::DockBuilderSplitNode(parentID, ImGuiDir_Right, 0.5f, nullptr, &parentID) };
    ImGuiID const centerBottomID{ ImGui::DockBuilderSplitNode(parentID, ImGuiDir_Down, 0.5f, nullptr, &parentID) };
    ImGuiID const centerTopID{ parentID };

    ImGui::DockBuilderFinish(parentNode);

    return DockingLayout{
        .left{ leftID },
        .right{ rightID },
        .centerBottom{ centerBottomID },
        .centerTop{ centerTopID },
    };
}

void imguiMeshInstanceControls(
    bool& shouldRender
    , std::span<std::shared_ptr<MeshAsset> const> meshes
    , size_t& meshIndexSelected
)
{
    std::vector<std::string> meshNames{};
    for (std::shared_ptr<MeshAsset> asset : meshes)
    {
        MeshAsset const& mesh{ *asset };

        meshNames.push_back(mesh.name);
    }

    PropertyTable::begin()
        .rowBoolean("Render Mesh Instances", shouldRender, true)
        .rowDropdown("Mesh", meshIndexSelected, 0, meshNames)
        .end();
}

static std::vector<std::tuple<RenderingPipelines, std::string>> renderingPipelineLabels{
    std::make_tuple(RenderingPipelines::DEFERRED, "Deferred")
    , std::make_tuple(RenderingPipelines::COMPUTE_COLLECTION, "Compute Collection")
};

void imguiRenderingSelection(RenderingPipelines& currentActivePipeline)
{
    ImGui::Text("Rendering Pipeline:");
    for (auto const& [pipeline, label] : renderingPipelineLabels)
    {
        if (ImGui::RadioButton(label.c_str(), currentActivePipeline == pipeline))
        {
            currentActivePipeline = pipeline;
        }
    }
}

template<>
void imguiStructureControls<AtmosphereParameters>(
    AtmosphereParameters& atmosphere
    , AtmosphereParameters const& defaultValues
)
{
    if (!ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    PropertyTable::begin()
        .rowBoolean(
            "Animate Sun"
            , atmosphere.animation.animateSun, defaultValues.animation.animateSun)
        .rowFloat(
            "Sun Animation Speed"
            , atmosphere.animation.animationSpeed, defaultValues.animation.animationSpeed
            , PropertySliderBehavior{
                .bounds{ -20.0f, 20.0f },
            })
        .rowBoolean(
            "Skip Night"
            , atmosphere.animation.skipNight, defaultValues.animation.skipNight)
        .rowVec3(
            "Sun Euler Angles"
            , atmosphere.sunEulerAngles, defaultValues.sunEulerAngles
            , PropertySliderBehavior{
                .speed{ 0.1f },
            })
        .rowReadOnlyVec3(
            "Direction to Sun"
            , atmosphere.directionToSun())
        .rowVec3(
            "Ground Diffuse Color"
            , atmosphere.groundColor, defaultValues.groundColor
            , PropertySliderBehavior{
                .bounds{ 0.0f, 1.0f },
            })
        .rowFloat(
            "Earth Radius"
            , atmosphere.earthRadiusMeters, defaultValues.earthRadiusMeters
            , PropertySliderBehavior{
                .bounds{ 1.0f, atmosphere.atmosphereRadiusMeters },
            })
        .rowFloat(
            "Atmosphere Radius"
            , atmosphere.atmosphereRadiusMeters, defaultValues.atmosphereRadiusMeters
            , PropertySliderBehavior{
                .bounds{ atmosphere.earthRadiusMeters, 1'000'000'000.0f },
            })
        .rowVec3(
            "Rayleigh Scattering Coefficient"
            , atmosphere.scatteringCoefficientRayleigh, defaultValues.scatteringCoefficientRayleigh
            , PropertySliderBehavior{
                .speed{ 0.001f },
                .bounds{ 0.0f, 1.0f },
            })
        .rowFloat(
            "Rayleigh Altitude Decay"
            , atmosphere.altitudeDecayRayleigh, defaultValues.altitudeDecayRayleigh
            , PropertySliderBehavior{
                .bounds{0.0f, 1'000'000.0f},
            })
        .rowVec3(
            "Mie Scattering Coefficient"
            , atmosphere.scatteringCoefficientMie, defaultValues.scatteringCoefficientMie
            , PropertySliderBehavior{
                .speed{ 0.001f },
                .bounds{ 0.0f, 1.0f },
            })
        .rowFloat(
            "Mie Altitude Decay"
            , atmosphere.altitudeDecayMie, defaultValues.altitudeDecayMie
            , PropertySliderBehavior{
                .bounds{0.0f, 1'000'000.0f},
            })
        .end();
}

template<>
void imguiStructureControls<CameraParameters>(
    CameraParameters& structure
    , CameraParameters const& defaultValues
)
{
    if (!ImGui::CollapsingHeader("Camera Parameters", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    PropertyTable::begin()
        .rowVec3(
            "Camera Position"
            , structure.cameraPosition, defaultValues.cameraPosition
            , PropertySliderBehavior{
                .speed{ 1.0f }
            })
        .rowVec3(
            "Euler Angles"
            , structure.eulerAngles, defaultValues.eulerAngles
            , PropertySliderBehavior{
                .bounds{ -glm::pi<float>(), glm::pi<float>() }
            })
        .rowFloat(
            "Field of View"
            , structure.fov, defaultValues.fov
            , PropertySliderBehavior{
                .bounds{ 0.01f, 179.99f }
            })
        .rowFloat(
            "Near Plane"
            , structure.near, std::min(structure.far, defaultValues.near)
            , PropertySliderBehavior{
                .bounds{ 0.01f, structure.far }
            })
        .rowFloat(
            "Far Plane"
            , structure.far, std::max(structure.near, defaultValues.far)
            , PropertySliderBehavior{
                .bounds{ structure.near + 0.01f, 1'000'000.0f }
            })
        .end();
}

template<>
void imguiStructureDisplay<DrawResultsGraphics>(
    DrawResultsGraphics const& structure
)
{
    ImGui::BeginGroup();
    ImGui::Text("Graphics Draw Results");
    ImGui::Indent(10.0);

    ImGui::BeginTable("drawResultsGraphics", 2);

    ImGui::TableNextColumn();
    ImGui::Text("Draw Calls");
    ImGui::TableNextColumn();
    ImGui::Text("%u", structure.drawCalls);

    ImGui::TableNextColumn();
    ImGui::Text("Vertices Drawn");
    ImGui::TableNextColumn();
    ImGui::Text("%u", structure.verticesDrawn);

    ImGui::TableNextColumn();
    ImGui::Text("Indices Drawn");
    ImGui::TableNextColumn();
    ImGui::Text("%u", structure.indicesDrawn);

    ImGui::EndTable();

    ImGui::Unindent(10.0);

    ImGui::EndGroup();
}

template<>
void imguiStructureControls<DebugLines>(
    DebugLines& structure
)
{
    if (!ImGui::CollapsingHeader("Debug Lines", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    auto table{ PropertyTable::begin() };
        
    table.rowReadOnlyText("Pipeline", fmt::format("0x{:x}", reinterpret_cast<uintptr_t>(structure.pipeline.get())))
        .rowReadOnlyInteger("Indices on GPU", structure.indices.get() ? structure.indices->deviceSize() : 0)
        .rowReadOnlyInteger("Vertices on GPU", structure.vertices.get() ? structure.vertices->deviceSize() : 0);

    if (!structure.pipeline || !structure.indices || !structure.vertices)
    {
        table.rowReadOnlyBoolean("Enabled", structure.enabled);
    }
    else
    {
        table.rowBoolean("Enabled", structure.enabled, false);
    }

    table.rowFloat("Line Width", structure.lineWidth, 1.0f, PropertySliderBehavior{
        .bounds{ 0.0f, 100.0f }
    });

    static bool collapseDrawResults{ true };

    table.rowChildProperty("Draw Results", collapseDrawResults);
    
    if (!collapseDrawResults)
    {
        DrawResultsGraphics const drawResults{ structure.lastFrameDrawResults };

        ImGui::Indent(10.0f);
        table.rowReadOnlyInteger("Draw Calls", drawResults.drawCalls)
            .rowReadOnlyInteger("Vertices Drawn", drawResults.verticesDrawn)
            .rowReadOnlyInteger("Indices Drawn", drawResults.indicesDrawn);
        ImGui::Unindent(10.0f);
    }

    table.end();
}

template<>
void imguiStructureControls<ShadowPassParameters>(
    ShadowPassParameters& structure
    , ShadowPassParameters const& defaultValues
)
{
    if (!ImGui::CollapsingHeader("Shadow Pass Parameters", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    PropertyTable::begin()
        .rowFloat(
            "Depth Bias Constant"
            , structure.depthBiasConstant, defaultValues.depthBiasConstant
            , PropertySliderBehavior{
                .speed{0.01f}
            })
        .rowReadOnlyBoolean(
            "Depth Bias Clamp"
            , false)
        .rowFloat(
            "Depth Bias Slope"
            , structure.depthBiasSlope, defaultValues.depthBiasSlope
            , PropertySliderBehavior{
                .speed{0.01f}
            })
        .end();
}

template<>
void imguiStructureControls<SceneBounds>(
    SceneBounds& structure
    , SceneBounds const& defaultValues
)
{
    if (!ImGui::CollapsingHeader("Scene Bounds", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    PropertyTable::begin()
        .rowVec3(
            "Scene Center"
            , structure.center, defaultValues.center
            , PropertySliderBehavior{
                .speed{1.0f}
            })
        .rowVec3(
            "Scene Extent"
            , structure.extent, defaultValues.extent
            , PropertySliderBehavior{
                .speed{1.0f}
            })
        .end();
}