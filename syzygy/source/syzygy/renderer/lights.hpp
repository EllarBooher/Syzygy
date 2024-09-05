#pragma once

#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/renderer/gputypes.hpp"

namespace syzygy
{
auto makeDirectional(
    glm::vec4 const color,
    float const strength,
    glm::vec3 const eulerAngles,
    AABB const sceneBounds
) -> DirectionalLightPacked
{
    glm::mat4x4 const view{syzygy::viewVk(glm::vec3{0.0}, eulerAngles)};

    glm::mat4x4 const projection{
        syzygy::projectionOrthoAABBVk(view, sceneBounds)
    };

    return DirectionalLightPacked{
        .color = color,
        .forward = glm::vec4{syzygy::forwardFromEulers(eulerAngles), 0.0},
        .projection = projection,
        .view = view,
        .strength = strength,
    };
}

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
auto makeSpot(SpotlightParams const params) -> SpotLightPacked
{
    return SpotLightPacked{
        .color = params.color,
        .forward =
            glm::vec4{syzygy::forwardFromEulers(params.eulerAngles), 0.0},
        .projection =
            syzygy::projectionVk(syzygy::PerspectiveProjectionParameters{
                .fov_y_degrees = params.verticalFOVDegrees,
                .aspectRatio = params.horizontalScale,
                .near = params.near,
                .far = params.far,
            }),
        .view = syzygy::viewVk(params.position, params.eulerAngles),
        .position = glm::vec4(params.position, 1.0),
        .strength = params.strength,
        .falloffFactor = params.falloffFactor,
        .falloffDistance = params.falloffDistance,
    };
}
} // namespace syzygy