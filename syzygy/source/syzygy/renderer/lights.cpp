#include "lights.hpp"

#include "syzygy/geometry/geometryhelpers.hpp"

namespace syzygy
{
auto makeDirectional(
    glm::vec4 const color,
    float const strength,
    glm::vec3 const eulerAngles,
    AABB const capturedBounds
) -> DirectionalLightPacked
{
    glm::mat4x4 const view{viewVk(glm::vec3{0.0}, eulerAngles)};

    glm::mat4x4 const projection{projectionOrthoAABBVk(view, capturedBounds)};

    return DirectionalLightPacked{
        .color = color,
        .forward = glm::vec4{forwardFromEulers(eulerAngles), 0.0},
        .projection = projection,
        .view = view,
        .strength = strength,
    };
}

auto makeSpot(SpotlightParams const params) -> SpotLightPacked
{
    return SpotLightPacked{
        .color = params.color,
        .forward = glm::vec4{forwardFromEulers(params.eulerAngles), 0.0},
        .projection = projectionVk(PerspectiveProjectionParameters{
            .fov_y_degrees = params.verticalFOVDegrees,
            .aspectRatio = params.horizontalScale,
            .near = params.near,
            .far = params.far,
        }),
        .view = viewVk(params.position, params.eulerAngles),
        .position = glm::vec4(params.position, 1.0),
        .strength = params.strength,
        .falloffFactor = params.falloffFactor,
        .falloffDistance = params.falloffDistance,
    };
}
} // namespace syzygy