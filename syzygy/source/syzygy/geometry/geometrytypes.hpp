#pragma once

#include "syzygy/platform/integer.hpp"
#include <array>
#include <glm/vec3.hpp>

namespace syzygy
{
struct Ray
{
    glm::vec3 position;
    // Possibly unnormalized
    glm::vec3 direction;

    static auto create(glm::vec3 from, glm::vec3 to) -> Ray;
};

struct AABB
{
    static size_t constexpr VERTEX_COUNT{8ULL};
    using Vertices = std::array<glm::vec3, VERTEX_COUNT>;

    [[nodiscard]] auto collectVertices() const -> Vertices;

    glm::vec3 center;
    glm::vec3 halfExtent;
};

} // namespace syzygy