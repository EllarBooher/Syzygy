#include "engineui.hpp"

#include <implot.h>
#include <fmt/format.h>

#include "pipelineui.hpp"

#include "../debuglines.hpp"
#include "../assets.hpp"
#include "../shaders.hpp"
#include "../engineparams.hpp"
#include "../shadowpass.hpp"

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

float draggableBar(
    std::string const id
    , float currentPosition
    , bool const horizontal
    , glm::vec2 const min
    , glm::vec2 const max
)
{
    ImGuiID const imguiID{ ImGui::GetID(id.c_str()) };
    static std::optional<ImGuiID> draggedID{};

    ImGuiIO const& imguiIO{ ImGui::GetIO() };

    ImVec2 const mousePosition{ imguiIO.MousePos };

    ImVec2 const boundsMin{
        horizontal
        ? ImVec2{ min.x, currentPosition - 4.0f }
        : ImVec2{ currentPosition - 4.0f, min.y }
    };
    ImVec2 const boundsMax{
        horizontal
        ? ImVec2{ max.x, currentPosition + 4.0f }
        : ImVec2{ currentPosition + 4.0f, max.y }
    };

    bool const mouseInBounds{
        mousePosition.x >= boundsMin.x
        && mousePosition.x < boundsMax.x
        && mousePosition.y >= boundsMin.y
        && mousePosition.y < boundsMax.y
    };

    // Track and only drag a single bar
    if (!draggedID.has_value() && imguiIO.MouseClicked[0] && mouseInBounds)
    {
        draggedID = imguiID;
    }

    bool const dragging{ draggedID.has_value() && draggedID.value() == imguiID };

    // Show indicator on hover or when being dragged
    if (dragging || mouseInBounds)
    {
        ImGui::GetForegroundDrawList()->AddRectFilled(
            horizontal
            ? ImVec2{ min.x, currentPosition - 1.0f }
            : ImVec2{ currentPosition - 1.0f, min.y }
            , horizontal
            ? ImVec2{ max.x, currentPosition + 1.0f }
            : ImVec2{ currentPosition + 1.0f, max.y }
            , ImGui::ColorConvertFloat4ToU32(ImVec4{ 0.0, 0.0, 1.0, 1.0 })
        );
    }

    if (dragging)
    {
        if (imguiIO.MouseDown[0])
        {
            ImVec2 const mouseDelta{ imguiIO.MouseDelta };
            currentPosition += horizontal ? mouseDelta.y : mouseDelta.x;
        }
        else
        {
            draggedID.reset();
        }
    }

    return horizontal 
        ? std::clamp(currentPosition, min.y, max.y)
        : std::clamp(currentPosition, min.x, max.x);
}

