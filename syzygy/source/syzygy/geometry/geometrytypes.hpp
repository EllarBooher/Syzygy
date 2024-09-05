#pragma once

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
} // namespace syzygy