#include "engineui.hpp"

#include <implot.h>
#include <fmt/format.h>
#include <array>

#include "pipelineui.hpp"

#include "../debuglines.hpp"
#include "../assets.hpp"
#include "../shaders.hpp"
#include "../engineparams.hpp"
#include "../shadowpass.hpp"

#include "imgui_internal.h"

#include "propertytable.hpp"

void imguiPerformanceWindow(
    PerformanceValues const values
    , float& targetFPS
)
{
    if (ImGui::Begin("Performance Information"))
    {
        ImGui::Text("%s", fmt::format("FPS: {:.1f}", values.averageFPS).c_str());
        float const minFPS{ 10.0 };
        float const maxFPS{ 1000.0 };
        ImGui::DragScalar(
            "Target FPS"
            , ImGuiDataType_Float
            , &targetFPS
            , 1.0
            , &minFPS
            , &maxFPS
            , nullptr
            , ImGuiSliderFlags_AlwaysClamp
        );

        ImVec2 const plotSize{ -1,200 };

        if (ImPlot::BeginPlot("FPS", plotSize))
        {
            ImPlot::SetupAxes(
                ""
                , "FPS"
                , ImPlotAxisFlags_NoDecorations 
                | ImPlotAxisFlags_Lock
                , ImPlotAxisFlags_LockMin
            );

            double constexpr DISPLAYED_FPS_MIN{ 0.0 };
            double constexpr DISPLAYED_FPS_MAX{ 320.0 };

            ImPlot::SetupAxesLimits(0, static_cast<double>(values.samplesFPS.size()), DISPLAYED_FPS_MIN, DISPLAYED_FPS_MAX);

            ImPlot::PlotLine(
                "##fpsValues"
                , values.samplesFPS.data()
                , static_cast<int32_t>(values.samplesFPS.size())
            );

            ImPlot::PlotInfLines(
                "##current"
                , &values.currentFrame
                , 1
            );
            
            ImPlot::EndPlot();
        }
    }
    ImGui::End();
}

