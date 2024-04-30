#include "geometryhelpers.hpp"

#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>

glm::vec3 geometry::projectPointOnPlane(glm::vec3 const planePoint, glm::vec3 const planeNormal, glm::vec3 const point)
{
    glm::vec3 const toPoint{ point - planePoint };
    glm::vec3 const projection{ glm::dot(toPoint, planeNormal) * planeNormal };

    return projection + point;
}

std::array<glm::vec3, 8> geometry::collectAABBVertices(glm::vec3 center, glm::vec3 extent)
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

glm::mat4x4 geometry::lookAtVk(glm::vec3 eye, glm::vec3 center, glm::vec3 up)
{
    return glm::scale(glm::vec3(1.0, -1.0, -1.0)) * glm::lookAtRH(
        eye
        , center
        , up
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
