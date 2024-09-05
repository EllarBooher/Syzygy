#pragma once

#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/renderer/gputypes.hpp"

namespace syzygy
{
auto makeDirectional(
    glm::vec4 color, float strength, glm::vec3 eulerAngles, AABB capturedBounds

) -> DirectionalLightPacked;

struct SpotlightParams
{
    glm::vec4 color;
    float strength;
    float falloffFactor;
    float falloffDistance;
    float verticalFOVDegrees;
    float horizontalScale;
    glm::vec3 eulerAngles;
    glm::vec3 position;
    float near;
    float far;
};

// TODO: less parameters constructor
auto makeSpot(SpotlightParams params) -> SpotLightPacked;
} // namespace syzygy