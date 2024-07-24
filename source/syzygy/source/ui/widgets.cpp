#include "widgets.hpp"

#include "engineui.hpp"
#include "propertytable.hpp"
#include <fmt/format.h>
#include <implot.h>

void ui::performanceWindow(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    RingBuffer const& values,
    float& targetFPS
)
{
    UIWindow const window{
        UIWindow::beginDockable(std::format("{}##performance", title), dockNode)
    };
    if (!window.open)
    {
        return;
    }

    ImGui::Text("%s", fmt::format("FPS: {:.1f}", values.average()).c_str());
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

        std::span<double const> const fpsValues{values.values()};

        ImPlot::SetupAxesLimits(
            0,
            static_cast<double>(fpsValues.size()),
            DISPLAYED_FPS_MIN,
            DISPLAYED_FPS_MAX
        );

        ImPlot::PlotLine(
            "##fpsValues",
            fpsValues.data(),
            static_cast<int32_t>(fpsValues.size())
        );

        size_t const currentIndex{values.current()};
        ImPlot::PlotInfLines("##current", &currentIndex, 1);

        ImPlot::EndPlot();
    }
}

namespace
{
void uiAtmosphere(
    scene::Atmosphere& atmosphere, scene::Atmosphere const& defaultValues
)
{
    FloatBounds constexpr SUN_ANIMATION_SPEED_BOUNDS{-20.0F, 20.0F};

    float constexpr EULER_ANGLES_SPEED{0.1F};

    FloatBounds constexpr RGBA_BOUNDS{0.0F, 1.0F};

    float constexpr PLANETARY_RADIUS_MIN{1.0F};
    float constexpr PLANETARY_RADIUS_MAX{1'000'000'000.0F};

    // Scattering coefficient meaningfully exists over a very small and
    // unpredictable range. Thus finer controls are needed, and a speed of 0.1
    // or default 0.0 is too high.
    float constexpr SCATTERING_COEFFICIENT_SPEED{0.01F};
    FloatBounds constexpr SCATTERING_COEFFICIENT_BOUNDS{0.0F, 1.0F};

    FloatBounds constexpr ALTITUDE_DECAY_BOUNDS{0.0F, 1'000'000.0F};

    PropertyTable::begin()
        .rowBoolean(
            "Animate Sun",
            atmosphere.animation.animateSun,
            defaultValues.animation.animateSun
        )
        .rowFloat(
            "Sun Animation Speed",
            atmosphere.animation.animationSpeed,
            defaultValues.animation.animationSpeed,
            PropertySliderBehavior{
                .bounds = SUN_ANIMATION_SPEED_BOUNDS,
            }
        )
        .rowBoolean(
            "Skip Night",
            atmosphere.animation.skipNight,
            defaultValues.animation.skipNight
        )
        .rowVec3(
            "Sun Euler Angles",
            atmosphere.sunEulerAngles,
            defaultValues.sunEulerAngles,
            PropertySliderBehavior{
                .speed = EULER_ANGLES_SPEED,
            }
        )
        .rowReadOnlyVec3("Direction to Sun", atmosphere.directionToSun())
        .rowVec3(
            "Ground Diffuse Color",
            atmosphere.groundColor,
            defaultValues.groundColor,
            PropertySliderBehavior{
                .bounds = RGBA_BOUNDS,
            }
        )
        .rowFloat(
            "Earth Radius",
            atmosphere.earthRadiusMeters,
            defaultValues.earthRadiusMeters,
            PropertySliderBehavior{
                .bounds =
                    FloatBounds{
                        PLANETARY_RADIUS_MIN, atmosphere.atmosphereRadiusMeters
                    },
            }
        )
        .rowFloat(
            "Atmosphere Radius",
            atmosphere.atmosphereRadiusMeters,
            defaultValues.atmosphereRadiusMeters,
            PropertySliderBehavior{
                .bounds =
                    FloatBounds{
                        atmosphere.earthRadiusMeters, PLANETARY_RADIUS_MAX
                    },
            }
        )
        .rowVec3(
            "Rayleigh Scattering Coefficient",
            atmosphere.scatteringCoefficientRayleigh,
            defaultValues.scatteringCoefficientRayleigh,
            PropertySliderBehavior{
                .speed = SCATTERING_COEFFICIENT_SPEED,
                .bounds = SCATTERING_COEFFICIENT_BOUNDS,
            }
        )
        .rowFloat(
            "Rayleigh Altitude Decay",
            atmosphere.altitudeDecayRayleigh,
            defaultValues.altitudeDecayRayleigh,
            PropertySliderBehavior{
                .bounds = ALTITUDE_DECAY_BOUNDS,
            }
        )
        .rowVec3(
            "Mie Scattering Coefficient",
            atmosphere.scatteringCoefficientMie,
            defaultValues.scatteringCoefficientMie,
            PropertySliderBehavior{
                .speed = SCATTERING_COEFFICIENT_SPEED,
                .bounds = SCATTERING_COEFFICIENT_BOUNDS,
            }
        )
        .rowFloat(
            "Mie Altitude Decay",
            atmosphere.altitudeDecayMie,
            defaultValues.altitudeDecayMie,
            PropertySliderBehavior{
                .bounds = ALTITUDE_DECAY_BOUNDS,
            }
        )
        .end();
}
} // namespace

void ui::sceneControlsWindow(
    std::string const& title,
    std::optional<ImGuiID> const dockNode,
    scene::Scene& scene
)
{
    UIWindow const window{
        UIWindow::beginDockable(std::format("{}##scene", title), dockNode)
    };
    if (!window.open)
    {
        return;
    }

    if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
    {
        uiAtmosphere(scene.atmosphere, scene::Atmosphere::DEFAULT_VALUES_EARTH);
    }
}