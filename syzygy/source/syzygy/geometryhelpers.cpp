#include "geometryhelpers.hpp"

#include "geometrystatics.hpp"
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <limits>

auto geometry::projectPointOnPlane(Plane const plane, glm::vec3 const point)
    -> glm::vec3
{
    glm::vec3 const toPoint{point - plane.point};
    glm::vec3 const projection{glm::dot(toPoint, plane.normal) * plane.normal};

    return projection + point;
}

auto geometry::collectAABBVertices(
    glm::vec3 const center, glm::vec3 const extent
) -> AABBVertices
{
    return AABBVertices{
        center + glm::vec3(extent.x, extent.y, extent.z),
        center + glm::vec3(extent.x, extent.y, -extent.z),
        center + glm::vec3(extent.x, -extent.y, extent.z),
        center + glm::vec3(extent.x, -extent.y, -extent.z),
        center + glm::vec3(-extent.x, extent.y, extent.z),
        center + glm::vec3(-extent.x, extent.y, -extent.z),
        center + glm::vec3(-extent.x, -extent.y, extent.z),
        center + glm::vec3(-extent.x, -extent.y, -extent.z),
    };
}

auto geometry::lookAtVk(
    glm::vec3 const eye, glm::vec3 const center, glm::vec3 const up
) -> glm::mat4x4
{
    return glm::scale(glm::vec3(1.0, -1.0, -1.0))
         * glm::lookAtRH(eye, center, up);
}

auto geometry::lookAtVkSafe(glm::vec3 const eye, glm::vec3 const center)
    -> glm::mat4x4
{
    float constexpr tolerance{0.99};

    float const cosine{glm::dot(forward, geometry::up)};

    bool const forwardIsUp{glm::abs(cosine) > tolerance};

    return geometry::lookAtVk(
        eye,
        center,
        forwardIsUp ? geometry::forward * glm::sign(cosine) : geometry::up
    );
}

auto geometry::projectionVk(PerspectiveProjectionParameters const parameters)
    -> glm::mat4x4
{
    float const swappedNear{parameters.far};
    float const swappedFar{parameters.near};

    return glm::perspectiveLH_ZO(
        glm::radians(parameters.fov_y),
        parameters.aspectRatio,
        swappedNear,
        swappedFar
    );
}

auto geometry::projectionOrthoVk(glm::vec3 const min, glm::vec3 const max)
    -> glm::mat4x4
{
    return glm::orthoLH_ZO(min.x, max.x, min.y, max.y, max.z, min.z);
}

auto geometry::forwardFromEulers(glm::vec3 const eulerAngles) -> glm::vec3
{
    return glm::orientate3(eulerAngles) * geometry::forward;
}

auto geometry::transformVk(
    glm::vec3 const position, glm::vec3 const eulerAngles
) -> glm::mat4x4
{
    return glm::translate(position) * glm::orientate4(eulerAngles);
}

auto geometry::viewVk(glm::vec3 const position, glm::vec3 const eulerAngles)
    -> glm::mat4x4
{
    return glm::inverse(transformVk(position, eulerAngles));
}

auto geometry::randomQuat() -> glm::quat
{
    // https://stackoverflow.com/a/56794499

    glm::vec2 const xy{glm::diskRand(1.0F)};
    glm::vec2 const uv{glm::diskRand(1.0F)};

    float const s{glm::sqrt((1 - glm::length2(xy)) / glm::length2(uv))};

    return {s * uv.y, xy.x, xy.y, s * uv.x};
}

auto geometry::projectionOrthoAABBVk(
    glm::mat4x4 const view,
    glm::vec3 const geometryCenter,
    glm::vec3 const geometryExtent
) -> glm::mat4x4
{
    // Project every vertex of the AABB supplied, to determine how large
    // the projection matrix needs to be

    std::array<glm::vec3, 8> const aabbVertices{
        geometry::collectAABBVertices(geometryCenter, geometryExtent)
    };

    glm::vec3 const centerViewSpace{view * glm::vec4(geometryCenter, 1.0)};
    glm::vec3 const forwardViewSpace{geometry::forward};

    glm::vec3 viewMax{std::numeric_limits<float>::lowest()};
    glm::vec3 viewMin{std::numeric_limits<float>::max()};
    for (glm::vec3 const vertex : aabbVertices)
    {
        glm::vec3 const vertexViewSpace{view * glm::vec4(vertex, 1.0)};
        glm::vec3 const projected{geometry::projectPointOnPlane(
            Plane{
                .point = centerViewSpace,
                .normal = forwardViewSpace,
            },
            vertexViewSpace
        )};

        viewMax = glm::max(projected, viewMax);
        viewMin = glm::min(projected, viewMin);
    }

    glm::mat4x4 const projection{geometry::projectionOrthoVk(viewMin, viewMax)};

    return projection;
}
