#pragma once

#include "../geometryhelpers.hpp"
#include "../gputypes.hpp"
#include "timing.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/transform.hpp>
#include <optional>

namespace scene
{
struct SceneBounds
{
    glm::vec3 center{};
    glm::vec3 extent{};
};

struct AtmosphereBaked
{
    gputypes::Atmosphere atmosphere{};
    std::optional<gputypes::LightDirectional> sunlight{};
    std::optional<gputypes::LightDirectional> moonlight{};
};

struct Atmosphere
{
    static Atmosphere const DEFAULT_VALUES_EARTH;

    struct SunAnimation
    {
        bool animateSun{false};
        float animationSpeed{0.2f};
        bool skipNight{false};
    };

    SunAnimation animation{};

    glm::vec3 sunEulerAngles{0.0, 0.0, 0.0};

    float earthRadiusMeters{0.0};
    float atmosphereRadiusMeters{0.0};

    glm::vec3 groundColor{1.0};

    glm::vec3 scatteringCoefficientRayleigh{1.0};
    float altitudeDecayRayleigh{1.0};

    glm::vec3 scatteringCoefficientMie{1.0};
    float altitudeDecayMie{1.0};

    auto directionToSun() const -> glm::vec3;

    auto toDeviceEquivalent() const -> gputypes::Atmosphere;
    auto baked(SceneBounds) const -> AtmosphereBaked;
};

struct Scene
{
    Atmosphere atmosphere{Atmosphere::DEFAULT_VALUES_EARTH};

    void tick(TickTiming);
};
} // namespace scene