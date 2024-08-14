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

auto szg_geometry::projectPointOnPlane(Plane const plane, glm::vec3 const point)
    -> glm::vec3
{
    glm::vec3 const toPoint{point - plane.point};
    glm::vec3 const projection{glm::dot(toPoint, plane.normal) * plane.normal};

    return projection + point;
}

auto szg_geometry::collectAABBVertices(
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

auto szg_geometry::lookAtVk(
    glm::vec3 const eye, glm::vec3 const center, glm::vec3 const up
) -> glm::mat4x4
{
    return glm::scale(glm::vec3(1.0, -1.0, -1.0))
         * glm::lookAtRH(eye, center, up);
}

auto szg_geometry::lookAtVkSafe(glm::vec3 const eye, glm::vec3 const center)
    -> glm::mat4x4
{
    float constexpr tolerance{0.99};

    float const cosine{glm::dot(forward, szg_geometry::up)};

    bool const forwardIsUp{glm::abs(cosine) > tolerance};

    return szg_geometry::lookAtVk(
        eye,
        center,
        forwardIsUp ? szg_geometry::forward * glm::sign(cosine)
                    : szg_geometry::up
    );
}

auto szg_geometry::projectionVk(PerspectiveProjectionParameters const parameters
) -> glm::mat4x4
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

auto szg_geometry::projectionOrthoVk(glm::vec3 const min, glm::vec3 const max)
    -> glm::mat4x4
{
    return glm::orthoLH_ZO(min.x, max.x, min.y, max.y, max.z, min.z);
}

auto szg_geometry::forwardFromEulers(glm::vec3 const eulerAngles) -> glm::vec3
{
    return glm::orientate3(eulerAngles) * szg_geometry::forward;
}

auto szg_geometry::transformVk(
    glm::vec3 const position, glm::vec3 const eulerAngles
) -> glm::mat4x4
{
    return glm::translate(position) * glm::orientate4(eulerAngles);
}

auto szg_geometry::viewVk(glm::vec3 const position, glm::vec3 const eulerAngles)
    -> glm::mat4x4
{
    return glm::inverse(transformVk(position, eulerAngles));
}

auto szg_geometry::randomQuat() -> glm::quat
{
    // https://stackoverflow.com/a/56794499

    glm::vec2 const xy{glm::diskRand(1.0F)};
    glm::vec2 const uv{glm::diskRand(1.0F)};

    float const s{glm::sqrt((1 - glm::length2(xy)) / glm::length2(uv))};

    return {s * uv.y, xy.x, xy.y, s * uv.x};
}

auto szg_geometry::projectionOrthoAABBVk(
    glm::mat4x4 const view,
    glm::vec3 const geometryCenter,
    glm::vec3 const geometryExtent
) -> glm::mat4x4
{
    // Project every vertex of the AABB supplied, to determine how large
    // the projection matrix needs to be

    std::array<glm::vec3, 8> const aabbVertices{
        szg_geometry::collectAABBVertices(geometryCenter, geometryExtent)
    };

    glm::vec3 const centerViewSpace{view * glm::vec4(geometryCenter, 1.0)};
    glm::vec3 const forwardViewSpace{szg_geometry::forward};

    glm::vec3 viewMax{std::numeric_limits<float>::lowest()};
    glm::vec3 viewMin{std::numeric_limits<float>::max()};
    for (glm::vec3 const vertex : aabbVertices)
    {
        glm::vec3 const vertexViewSpace{view * glm::vec4(vertex, 1.0)};
        glm::vec3 const projected{szg_geometry::projectPointOnPlane(
            Plane{
                .point = centerViewSpace,
                .normal = forwardViewSpace,
            },
            vertexViewSpace
        )};

        viewMax = glm::max(projected, viewMax);
        viewMin = glm::min(projected, viewMin);
    }

    glm::mat4x4 const projection{
        szg_geometry::projectionOrthoVk(viewMin, viewMax)
    };

    return projection;
}