HUDState renderHUD(glm::vec2 const extent)
{
    ImVec2 menuBarSize{};
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Test");
        menuBarSize = ImGui::GetWindowSize();
        ImGui::EndMainMenuBar();
    }

    UIRectangle const belowMenuBarArea{
        UIRectangle{
            .min{ 0.0f, menuBarSize.y },
            .max{ extent },
        }.clampToMin()
    };
    // This is a mutable rectangle we eat away from, so it's easier to avoid overlapping UI
    UIRectangle workArea{ belowMenuBarArea };

    struct SidebarSizes
    {
        float leftWidth;
        float rightWidth;
        float bottomHeight;
    };

    static SidebarSizes sidebars{
        .leftWidth{ glm::min(300.0f, workArea.size().x / 2.0f) },
        .rightWidth{ glm::min(300.0f, workArea.size().x / 2.0f) },
        .bottomHeight{ glm::min(300.0f, workArea.size().y) },
    };

    float constexpr sidebarMinimumSize{ 30.0f };

    // Determine the sidebars' current state

    { // Left sidebar draggable right side
        UIRectangle const leftSidebarArea{
            workArea
                .shrinkMax(glm::vec2{sidebars.rightWidth, 0.0f})
                .clampToMin()
        };

        float const position{ workArea.min.x + sidebars.leftWidth };

        float const newPosition{
            draggableBar(
                "##leftSidebarDragRect"
                , position
                , false
                , leftSidebarArea.min
                , leftSidebarArea.max
            )
        };

        sidebars.leftWidth = glm::max(sidebarMinimumSize, newPosition - workArea.min.x);
        workArea = workArea.shrinkMin(glm::vec2{ sidebars.leftWidth, 0.0f });
    }

    { // Right sidebar draggable left side
        UIRectangle const rightSidebarArea{ workArea.clampToMin() };

        float const position{ workArea.max.x - sidebars.rightWidth };

        float const newPosition{
            draggableBar(
                "##rightSidebarDragRect"
                , position
                , false
                , rightSidebarArea.min
                , rightSidebarArea.max
            )
        };

        sidebars.rightWidth = glm::max(sidebarMinimumSize, workArea.max.x - newPosition);
        workArea = workArea.shrinkMax(glm::vec2{ sidebars.rightWidth, 0.0f });
    }

    { // Bottom sidebar draggable top side
        UIRectangle const bottomSidebarArea{ workArea.clampToMin() };

        float const position{ workArea.max.y - sidebars.bottomHeight };

        float const newPosition{
            draggableBar(
                "##bottomSidebarDragRect"
                , position
                , true
                , bottomSidebarArea.min
                , bottomSidebarArea.max
            )
        };

        sidebars.bottomHeight = glm::max(sidebarMinimumSize, workArea.max.y - newPosition);
        workArea = workArea.shrinkMax(glm::vec2{ 0.0f, sidebars.bottomHeight });
    }

    // Draw sidebars

    HUDState docks{
        .remainingArea{ workArea }
    };

    { // Begin bottom sidebar
        UIRectangle const bottomSidebar{
            .min{ belowMenuBarArea.min.x + sidebars.leftWidth, belowMenuBarArea.max.y - sidebars.bottomHeight },
            .max{ belowMenuBarArea.max.x - sidebars.rightWidth, belowMenuBarArea.max.y },
        };

        ImGui::SetNextWindowPos(bottomSidebar.pos(), ImGuiCond_Always);
        ImGui::SetNextWindowSize(bottomSidebar.size(), ImGuiCond_Always);

        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        if (ImGui::Begin(
            "BottomSidebarWindow"
            , nullptr
            , ImGuiWindowFlags_None
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
        ))
        {
            docks.bottomDock = ImGui::DockSpace(ImGui::GetID("BottomSidebarDock"));
        }
        ImGui::End();
    } // End bottom sidebar

    { // Begin left sidebar
        UIRectangle const leftSidebar{
            .min{ belowMenuBarArea.min },
            .max{ glm::vec2{ belowMenuBarArea.min.x + sidebars.leftWidth, belowMenuBarArea.max.y }}
        };

        ImGui::SetNextWindowPos(leftSidebar.pos(), ImGuiCond_Always);
        ImGui::SetNextWindowSize(leftSidebar.size(), ImGuiCond_Always);

        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        if (ImGui::Begin(
            "LeftSidebarWindow"
            , nullptr
            , ImGuiWindowFlags_None
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
        ))
        {
            docks.leftDock = ImGui::DockSpace(ImGui::GetID("LeftSidebarDock"));
        }
        ImGui::End();
    } // End left sidebar

    { // Begin right sidebar
        UIRectangle const rightSidebar{
            .min{ glm::vec2{ belowMenuBarArea.max.x - sidebars.rightWidth, belowMenuBarArea.min.y } },
            .max{ belowMenuBarArea.max },
        };

        ImGui::SetNextWindowPos(rightSidebar.pos(), ImGuiCond_Always);
        ImGui::SetNextWindowSize(rightSidebar.size(), ImGuiCond_Always);

        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        if (ImGui::Begin(
            "RightSidebarWindow"
            , nullptr
            , ImGuiWindowFlags_None
            | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
        ))
        {
            docks.rightDock = ImGui::DockSpace(ImGui::GetID("RightSidebarDock"));
        }
        ImGui::End();
    } // End right sidebar

    return docks;
}

