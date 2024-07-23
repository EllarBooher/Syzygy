#include "scene.hpp"

#include "../geometrystatics.hpp"
#include "../lights.hpp"

/*
 * Values derived from:
 * https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html
 * Which is based on the original paper "Display of the Earth Taking into
 * Account Atmospheric Scattering" by Tomoyuki Nishita, Takao Sirai, Katsumi
 * Tadamura, Eihachiro Nakamae
 */
scene::Atmosphere const scene::Atmosphere::DEFAULT_VALUES_EARTH{
    scene::Atmosphere{
        .sunEulerAngles = glm::vec3(1.0, 0.0, 0.0),

        .earthRadiusMeters = 6378000,
        .atmosphereRadiusMeters = 6420000,

        .groundColor = glm::vec3{0.9, 0.8, 0.6},

        .scatteringCoefficientRayleigh =
            glm::vec3(0.0000038, 0.0000135, 0.0000331),
        .altitudeDecayRayleigh = 7994.0,

        .scatteringCoefficientMie = glm::vec3(0.000021),
        .altitudeDecayMie = 1200.0,
    }
};

namespace
{
auto tickSunEulerAngles(
    scene::Atmosphere const& atmosphere, TickTiming const timing
) -> glm::vec3
{
    scene::Atmosphere::SunAnimation const animation{atmosphere.animation};

    if (!animation.animateSun)
    {
        return atmosphere.sunEulerAngles;
    }

    // position of sun as proxy for time
    float const time{glm::dot(geometry::up, atmosphere.directionToSun())};

    float constexpr TIME_NIGHT_COSINE{-0.11F};
    float const SUNRISE_ANGLE_RADIANS{glm::asin(0.1F)};

    bool const isNight{time < TIME_NIGHT_COSINE};

    glm::vec3 finalAngles{atmosphere.sunEulerAngles};
    if (isNight && animation.skipNight)
    {
        // Skip to the right horizon along the sun's great circle path
        if (animation.animationSpeed > 0.0)
        {
            finalAngles.x = glm::pi<float>() - SUNRISE_ANGLE_RADIANS;
        }
        else
        {
            finalAngles.x = SUNRISE_ANGLE_RADIANS;
        }
    }
    else
    {
        finalAngles.x += static_cast<float>(timing.deltaTimeSeconds)
                       * animation.animationSpeed;
    }

    glm::vec3 constexpr MAX_ANGLES_RADIANS{glm::vec3(glm::two_pi<float>())};
    return glm::mod(finalAngles, MAX_ANGLES_RADIANS);
}
} // namespace

void scene::Scene::tick(TickTiming const lastFrame)
{
    atmosphere.sunEulerAngles = tickSunEulerAngles(atmosphere, lastFrame);
}

