#pragma once

#include "syzygy/platform/integer.hpp"
#include <array>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/vec3.hpp>

namespace syzygy
{
struct Transform;
} // namespace syzygy

namespace syzygy
{
struct Ray
{
    glm::vec3 position;
    // Possibly unnormalized
    glm::vec3 direction;

    auto at(float) const -> glm::vec3;
    static auto create(glm::vec3 from, glm::vec3 to) -> Ray;
};

struct HitResult
{
    bool hit;
    float distance;
    glm::vec3 hitPosition;
};

struct AABB
{
    static size_t constexpr VERTEX_COUNT{8ULL};
    using Vertices = std::array<glm::vec3, VERTEX_COUNT>;

    static auto create(glm::vec3 min, glm::vec3 max) -> AABB;

    [[nodiscard]] auto raySimpleHitTest(Ray) const -> bool;
    [[nodiscard]] auto rayHitTest(Ray) const -> HitResult;

    [[nodiscard]] auto constexpr translate(glm::vec3 const translation) const
        -> AABB
    {
        AABB result{*this};
        result.center += translation;
        return result;
    }

    [[nodiscard]] auto collectVertices() const -> Vertices;
    [[nodiscard]] auto transformed(Transform) const -> Vertices;
    [[nodiscard]] auto min() const -> glm::vec3;
    [[nodiscard]] auto max() const -> glm::vec3;

    glm::vec3 center;
    glm::vec3 halfExtent;
};

} // namespace syzygy