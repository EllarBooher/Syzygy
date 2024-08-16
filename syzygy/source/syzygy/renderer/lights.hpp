#pragma once

#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/renderer/gputypes.hpp"

namespace syzygy
{
static DirectionalLightPacked makeDirectional(
    glm::vec4 const color,
    float const strength,
    glm::vec3 const eulerAngles,
    glm::vec3 const geometryCenter,
    glm::vec3 const geometryExtent
)
{
    glm::mat4x4 const view{syzygy::viewVk(glm::vec3{0.0}, eulerAngles)};

    glm::mat4x4 const projection{
        syzygy::projectionOrthoAABBVk(view, geometryCenter, geometryExtent)
    };

    return DirectionalLightPacked{
        .color = color,
        .forward = glm::vec4{syzygy::forwardFromEulers(eulerAngles), 0.0},
        .projection = projection,
        .view = view,
        .strength = strength,
    };
}

// TODO: less parameters constructor
static SpotLightPacked makeSpot(
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
    return SpotLightPacked{
        .color = color,
        .forward = glm::vec4{syzygy::forwardFromEulers(eulerAngles), 0.0},
        .projection =
            syzygy::projectionVk(syzygy::PerspectiveProjectionParameters{
                .fov_y = verticalFOV,
                .aspectRatio = horizontalScale,
                .near = near,
                .far = far,
            }),
        .view = syzygy::viewVk(position, eulerAngles),
        .position = glm::vec4(position, 1.0),
        .strength = strength,
        .falloffFactor = falloffFactor,
        .falloffDistance = falloffDistance,
    };
}
} // namespace syzygy