namespace
{
    void renderPreferences(
        bool& open
        , UIPreferences& preferences
        , HUDState& hud
    )
    {
        if (ImGui::Begin("Preferences", &open))
        {
            float constexpr DPI_SPEED{ 0.05F };
            float constexpr DPI_MIN{ 0.5F };
            float constexpr DPI_MAX{ 4.0F };

            ImGui::DragFloat("DPI Scale", &preferences.dpiScale, DPI_SPEED, DPI_MIN, DPI_MAX);

            ImGui::TextWrapped(
                "Some DPI Scale values will produce blurry fonts, "
                "so consider using an integer value."
            );

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
}

auto renderHUD(UIPreferences &preferences) -> HUDState
{
    HUDState hud{};

    ImGuiViewport const &viewport{*ImGui::GetMainViewport()};
    { // Create background windw, as a target for docking

        ImGuiWindowFlags constexpr WINDOW_INVISIBLE_FLAGS{
            ImGuiWindowFlags_MenuBar
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBackground
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoNavFocus
        };
        
        ImGui::SetNextWindowPos(viewport.WorkPos);
        ImGui::SetNextWindowSize(viewport.WorkSize);
        ImGui::SetNextWindowViewport(viewport.ID);

        bool resetLayoutRequested{ false };

        static bool maximizeSceneViewport{ false };
        static bool showPreferences{ false };
        static bool showUIDemoWindow{ false };

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
                ImGui::MenuItem(
                    "Preferences"
                    , nullptr
                    , &showPreferences
                );
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem(
                    "Maximize Scene Viewport"
                    , nullptr
                    , &maximizeSceneViewport
                );
                ImGui::MenuItem(
                    "UI Demo Window"
                    , nullptr
                    , &showUIDemoWindow
                );
                ImGui::MenuItem(
                    "Reset Window Layout"
                    , nullptr
                    , &resetLayoutRequested
                );
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
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

        if (showPreferences)
        {
            renderPreferences(showPreferences, preferences, hud);
        }

        if (showUIDemoWindow)
        {
            PropertyTable::demoWindow(showUIDemoWindow);
        }
    }

    static bool firstLoop{ true };
    if (firstLoop)
    {
        hud.resetLayoutRequested = true;
        firstLoop = false;
    }

    return hud;
}

auto buildDefaultMultiWindowLayout(UIRectangle const workArea, ImGuiID const parentNode) -> DockingLayout
{
    ImGui::DockBuilderAddNode(parentNode);

    // Set the size and position:
    ImGui::DockBuilderSetNodeSize(parentNode, workArea.size());
    ImGui::DockBuilderSetNodePos(parentNode, workArea.pos());

    ImGuiID parentID{ parentNode };

    ImGuiID const leftID{ 
        ImGui::DockBuilderSplitNode(
            parentID
            , ImGuiDir_Left
            , 3.0 / 10.0
            , nullptr
            , &parentID
        ) 
    };
    
    ImGuiID const rightID{ 
        ImGui::DockBuilderSplitNode(
            parentID
            , ImGuiDir_Right
            , 3.0 / 7.0
            , nullptr
            , &parentID
        ) 
    };
    
    ImGuiID const centerBottomID{ 
        ImGui::DockBuilderSplitNode(
            parentID
            , ImGuiDir_Down
            , 3.0 / 10.0
            , nullptr
            , &parentID
        ) 
    };
    
    ImGuiID const centerTopID{ parentID };

    ImGui::DockBuilderFinish(parentNode);

    return DockingLayout{
        .left = leftID,
        .right = rightID,
        .centerBottom = centerBottomID,
        .centerTop = centerTopID,
    };
}

void imguiMeshInstanceControls(
    bool& shouldRender
    , std::span<std::shared_ptr<MeshAsset> const> const meshes
    , size_t& meshIndexSelected
)
{
    std::vector<std::string> meshNames{};
    for (const std::shared_ptr<MeshAsset> &asset : meshes)
    {
        MeshAsset const& mesh{ *asset };

        meshNames.push_back(mesh.name);
    }

    PropertyTable::begin()
        .rowBoolean("Render Mesh Instances", shouldRender, true)
        .rowDropdown("Mesh", meshIndexSelected, 0, meshNames)
        .end();
}

void imguiRenderingSelection(RenderingPipelines& currentActivePipeline)
{
    auto const pipelineOrdering{
        std::to_array<RenderingPipelines>({
            RenderingPipelines::DEFERRED
            , RenderingPipelines::COMPUTE_COLLECTION
        })
    };
    auto const labels{
        std::to_array<std::string>({
            "Deferred"
            , "Compute Collection"
        })
    };

    auto const selectedIt{ 
        std::find(
            pipelineOrdering.begin()
            , pipelineOrdering.end()
            , currentActivePipeline
        ) 
    };
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
        size_t const defaultIndex{ 0 };
        
        auto selectedIndex{ 
            static_cast<size_t>(
                std::distance(pipelineOrdering.begin(), selectedIt)
            ) 
        };
        
        PropertyTable::begin()
            .rowDropdown(
                "Rendering Pipeline"
                , selectedIndex
                , defaultIndex
                , labels
                )
            .end();

        currentActivePipeline = pipelineOrdering[selectedIndex];
    }
}

template<>
void imguiStructureControls(AtmosphereParameters& structure, AtmosphereParameters const& defaultStructure)
{
    if (!ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    FloatBounds constexpr SUN_ANIMATION_SPEED_BOUNDS{ -20.0F, 20.0F };

    float constexpr EULER_ANGLES_SPEED{ 0.1F };

    FloatBounds constexpr RGBA_BOUNDS{ 0.0F, 1.0F };

    float constexpr PLANETARY_RADIUS_MIN{ 1.0F };
    float constexpr PLANETARY_RADIUS_MAX{ 1'000'000'000.0F };

    // Scattering coefficient meaningfully exists over a very small and unpredictable range. 
    // Thus finer controls are needed, and a speed of 0.1 or default 0.0 is too high.
    float constexpr SCATTERING_COEFFICIENT_SPEED{ 0.01F }; 
    FloatBounds constexpr SCATTERING_COEFFICIENT_BOUNDS{ 0.0F, 1.0F };

    FloatBounds constexpr ALTITUDE_DECAY_BOUNDS{ 0.0F, 1'000'000.0F };

    PropertyTable::begin()
        .rowBoolean("Animate Sun", structure.animation.animateSun, defaultStructure.animation.animateSun)
        .rowFloat(
            "Sun Animation Speed",
            structure.animation.animationSpeed,
            defaultStructure.animation.animationSpeed,
            PropertySliderBehavior{
                .bounds = SUN_ANIMATION_SPEED_BOUNDS,
            }
        )
        .rowBoolean("Skip Night", structure.animation.skipNight, defaultStructure.animation.skipNight)
        .rowVec3(
            "Sun Euler Angles",
            structure.sunEulerAngles,
            defaultStructure.sunEulerAngles,
            PropertySliderBehavior{
                .speed = EULER_ANGLES_SPEED,
            }
        )
        .rowReadOnlyVec3("Direction to Sun", structure.directionToSun())
        .rowVec3(
            "Ground Diffuse Color",
            structure.groundColor,
            defaultStructure.groundColor,
            PropertySliderBehavior{
                .bounds = RGBA_BOUNDS,
            }
        )
        .rowFloat(
            "Earth Radius",
            structure.earthRadiusMeters,
            defaultStructure.earthRadiusMeters,
            PropertySliderBehavior{
                .bounds = FloatBounds{PLANETARY_RADIUS_MIN, structure.atmosphereRadiusMeters},
            }
        )
        .rowFloat(
            "Atmosphere Radius",
            structure.atmosphereRadiusMeters,
            defaultStructure.atmosphereRadiusMeters,
            PropertySliderBehavior{
                .bounds = FloatBounds{structure.earthRadiusMeters, PLANETARY_RADIUS_MAX},
            }
        )
        .rowVec3(
            "Rayleigh Scattering Coefficient",
            structure.scatteringCoefficientRayleigh,
            defaultStructure.scatteringCoefficientRayleigh,
            PropertySliderBehavior{
                .speed = SCATTERING_COEFFICIENT_SPEED,
                .bounds = SCATTERING_COEFFICIENT_BOUNDS,
            }
        )
        .rowFloat(
            "Rayleigh Altitude Decay",
            structure.altitudeDecayRayleigh,
            defaultStructure.altitudeDecayRayleigh,
            PropertySliderBehavior{
                .bounds = ALTITUDE_DECAY_BOUNDS,
            }
        )
        .rowVec3(
            "Mie Scattering Coefficient",
            structure.scatteringCoefficientMie,
            defaultStructure.scatteringCoefficientMie,
            PropertySliderBehavior{
                .speed = SCATTERING_COEFFICIENT_SPEED,
                .bounds = SCATTERING_COEFFICIENT_BOUNDS,
            }
        )
        .rowFloat(
            "Mie Altitude Decay",
            structure.altitudeDecayMie,
            defaultStructure.altitudeDecayMie,
            PropertySliderBehavior{
                .bounds = ALTITUDE_DECAY_BOUNDS,
            }
        )
        .end();
}

template<>
void imguiStructureControls(CameraParameters& structure, CameraParameters const& defaultStructure)
{
    bool const headerOpen{
        ImGui::CollapsingHeader(
            "Camera Parameters"
            , ImGuiTreeNodeFlags_DefaultOpen
        )
    };

    if (!headerOpen)
    {
        return;
    }

    // Stay an arbitrary distance away 0 and 180 to avoid singularities
    FloatBounds constexpr FOV_BOUNDS{ 0.01F, 179.99F };

    float constexpr CLIPPING_PLANE_MIN{ 0.01F };
    float constexpr CLIPPING_PlANE_MAX{ 1'000'000.0F };

    float constexpr CLIPPING_PLANE_MARGIN{ 0.01F };

    PropertyTable::begin()
        .rowVec3(
            "Camera Position",
            structure.cameraPosition,
            defaultStructure.cameraPosition,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .rowVec3(
            "Euler Angles",
            structure.eulerAngles,
            defaultStructure.eulerAngles,
            PropertySliderBehavior{
                .bounds = FloatBounds{-glm::pi<float>(), glm::pi<float>()},
            }
        )
        .rowFloat(
            "Field of View",
            structure.fov,
            defaultStructure.fov,
            PropertySliderBehavior{
                .bounds = FOV_BOUNDS,
            }
        )
        .rowFloat(
            "Near Plane",
            structure.near,
            std::min(structure.far, defaultStructure.near),
            PropertySliderBehavior{
                .bounds = FloatBounds{CLIPPING_PLANE_MIN, structure.far},
            }
        )
        .rowFloat(
            "Far Plane",
            structure.far,
            std::max(structure.near, defaultStructure.far),
            PropertySliderBehavior{
                .bounds = FloatBounds{structure.near + CLIPPING_PLANE_MARGIN, CLIPPING_PlANE_MAX},
            }
        )
        .end();
}

template<>
void imguiStructureControls<DebugLines>(
    DebugLines& structure
)
{
    bool const headerOpen{
        ImGui::CollapsingHeader(
            "Debug Lines"
            , ImGuiTreeNodeFlags_DefaultOpen
        )
    };

    if (!headerOpen)
    {
        return;
    }

    auto table{ PropertyTable::begin() };

    table.rowReadOnlyText("Pipeline", fmt::format("0x{:x}", reinterpret_cast<uintptr_t>(structure.pipeline.get())))
        .rowReadOnlyInteger(
            "Indices on GPU", static_cast<int32_t>(structure.indices != nullptr ? structure.indices->deviceSize() : 0)
        )
        .rowReadOnlyInteger(
            "Vertices on GPU",
            static_cast<int32_t>(structure.vertices != nullptr ? structure.vertices->deviceSize() : 0)
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
        DrawResultsGraphics const drawResults{ 
            structure.lastFrameDrawResults 
        };

        table.rowChildPropertyBegin("Draw Results")
            .rowReadOnlyInteger("Draw Calls", static_cast<int32_t>(drawResults.drawCalls))
            .rowReadOnlyInteger("Vertices Drawn", static_cast<int32_t>(drawResults.verticesDrawn))
            .rowReadOnlyInteger("Indices Drawn", static_cast<int32_t>(drawResults.indicesDrawn))
            .childPropertyEnd();
    }

    table.end();
}

template<>
void imguiStructureControls(ShadowPassParameters& structure, ShadowPassParameters const& defaultStructure)
{
    bool const headerOpen{
        ImGui::CollapsingHeader(
            "Shadow Pass Parameters"
            , ImGuiTreeNodeFlags_DefaultOpen
        )
    };
    
    if (!headerOpen)
    {
        return;
    }

    float constexpr DEPTH_BIAS_SPEED{ 0.01F };

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

template<>
void imguiStructureControls(SceneBounds& structure, SceneBounds const& defaultStructure)
{
    bool const headerOpen{
        ImGui::CollapsingHeader("Scene Bounds", ImGuiTreeNodeFlags_DefaultOpen)
    };
    
    if (!headerOpen)
    {
        return;
    }

    PropertyTable::begin()
        .rowVec3(
            "Scene Center",
            structure.center,
            defaultStructure.center,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .rowVec3(
            "Scene Extent",
            structure.extent,
            defaultStructure.extent,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .end();
}