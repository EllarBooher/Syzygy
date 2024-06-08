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
    std::span<double const> const fpsValues
    , double const averageFPS
    , size_t const currentFrame
    , float& targetFPS)
{
    if (ImGui::Begin("Performance Information"))
    {
        ImGui::Text("%s", fmt::format("FPS: {:.1f}", averageFPS).c_str());
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
        if (ImPlot::BeginPlot("FPS", ImVec2(-1,200)))
        {
            ImPlot::SetupAxes(
                ""
                , "FPS"
                , ImPlotAxisFlags_NoDecorations 
                | ImPlotAxisFlags_Lock
                , ImPlotAxisFlags_LockMin
            );

            ImPlot::SetupAxesLimits(0, static_cast<double>(fpsValues.size()), 0.0F, 320.0F);

            ImPlot::PlotLine(
                "##fpsValues"
                , fpsValues.data()
                , static_cast<int32_t>(fpsValues.size())
            );

            ImPlot::PlotInfLines(
                "##current"
                , &currentFrame
                , 1
            );
            
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
        ImGui::DragFloat("DPI Scale", &preferences.dpiScale, 0.05F, 0.5F, 4.0F);

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

auto renderHUD(UIPreferences &preferences) -> HUDState
{
    HUDState hud{};

    ImGuiViewport& viewport{ *ImGui::GetMainViewport() };
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

auto buildDefaultMultiWindowLayout(ImVec2 const pos, ImVec2 const size, ImGuiID const parentNode) -> DockingLayout
{
    ImGui::DockBuilderAddNode(parentNode);

    // Set the size and position:
    ImGui::DockBuilderSetNodeSize(parentNode, size);
    ImGui::DockBuilderSetNodePos(parentNode, pos);

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
        .rowBoolean("Animate Sun", atmosphere.animation.animateSun, defaultValues.animation.animateSun)
        .rowFloat(
            "Sun Animation Speed",
            atmosphere.animation.animationSpeed,
            defaultValues.animation.animationSpeed,
            PropertySliderBehavior{
                .bounds = FloatBounds{-20.0F, 20.0F},
            }
        )
        .rowBoolean("Skip Night", atmosphere.animation.skipNight, defaultValues.animation.skipNight)
        .rowVec3(
            "Sun Euler Angles",
            atmosphere.sunEulerAngles,
            defaultValues.sunEulerAngles,
            PropertySliderBehavior{
                .speed = 0.1F,
            }
        )
        .rowReadOnlyVec3("Direction to Sun", atmosphere.directionToSun())
        .rowVec3(
            "Ground Diffuse Color",
            atmosphere.groundColor,
            defaultValues.groundColor,
            PropertySliderBehavior{
                .bounds = FloatBounds{0.0F, 1.0F},
            }
        )
        .rowFloat(
            "Earth Radius",
            atmosphere.earthRadiusMeters,
            defaultValues.earthRadiusMeters,
            PropertySliderBehavior{
                .bounds = FloatBounds{1.0F, atmosphere.atmosphereRadiusMeters},
            }
        )
        .rowFloat(
            "Atmosphere Radius",
            atmosphere.atmosphereRadiusMeters,
            defaultValues.atmosphereRadiusMeters,
            PropertySliderBehavior{
                .bounds = FloatBounds{atmosphere.earthRadiusMeters, 1'000'000'000.0F},
            }
        )
        .rowVec3(
            "Rayleigh Scattering Coefficient",
            atmosphere.scatteringCoefficientRayleigh,
            defaultValues.scatteringCoefficientRayleigh,
            PropertySliderBehavior{
                .speed = 0.001F,
                .bounds = FloatBounds{0.0F, 1.0F},
            }
        )
        .rowFloat(
            "Rayleigh Altitude Decay",
            atmosphere.altitudeDecayRayleigh,
            defaultValues.altitudeDecayRayleigh,
            PropertySliderBehavior{
                .bounds = FloatBounds{0.0F, 1'000'000.0F},
            }
        )
        .rowVec3(
            "Mie Scattering Coefficient",
            atmosphere.scatteringCoefficientMie,
            defaultValues.scatteringCoefficientMie,
            PropertySliderBehavior{
                .speed = 0.001F,
                .bounds = FloatBounds{0.0F, 1.0F},
            }
        )
        .rowFloat(
            "Mie Altitude Decay",
            atmosphere.altitudeDecayMie,
            defaultValues.altitudeDecayMie,
            PropertySliderBehavior{
                .bounds = FloatBounds{0.0F, 1'000'000.0F},
            }
        )
        .end();
}

template<>
void imguiStructureControls<CameraParameters>(
    CameraParameters& structure
    , CameraParameters const& defaultValues
)
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

    PropertyTable::begin()
        .rowVec3(
            "Camera Position",
            structure.cameraPosition,
            defaultValues.cameraPosition,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .rowVec3(
            "Euler Angles",
            structure.eulerAngles,
            defaultValues.eulerAngles,
            PropertySliderBehavior{
                .bounds = FloatBounds{-glm::pi<float>(), glm::pi<float>()},
            }
        )
        .rowFloat(
            "Field of View",
            structure.fov,
            defaultValues.fov,
            PropertySliderBehavior{
                .bounds = FloatBounds{0.01F, 179.99F},
            }
        )
        .rowFloat(
            "Near Plane",
            structure.near,
            std::min(structure.far, defaultValues.near),
            PropertySliderBehavior{
                .bounds = FloatBounds{0.01F, structure.far},
            }
        )
        .rowFloat(
            "Far Plane",
            structure.far,
            std::max(structure.near, defaultValues.far),
            PropertySliderBehavior{
                .bounds = FloatBounds{structure.near + 0.01F, 1'000'000.0F},
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
        
    table.rowReadOnlyText(
            "Pipeline"
            , fmt::format(
                "0x{:x}"
                , reinterpret_cast<uintptr_t>(structure.pipeline.get())
            )
        )
        .rowReadOnlyInteger(
            "Indices on GPU"
            , static_cast<int32_t>(
                structure.indices.get() ? structure.indices->deviceSize() : 0
            )
        )
        .rowReadOnlyInteger(
            "Vertices on GPU"
            , static_cast<int32_t>(
                structure.vertices.get() ? structure.vertices->deviceSize() : 0
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
void imguiStructureControls<ShadowPassParameters>(
    ShadowPassParameters& structure
    , ShadowPassParameters const& defaultValues
)
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

    PropertyTable::begin()
        .rowFloat(
            "Depth Bias Constant",
            structure.depthBiasConstant,
            defaultValues.depthBiasConstant,
            PropertySliderBehavior{
                .speed = 0.01F,
            }
        )
        .rowReadOnlyBoolean("Depth Bias Clamp", false)
        .rowFloat(
            "Depth Bias Slope",
            structure.depthBiasSlope,
            defaultValues.depthBiasSlope,
            PropertySliderBehavior{
                .speed = 0.01F,
            }
        )
        .end();
}

template<>
void imguiStructureControls<SceneBounds>(
    SceneBounds& structure
    , SceneBounds const& defaultValues
)
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
            defaultValues.center,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .rowVec3(
            "Scene Extent",
            structure.extent,
            defaultValues.extent,
            PropertySliderBehavior{
                .speed = 1.0F,
            }
        )
        .end();
}