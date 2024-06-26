#pragma once

#include "enginetypes.hpp"
#include "geometryhelpers.hpp"

#include "gputypes.hpp"

namespace lights
{
static gputypes::LightDirectional makeDirectional(
    glm::vec4 const color,
    float const strength,
    glm::vec3 const eulerAngles,
    glm::vec3 const geometryCenter,
    glm::vec3 const geometryExtent
)
{
    glm::mat4x4 const view{geometry::viewVk(glm::vec3{0.0}, eulerAngles)};

    glm::mat4x4 const projection{
        geometry::projectionOrthoAABBVk(view, geometryCenter, geometryExtent)
    };

    return gputypes::LightDirectional{
        .color = color,
        .forward = glm::vec4{geometry::forwardFromEulers(eulerAngles), 0.0},
        .projection = projection,
        .view = view,
        .strength = strength,
    };
}

// TODO: less parameters constructor
static gputypes::LightSpot makeSpot(
    glm::vec4 const color,
    float const strength,
    float const falloffFactor,
    float const falloffDistance,
    float const verticalFOV,
    float const horizontalScale,
    glm::vec3 const eulerAngles,
    glm::vec3 const position,
    float const near,
    float const far
)
{
    return gputypes::LightSpot{
        .color = color,
        .forward = glm::vec4{geometry::forwardFromEulers(eulerAngles), 0.0},
        .projection =
            geometry::projectionVk(geometry::PerspectiveProjectionParameters{
                .fov_y = verticalFOV,
                .aspectRatio = horizontalScale,
                .near = near,
                .far = far,
            }),
        .view = geometry::viewVk(position, eulerAngles),
        .position = glm::vec4(position, 1.0),
        .strength = strength,
        .falloffFactor = falloffFactor,
        .falloffDistance = falloffDistance,
    };
}
} // namespace lights