#include "lights.hpp"

#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include <glm/mat4x4.hpp>

namespace syzygy
{
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