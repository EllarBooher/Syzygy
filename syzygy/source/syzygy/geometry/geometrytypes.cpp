#include "geometrytypes.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/vec_swizzle.hpp>

namespace syzygy
{
auto Ray::at(float const t) const -> glm::vec3
{
    return position + t * direction;
}
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

// The following two methods adapted from code licensed under Creative
// Commons CC BY-ND 3.0:
//
// Alexander Majercik, Cyril Crassin, Peter Shirley, and Morgan
// McGuire, A Ray-Box Intersection Algorithm and Efficient Dynamic Voxel
// Rendering, Journal of Computer Graphics Techniques (JCGT), vol. 7, no. 3,
// 66-81, 2018
//
// https://jcgt.org/published/0007/03/04/

auto AABB::raySimpleHitTest(Ray const ray) const -> bool
{
    glm::vec3 const invRayDirection{1.0F / glm::normalize(ray.direction)};
    glm::vec3 const t0 = (min() - ray.position) * invRayDirection;
    glm::vec3 const t1 = (max() - ray.position) * invRayDirection;
    glm::vec3 const tMin = glm::min(t0, t1);
    glm::vec3 const tMax = glm::max(t0, t1);

    return glm::compMax(tMin) < glm::compMin(tMax);
}

auto AABB::rayHitTest(Ray const ray) const -> HitResult
{
    Ray const rayBoxLocal{
        .position = ray.position - center,
        .direction = glm::normalize(ray.direction)
    };

    glm::vec3 const boxInverseRadius{1.0F / halfExtent};

    // Consider if we're looking for backface vs frontface collisions
    float const winding{
        glm::compMax(glm::abs(rayBoxLocal.position) * boxInverseRadius) < 1.0F
            ? -1.0F
            : 1.0F
    };

    glm::vec3 const directionSign{-1.0F * glm::sign(rayBoxLocal.direction)};
    // Distance to box plane
    glm::vec3 const faceDistance =
        (halfExtent * winding * directionSign - rayBoxLocal.position)
        / rayBoxLocal.direction;

    glm::bvec3 const test{
        faceDistance.x >= 0.0F
            && glm::all(glm::lessThan(
                glm::abs(
                    glm::yz(rayBoxLocal.position)
                    + glm::yz(rayBoxLocal.direction) * faceDistance.x
                ),
                glm::yz(halfExtent)
            )),
        faceDistance.y >= 0.0F
            && glm::all(glm::lessThan(
                glm::abs(
                    glm::zx(rayBoxLocal.position)
                    + glm::zx(rayBoxLocal.direction) * faceDistance.y
                ),
                glm::zx(halfExtent)
            )),
        faceDistance.z >= 0.0F
            && glm::all(glm::lessThan(
                glm::abs(
                    glm::xy(rayBoxLocal.position)
                    + glm::xy(rayBoxLocal.direction) * faceDistance.z
                ),
                glm::xy(halfExtent)
            ))
    };
    glm::vec3 const sign{
        test.x
            ? glm::vec3{directionSign.x, 0.0F, 0.0F}
            : (test.y ? glm::vec3{0.0F, directionSign.y, 0.0F}
                      : glm::vec3{0.0F, 0.0F, test.z ? directionSign.z : 0.0F})
    };

    float const hitDistance{
        sign.x != 0 ? faceDistance.x
                    : ((sign.y != 0) ? faceDistance.y : faceDistance.z)
    };

    return HitResult{
        .hit = sign.x != 0 || sign.y != 0 || sign.z != 0,
        .distance = hitDistance,
        .hitPosition =
            center + rayBoxLocal.position + hitDistance * rayBoxLocal.direction
    };
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