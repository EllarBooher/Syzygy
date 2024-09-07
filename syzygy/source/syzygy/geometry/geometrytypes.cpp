#include "geometrytypes.hpp"

#include "syzygy/geometry/transform.hpp"
#include <glm/common.hpp>

namespace syzygy
{
auto Ray::create(glm::vec3 from, glm::vec3 to) -> Ray
{
    return {.position = from, .direction = to - from};
}
auto AABB::create(glm::vec3 const min, glm::vec3 const max) -> AABB
{
    glm::vec3 const safeMin = glm::min(min, max);
    glm::vec3 const safeMax = glm::max(min, max);

    glm::vec3 const center = 0.5F * (safeMax + safeMin);

    return AABB{.center = center, .halfExtent = safeMax - center};
}
auto AABB::collectVertices() const -> Vertices
{
    return {
        center + glm::vec3(halfExtent.x, halfExtent.y, halfExtent.z),
        center + glm::vec3(halfExtent.x, halfExtent.y, -halfExtent.z),
        center + glm::vec3(halfExtent.x, -halfExtent.y, halfExtent.z),
        center + glm::vec3(halfExtent.x, -halfExtent.y, -halfExtent.z),
        center + glm::vec3(-halfExtent.x, halfExtent.y, halfExtent.z),
        center + glm::vec3(-halfExtent.x, halfExtent.y, -halfExtent.z),
        center + glm::vec3(-halfExtent.x, -halfExtent.y, halfExtent.z),
        center + glm::vec3(-halfExtent.x, -halfExtent.y, -halfExtent.z),
    };
}
auto AABB::min() const -> glm::vec3 { return center - glm::abs(halfExtent); }
auto AABB::max() const -> glm::vec3 { return center + glm::abs(halfExtent); }
} // namespace syzygy