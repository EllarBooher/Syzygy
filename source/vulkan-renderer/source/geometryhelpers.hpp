#pragma once

#include <array>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

/*
* An important aspect of these functions is that they abstract away the choice of handed-ness and coordinate systems.
* 
* The convention throughout this engine for world and view space is:
* +x is right
* +y is down
* +z is forward
* 
* This is a right handed coordinate system.
* 
* HOWEVER for depth values, used when in screen/clip/NDC space, 1.0 is near and 0.0 is far. 
* This leads to a better distribution of precision when mapping to 1/z.
* 
* All the functions present here produce matrices that respect these conventions.
*/

namespace geometry
{
    glm::vec3 projectPointOnPlane(glm::vec3 planePoint, glm::vec3 planeNormal, glm::vec3 point);
    std::array<glm::vec3, 8> collectAABBVertices(glm::vec3 center, glm::vec3 extent);

    // Creates a look at matrix that is properly right handed, with +x right, +y down, and +z forward.
    glm::mat4x4 lookAtVk(glm::vec3 eye, glm::vec3 center, glm::vec3 up);
    
    // Creates a look at matrix, with a fallback up direction in case the forward is already up.
    glm::mat4x4 lookAtVkSafe(glm::vec3 eye, glm::vec3 center);

    glm::mat4x4 projectionVk(float fov, float aspectRatio, float near, float far);

    // Creates an orthographic projection matrix that is right handed, with +x right, +y down,
    // and depth values mapped near is 1.0 and far is 0.0.
    glm::mat4x4 projectionOrthoVk(glm::vec3 min, glm::vec3 max);

    // Creates an orthographic projection that contains the entirety of a AABB.
    // This is useful for directional lights, since they cast on arbitrary amounts of geometry.
    // TODO: support aspect ratios.
    glm::mat4x4 projectionOrthoAABBVk(
        glm::mat4x4 view
        , glm::vec3 geometryCenter
        , glm::vec3 geometryExtent
    ); 

    glm::vec3 forwardFromEulers(glm::vec3 eulerAngles);

    glm::mat4x4 transformVk(glm::vec3 position, glm::vec3 eulerAngles);
    glm::mat4x4 viewVk(glm::vec3 position, glm::vec3 eulerAngles);
}