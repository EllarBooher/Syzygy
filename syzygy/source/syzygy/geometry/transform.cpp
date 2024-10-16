#include "transform.hpp"

#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>

namespace syzygy
{
auto Transform::identity() -> Transform { return Transform{}; }
auto Transform::toMatrix() const -> glm::mat4x4
{
    return glm::translate(translation) * glm::orientate4(eulerAnglesRadians)
         * glm::scale(scale);
}

auto Transform::lookAt(Ray const eyeTarget, glm::vec3 const scale) -> Transform
{
    glm::vec3 const forward{glm::normalize(eyeTarget.direction)};

    glm::vec3 const eulerAngles{eulersFromForward(forward)};

    return Transform{
        .translation = eyeTarget.position,
        .eulerAnglesRadians = eulerAngles,
        .scale = scale,
    };
}
} // namespace syzygy