namespace
{
auto createSunlight(
    scene::SceneBounds const sceneBounds,
    glm::vec3 const sunEulerAngles,
    glm::vec3 const sunlightRGB
) -> gputypes::LightDirectional
{
    float constexpr SUNLIGHT_STRENGTH{0.5F};

    return lights::makeDirectional(
        glm::vec4(sunlightRGB, 1.0),
        SUNLIGHT_STRENGTH,
        sunEulerAngles,
        sceneBounds.center,
        sceneBounds.extent
    );
}
auto createMoonlight(
    scene::SceneBounds const sceneBounds,
    float const sunCosine,
    float const sunsetCosine
) -> gputypes::LightDirectional
{
    float constexpr MOONRISE_LENGTH{0.08};

    float const moonlightStrength{
        0.1F
        * glm::clamp(
            0.0F, 1.0F, glm::abs(sunCosine - sunsetCosine) / MOONRISE_LENGTH
        )
    };

    glm::vec4 constexpr MOONLIGHT_COLOR_RGBA{0.3, 0.4, 0.6, 1.0};
    glm::vec3 constexpr STRAIGHT_DOWN_EULER_ANGLES{
        -glm::half_pi<float>(), 0.0F, 0.0F
    };

    return lights::makeDirectional(
        MOONLIGHT_COLOR_RGBA,
        moonlightStrength,
        STRAIGHT_DOWN_EULER_ANGLES,
        sceneBounds.center,
        sceneBounds.extent
    );
}

// Returns an estimate of the color of sunlight that has reached the
// origin.
auto computeSunlight(scene::Atmosphere const& atmosphere) -> glm::vec4
{
    float const surfaceCosine{
        glm::dot(atmosphere.directionToSun(), glm::vec3{0.0, -1.0, 0.0})
    };
    if (surfaceCosine <= 0.0)
    {
        return {0.0, 0.0, 0.0, 1.0};
    }

    glm::vec3 const start{0.0, -atmosphere.earthRadiusMeters, 0.0};
    float outDistance{0.0};
    if (!glm::intersectRaySphere(
            start,
            atmosphere.directionToSun(),
            glm::vec3(0.0),
            atmosphere.atmosphereRadiusMeters
                * atmosphere.atmosphereRadiusMeters,
            outDistance
        ))
    {
        glm::vec4 constexpr RAW_SUNLIGHT_COLOR{1.0, 1.0, 1.0, 1.0};
        return RAW_SUNLIGHT_COLOR;
    };

    float const atmosphereThickness{outDistance};

    // Calculations derived from sky.comp, we do a single ray straight up
    // to get an idea of the ambient color
    float const opticalDepthRayleigh{
        atmosphere.altitudeDecayRayleigh / surfaceCosine
        * (1.0F
           - std::exp(-atmosphereThickness / atmosphere.altitudeDecayRayleigh))
    };
    float const opticalDepthMie{
        atmosphere.altitudeDecayMie / surfaceCosine
        * (1.0F - std::exp(-atmosphereThickness / atmosphere.altitudeDecayMie))
    };

    glm::vec3 const tau{
        atmosphere.scatteringCoefficientRayleigh * opticalDepthRayleigh
        + 1.1F * atmosphere.scatteringCoefficientMie * opticalDepthMie
    };
    glm::vec3 const attenuation{glm::exp(-tau)};

    return {attenuation, 1.0};
}
} // namespace

auto scene::Atmosphere::directionToSun() const -> glm::vec3
{
    return -geometry::forwardFromEulers(sunEulerAngles);
}

auto scene::Atmosphere::toDeviceEquivalent() const -> gputypes::Atmosphere
{
    // TODO: move these computations out to somewhere more sensible

    glm::vec4 const sunlight{computeSunlight(*this)};
    glm::vec3 const sunDirection{glm::normalize(directionToSun())};

    return gputypes::Atmosphere{
        .directionToSun = sunDirection,
        .earthRadiusMeters = earthRadiusMeters,
        .scatteringCoefficientRayleigh = scatteringCoefficientRayleigh,
        .altitudeDecayRayleigh = altitudeDecayRayleigh,
        .scatteringCoefficientMie = scatteringCoefficientMie,
        .altitudeDecayMie = altitudeDecayMie,
        .ambientColor = glm::vec3(sunlight) * groundColor
                      * glm::dot(sunDirection, glm::vec3(0.0, -1.0, 0.0)),
        .atmosphereRadiusMeters = atmosphereRadiusMeters,
        .sunlightColor = glm::vec3(sunlight),
        .groundColor = groundColor,
    };
}

auto scene::Atmosphere::baked(SceneBounds const sceneBounds) const
    -> AtmosphereBaked
{
    gputypes::Atmosphere const atmosphere{toDeviceEquivalent()};

    // position of sun as proxy for time
    float const sunCosine{glm::dot(geometry::up, atmosphere.directionToSun)};
    float constexpr SUNSET_COSINE{0.06};

    std::optional<gputypes::LightDirectional> sunlight{};
    std::optional<gputypes::LightDirectional> moonlight{};

    if (sunCosine > 0.0F)
    {
        sunlight = createSunlight(
            sceneBounds, sunEulerAngles, atmosphere.sunlightColor
        );
    }
    if (sunCosine < SUNSET_COSINE)
    {
        moonlight = createMoonlight(sceneBounds, sunCosine, SUNSET_COSINE);
    }

    return AtmosphereBaked{
        .atmosphere = atmosphere,
        .sunlight = sunlight,
        .moonlight = moonlight,
    };
}
