#include "geometryhelpers.hpp"

#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "geometrystatics.hpp"

glm::vec3 geometry::projectPointOnPlane(
    glm::vec3 const planePoint
    , glm::vec3 const planeNormal
    , glm::vec3 const point
)
{
    glm::vec3 const toPoint{ point - planePoint };
    glm::vec3 const projection{ glm::dot(toPoint, planeNormal) * planeNormal };

    return projection + point;
}

std::array<glm::vec3, 8> geometry::collectAABBVertices(
    glm::vec3 center
    , glm::vec3 extent
)
{
    return std::array<glm::vec3, 8>{
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

glm::mat4x4 geometry::lookAtVk(
    glm::vec3 eye
    , glm::vec3 center
    , glm::vec3 up
)
{
    return glm::scale(glm::vec3(1.0, -1.0, -1.0)) 
        * glm::lookAtRH(
            eye
            , center
            , up
    );
}

glm::mat4x4 geometry::lookAtVkSafe(
    glm::vec3 eye
    , glm::vec3 center
)
{
    float constexpr tolerance{ 0.99 };

    float const cosine{ glm::dot(forward, geometry::up) };

    bool const forwardIsUp{ glm::abs(cosine) > tolerance };

    return geometry::lookAtVk(
        eye
        , center
        , forwardIsUp ? geometry::forward * glm::sign(cosine) : geometry::up
    );
}

glm::mat4x4 geometry::projectionVk(
    float const fov
    , float const aspectRatio
    , float const near
    , float const far
)
{
    return glm::perspectiveLH_ZO(
        glm::radians(fov)
        , aspectRatio
        , far
        , near
    );
}

glm::mat4x4 geometry::projectionOrthoVk(glm::vec3 min, glm::vec3 max)
{
    return glm::orthoLH_ZO(
        min.x
        , max.x
        , min.y
        , max.y
        , max.z
        , min.z
    );
}

glm::vec3 geometry::forwardFromEulers(glm::vec3 eulerAngles)
{
    return glm::orientate3(eulerAngles) * geometry::forward;
}

glm::mat4x4 geometry::transformVk(glm::vec3 position, glm::vec3 eulerAngles)
{
    return glm::translate(position) * glm::orientate4(eulerAngles);
}

glm::mat4x4 geometry::viewVk(glm::vec3 position, glm::vec3 eulerAngles)
{
    return glm::inverse(transformVk(position, eulerAngles));
}

glm::mat4x4 geometry::projectionOrthoAABBVk(
    glm::mat4x4 const view
    , glm::vec3 const geometryCenter
    , glm::vec3 const geometryExtent
)
{
    // Project every vertex of the AABB supplied, to determine how large the projection matrix needs to be

    std::array<glm::vec3, 8> const aabbVertices{ 
        geometry::collectAABBVertices(geometryCenter, geometryExtent) 
    };

    glm::vec3 const centerViewSpace{ view * glm::vec4(geometryCenter,1.0) };
    glm::vec3 const forwardViewSpace{ geometry::forward };

    glm::vec3 viewMax{ std::numeric_limits<float>::lowest() };
    glm::vec3 viewMin{ std::numeric_limits<float>::max() };
    for (glm::vec3 const vertex : aabbVertices)
    {
        glm::vec3 const vertexViewSpace{ view * glm::vec4(vertex,1.0) };
        glm::vec3 const projected{ 
            geometry::projectPointOnPlane(
                centerViewSpace
                , forwardViewSpace
                , vertexViewSpace
            )
        };

        viewMax = glm::max(projected, viewMax);
        viewMin = glm::min(projected, viewMin);
    }

    float const viewExtentX = viewMax.x - viewMin.x;

    glm::mat4x4 const projection{
        geometry::projectionOrthoVk(viewMin, viewMax)
    };
    
    return projection;
}
