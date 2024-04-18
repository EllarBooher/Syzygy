#include "engineui.hpp"

#include <imgui.h>
#include <implot.h>
#include <fmt/format.h>

#include "../shaders.hpp"
#include "../engineparams.hpp"

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
        if (ImPlot::BeginPlot("FPS"))
        {
            ImPlot::SetupAxes("", "FPS", ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock, ImPlotAxisFlags_LockMin);
            ImPlot::SetupAxesLimits(0, fpsValues.size(), 0.0f, 320.0f);
            ImPlot::PlotLine("Test", fpsValues.data(), fpsValues.size());
            ImPlot::PlotInfLines("Current Frame", &currentFrame, 1);
            ImPlot::EndPlot();
        }
    }
    ImGui::End();
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
    , std::span<float> values
    , float min
    , float max
    , ImGuiSliderFlags flags = 0
    , std::string format = "%f"
)
{
    ImGui::DragScalarN(
        label.c_str()
        , ImGuiDataType_Float
        , values.data(), values.size()
        , 0.0f
        , &min
        , &max
        , format.c_str()
        , flags
    );
}

template<typename T>
struct is_glm_vec : std::false_type
{};

template<size_t N, typename T>
struct is_glm_vec<glm::vec<N, T>> : std::true_type
{};

template<typename T>
static void DragScalarFloats(
    std::string const& label
    , T& values
    , float min
    , float max
    , ImGuiSliderFlags flags = 0
    , std::string format = "%f"
)
{
    static_assert(sizeof(T) % 4 == 0);
    static_assert(is_glm_vec<T>::value || std::is_same<T, float>::value);
    size_t constexpr N = sizeof(T) / 4;
    DragScalarFloats(label, std::span<float>(reinterpret_cast<float*>(&values), N), min, max, flags, format);
}

// Templating for value types that cannot be implicitely converted to span of floats.
template<typename T>
static void FractionalCoefficientSlider(std::string const& label, T& values)
{
    DragScalarFloats(
        label
        , values
        , 0.0
        , 1.0
        , ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic
        , "%.8f"
    );
}

template<>
void imguiStructureControls<AtmosphereParameters>(AtmosphereParameters& atmosphere, AtmosphereParameters const& defaultValues)
{
    ImGui::BeginGroup();
    ImGui::Text("Atmosphere Parameters");
    ResetButton("atmosphereParameters", atmosphere, defaultValues);

    atmosphere.directionToSun = glm::normalize(atmosphere.directionToSun);
    DragScalarFloats("Direction to Sun", atmosphere.directionToSun, -1.0f, 1.0f);
    ResetButton("directionToSun", atmosphere.directionToSun, defaultValues.directionToSun);

    DragScalarFloats(
        "Earth Radius (meters)", atmosphere.earthRadiusMeters
        , 1.0f, atmosphere.atmosphereRadiusMeters
        , ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic
    );
    ResetButton("earthRadiusMeters", atmosphere.earthRadiusMeters, defaultValues.earthRadiusMeters);

    DragScalarFloats(
        "Atmosphere Radius (meters)", atmosphere.atmosphereRadiusMeters
        , atmosphere.earthRadiusMeters, 1'000'000'000.0f
        , ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic
    );
    ResetButton("atmosphereRadiusMeters", atmosphere.atmosphereRadiusMeters, defaultValues.atmosphereRadiusMeters);

    FractionalCoefficientSlider("Rayleigh Scattering Coefficient", atmosphere.scatteringCoefficientRayleigh);
    ResetButton("scatteringCoefficientRayleigh", atmosphere.scatteringCoefficientRayleigh, defaultValues.scatteringCoefficientRayleigh);

    DragScalarFloats("Rayleigh Altitude Decay", atmosphere.altitudeDecayRayleigh, 0.0f, 10'000.0f);
    ResetButton("altitudeDecayRayleigh", atmosphere.altitudeDecayRayleigh, defaultValues.altitudeDecayRayleigh);

    FractionalCoefficientSlider("Mie Scattering Coefficient", atmosphere.scatteringCoefficientMie);
    ResetButton("scatteringCoefficientMie", atmosphere.scatteringCoefficientMie, defaultValues.scatteringCoefficientMie);

    DragScalarFloats("Mie Altitude Decay", atmosphere.altitudeDecayMie, 0.0f, 10'000.0f);
    ResetButton("altitudeDecayMie", atmosphere.altitudeDecayMie, defaultValues.altitudeDecayMie);

    ImGui::EndGroup();
}

template<>
void imguiStructureControls<CameraParameters>(CameraParameters& structure, CameraParameters const& defaultValues)
{
    ImGui::BeginGroup();
    ImGui::Text("Camera Parameters");
    ResetButton("cameraParameters", structure, defaultValues);

    ImGui::DragScalarN("cameraPosition", ImGuiDataType_Float, &structure.cameraPosition, 3, 0.2f);
    ResetButton("cameraPosition", structure.cameraPosition, defaultValues.cameraPosition);

    DragScalarFloats("eulerAngles", structure.eulerAngles, glm::radians(-90.0f), glm::radians(90.0f));
    ResetButton("eulerAngles", structure.eulerAngles, defaultValues.eulerAngles);

    DragScalarFloats("fov", structure.fov, 0.0f, 180.0f, 0, "%.0f");
    ResetButton("fov", structure.fov, defaultValues.fov);

    DragScalarFloats("nearPlane", structure.near, 0.01f, structure.far, ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic, "%.2f");
    ResetButton("nearPlane", structure.near, std::min(structure.far, defaultValues.near));

    DragScalarFloats("farPlane", structure.far, structure.near + 0.01f, 1'000'000.0f, ImGuiSliderFlags_::ImGuiSliderFlags_Logarithmic, "%.2f");
    ResetButton("farPlane", structure.far, std::max(structure.near, defaultValues.far));

    ImGui::EndGroup();
}