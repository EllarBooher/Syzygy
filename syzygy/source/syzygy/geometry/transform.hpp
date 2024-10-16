#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace syzygy
{
struct Ray;
} // namespace syzygy

namespace syzygy
{
struct Transform
{
    glm::vec3 translation{0.0F};
    glm::vec3 eulerAnglesRadians{0.0F};
    glm::vec3 scale{1.0F};

    [[nodiscard]] static auto identity() -> Transform;
    [[nodiscard]] auto toMatrix() const -> glm::mat4x4;
    [[nodiscard]] static auto lookAt(Ray eyeTarget, glm::vec3 scale)
        -> Transform;
};
} // namespace syzygy