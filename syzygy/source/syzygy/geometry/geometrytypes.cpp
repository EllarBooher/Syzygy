#include "geometrytypes.hpp"

namespace syzygy
{
auto Ray::create(glm::vec3 from, glm::vec3 to) -> Ray
{
    return {.position = from, .direction = to - from};
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
} // namespace syzygy