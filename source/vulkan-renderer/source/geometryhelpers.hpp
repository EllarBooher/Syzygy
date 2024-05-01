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

    // Creates an orthographic projection matrix that is right handed, with +x right, +y down,
    // and depth values mapped near is 1.0 and far is 0.0.
    glm::mat4x4 projectionOrthoVk(glm::vec3 min, glm::vec3 max);
}