void imguiMeshInstanceControls(
    bool& shouldRender
    , std::span<std::shared_ptr<MeshAsset> const> meshes
    , size_t& meshIndexSelected
)
{
    ImGui::Checkbox("Render Mesh Instances", &shouldRender);
    ImGui::Indent(10.0f);
    ImGui::BeginDisabled(!shouldRender);
    {
        ImGui::Text("Select loaded mesh to use:");
        size_t meshIndex{ 0 };
        for (std::shared_ptr<MeshAsset> asset : meshes)
        {
            MeshAsset const& mesh{ *asset };

            if (ImGui::RadioButton(mesh.name.c_str(), meshIndexSelected == meshIndex))
            {
                meshIndexSelected = meshIndex;
            }
            meshIndex += 1;
        }
    }
    ImGui::EndDisabled();
    ImGui::Unindent(10.0f);
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

static bool RightJustifiedButton(std::string const& label, std::string const& suffix)
{
    std::string const fullLabel = fmt::format("{}##{}", label, suffix);
    ImVec2 const textSize{ ImGui::CalcTextSize(fullLabel.c_str(), nullptr, true) };

    float const buttonWidth{ textSize.x + 20.0f };

    ImGui::SameLine(ImGui::GetWindowWidth() - buttonWidth, 0.0);
    return ImGui::SmallButton(fullLabel.c_str());
}

template<typename T>
static void ResetButton(std::string const& label, T& value, T const& defaultValue)
{
    if (RightJustifiedButton("Reset", label))
    {
        value = defaultValue;
    }
}

static void DragScalarFloats(
    std::string const& label
    , std::span<float> const values
    , float const speed
    , std::optional<float> const min
    , std::optional<float> const max
    , ImGuiSliderFlags const flags = 0
    , std::string const format = "%f"
)
{
    ImGui::DragScalarN(
        label.c_str()
        , ImGuiDataType_Float
        , values.data(), values.size()
        , speed
        , min.has_value() ? &min.value() : nullptr
        , max.has_value() ? &max.value() : nullptr
        , format.c_str()
        , flags
    );
}

template<typename T>
struct is_glm_vec : std::false_type
{};

template<size_t N, typename T>
struct is_glm_vec<glm::vec<N, T>> : std::is_same<T, float>
{};

template<typename T>
static void DragScalarFloats(
    std::string const& label
    , T& values
    , float const speed
    , ImGuiSliderFlags const flags = 0
    , std::string const format = "%f"
)
{
    static_assert(sizeof(T) % 4 == 0);
    static_assert(is_glm_vec<T>::value || std::is_same<T, float>::value);
    size_t constexpr N = sizeof(T) / 4;
    DragScalarFloats(
        label
        , std::span<float>(reinterpret_cast<float*>(&values), N)
        , speed
        , std::nullopt
        , std::nullopt
        , flags
        , format
    );
}

template<typename T>
static void DragScalarFloats(
    std::string const& label
    , T& values
    , std::tuple<float,float> const bounds 
    , ImGuiSliderFlags const flags = 0
    , std::string const format = "%f"
)
{
    static_assert(sizeof(T) % 4 == 0);
    static_assert(is_glm_vec<T>::value || std::is_same<T, float>::value);
    size_t constexpr N = sizeof(T) / 4;

    float constexpr speed = 0.0f;

    DragScalarFloats(
        label
        , std::span<float>(reinterpret_cast<float*>(&values), N)
        , speed
        , std::get<0>(bounds)
        , std::get<1>(bounds)
        , flags
        , format
    );
}

// Templating for value types that cannot be implicitely converted to span of floats.
template<typename T>
static void FractionalCoefficientSlider(std::string const& label, T& values)
{
    auto const range{ std::make_tuple<float,float>(0.0, 1.0) };

    DragScalarFloats(
        label
        , values
        , range
        , ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic
        , "%.8f"
    );
}

template<>
void imguiStructureControls<AtmosphereParameters>(
    AtmosphereParameters& atmosphere
    , AtmosphereParameters const& defaultValues
)
{
    ImGui::BeginGroup();
    ImGui::Text("Atmosphere Parameters");
    ResetButton("atmosphereParameters", atmosphere, defaultValues);

    { // Sun direction controls
        ImGui::Checkbox("Animate Sun", &atmosphere.animation.animateSun);
        ImGui::Indent(10.0f);

        DragScalarFloats("Speed##sun", atmosphere.animation.animationSpeed, { 0.0, 100.0 });
        ResetButton("sunSpeed", atmosphere.animation.animationSpeed, defaultValues.animation.animationSpeed);

        ImGui::Checkbox("Night Multiplier##sun", &atmosphere.animation.skipNight);
        
        ImGui::BeginDisabled(atmosphere.animation.animateSun);
        DragScalarFloats("sunEulerAngles", atmosphere.sunEulerAngles, 0.1f);
        ResetButton("sunEulerAngles", atmosphere.sunEulerAngles, defaultValues.sunEulerAngles);
        ImGui::EndDisabled();

        ImGui::BeginDisabled(true);
        {
            glm::vec3 direction{ atmosphere.directionToSun() };
            DragScalarFloats("directionToSun", direction, { -1.0, 1.0 });
        }
        ImGui::EndDisabled();
        ImGui::Unindent(10.0f);
    }

    DragScalarFloats(
        "Ground Diffuse Color", atmosphere.groundColor
        , 0.0f, 1.0f
    );
    ResetButton("groundColor", atmosphere.groundColor, defaultValues.groundColor);

    DragScalarFloats(
        "Earth Radius (meters)", atmosphere.earthRadiusMeters
        , { 1.0f, atmosphere.atmosphereRadiusMeters }
        , ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic
    );
    ResetButton("earthRadiusMeters", atmosphere.earthRadiusMeters, defaultValues.earthRadiusMeters);

    DragScalarFloats(
        "Atmosphere Radius (meters)", atmosphere.atmosphereRadiusMeters
        , { atmosphere.earthRadiusMeters, 1'000'000'000.0f }
        , ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic
    );
    ResetButton("atmosphereRadiusMeters", atmosphere.atmosphereRadiusMeters, defaultValues.atmosphereRadiusMeters);

    FractionalCoefficientSlider("Rayleigh Scattering Coefficient", atmosphere.scatteringCoefficientRayleigh);
    ResetButton("scatteringCoefficientRayleigh", atmosphere.scatteringCoefficientRayleigh, defaultValues.scatteringCoefficientRayleigh);

    DragScalarFloats("Rayleigh Altitude Decay", atmosphere.altitudeDecayRayleigh, { 0.0f, 10'000.0f });
    ResetButton("altitudeDecayRayleigh", atmosphere.altitudeDecayRayleigh, defaultValues.altitudeDecayRayleigh);

    FractionalCoefficientSlider("Mie Scattering Coefficient", atmosphere.scatteringCoefficientMie);
    ResetButton("scatteringCoefficientMie", atmosphere.scatteringCoefficientMie, defaultValues.scatteringCoefficientMie);

    DragScalarFloats("Mie Altitude Decay", atmosphere.altitudeDecayMie, { 0.0f, 10'000.0f });
    ResetButton("altitudeDecayMie", atmosphere.altitudeDecayMie, defaultValues.altitudeDecayMie);

    ImGui::EndGroup();
}

template<>
void imguiStructureControls<CameraParameters>(
    CameraParameters& structure
    , CameraParameters const& defaultValues
)
{
    ImGui::BeginGroup();
    ImGui::Text("Camera Parameters");
    ResetButton("cameraParameters", structure, defaultValues);

    ImGui::DragScalarN("cameraPosition", ImGuiDataType_Float, &structure.cameraPosition, 3, 0.2f);
    ResetButton("cameraPosition", structure.cameraPosition, defaultValues.cameraPosition);

    DragScalarFloats("eulerAngles", structure.eulerAngles, { glm::radians(-180.0f), glm::radians(180.0f) });
    ResetButton("eulerAngles", structure.eulerAngles, defaultValues.eulerAngles);
    structure.eulerAngles.x = glm::clamp(structure.eulerAngles.x, -glm::radians(90.0f), glm::radians(90.0f));

    DragScalarFloats("fov", structure.fov, { 0.0f, 180.0f }, 0, "%.0f");
    ResetButton("fov", structure.fov, defaultValues.fov);

    DragScalarFloats("nearPlane", structure.near, { 0.01f, structure.far }, ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic, "%.2f");
    ResetButton("nearPlane", structure.near, std::min(structure.far, defaultValues.near));

    DragScalarFloats("farPlane", structure.far, { structure.near + 0.01f, 1'000'000.0f }, ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic, "%.2f");
    ResetButton("farPlane", structure.far, std::max(structure.near, defaultValues.far));

    ImGui::EndGroup();
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
    ImGui::BeginGroup();
    ImGui::Text("Debug Lines");

    ImGui::BeginDisabled(!structure.pipeline || !structure.indices || !structure.vertices);
    ImGui::Checkbox("enabled", &structure.enabled);
    ResetButton("enabled", structure.enabled, false);
    ImGui::EndDisabled();

    DragScalarFloats("lineWidth", structure.lineWidth, { 1.0, 10.0 });
    ResetButton("lineWidth", structure.lineWidth, 1.0f);

    imguiStructureDisplay(structure.lastFrameDrawResults);

    ImGui::EndGroup();
}

template<>
void imguiStructureControls<ShadowPassParameters>(
    ShadowPassParameters& structure
    , ShadowPassParameters const& defaultValues
)
{
    ImGui::BeginGroup();
    ImGui::Text("Shadow Pass Parameters");
    ResetButton("shadowPassParameters", structure, defaultValues);

    ImGui::DragFloat("Depth Bias Constant", &structure.depthBiasConstant);
    ResetButton("depthBiasConstant", structure.depthBiasConstant, defaultValues.depthBiasConstant);

    ImGui::Text("Depth Bias Clamp is not supported."); // TODO: support depth bias clamp
    ImGui::BeginDisabled();
    ResetButton("depthBiasClamp", structure, defaultValues);
    ImGui::EndDisabled();

    ImGui::DragFloat("Depth Bias Slope", &structure.depthBiasSlope);
    ResetButton("depthBiasSlope", structure.depthBiasSlope, defaultValues.depthBiasSlope);

    ImGui::EndGroup();
}

template<>
void imguiStructureControls<SceneBounds>(
    SceneBounds& structure
    , SceneBounds const& defaultValues
)
{
    ImGui::BeginGroup();
    ImGui::Text("Scene Bounds");
    ResetButton("sceneBounds", structure, defaultValues);

    ImGui::DragFloat3("Scene Center", reinterpret_cast<float*>(&structure.center));
    ResetButton("sceneCenter", structure.center, defaultValues.center);

    ImGui::DragFloat3("Scene Extent", reinterpret_cast<float*>(&structure.extent));
    ResetButton("sceneExtent", structure.extent, defaultValues.extent);

    ImGui::EndGroup();
}