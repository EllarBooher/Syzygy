#pragma once

#include "syzygy/enginetypes.hpp"
#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/renderer/gputypes.hpp"

namespace szg_renderer
{
static LightDirectional makeDirectional(
    glm::vec4 const color,
    float const strength,
    glm::vec3 const eulerAngles,
    glm::vec3 const geometryCenter,
    glm::vec3 const geometryExtent
)
{
    glm::mat4x4 const view{szg_geometry::viewVk(glm::vec3{0.0}, eulerAngles)};

    glm::mat4x4 const projection{szg_geometry::projectionOrthoAABBVk(
        view, geometryCenter, geometryExtent
    )};

    return LightDirectional{
        .color = color,
        .forward = glm::vec4{szg_geometry::forwardFromEulers(eulerAngles), 0.0},
        .projection = projection,
        .view = view,
        .strength = strength,
    };
}

// TODO: less parameters constructor
static LightSpot makeSpot(
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
    return LightSpot{
        .color = color,
        .forward = glm::vec4{szg_geometry::forwardFromEulers(eulerAngles), 0.0},
        .projection = szg_geometry::projectionVk(
            szg_geometry::PerspectiveProjectionParameters{
                .fov_y = verticalFOV,
                .aspectRatio = horizontalScale,
                .near = near,
                .far = far,
            }
        ),
        .view = szg_geometry::viewVk(position, eulerAngles),
        .position = glm::vec4(position, 1.0),
        .strength = strength,
        .falloffFactor = falloffFactor,
        .falloffDistance = falloffDistance,
    };
}
} // namespace szg_renderer