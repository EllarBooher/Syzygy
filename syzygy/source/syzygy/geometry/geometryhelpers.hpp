#pragma once

#include <array>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

// These functions help decide handed-ness and coordinate system.
// Renderer code generally still needs to consider handedness, but deciding
// which glm methods to use can be confusing. These helpers aid in that.
//
// The convention throughout this engine for world and view space is:
// +x is right
// +y is down
// +z is forward
//
// This is a right handed coordinate system.
//
// HOWEVER for depth values, we use 1.0 as near and 0.0 as far.
// We do this for better distribution of precision when mapping depths as 1/z.
//
// However, this results in a left-handed coordinate system.
// The creation of a right-handed projection matrix with flipped depth values
// is difficult, and this is just one difficulty that these helpers aid on.
namespace szg_geometry
{
typedef std::array<glm::vec3, 8> AABBVertices;

struct Plane
{
    glm::vec3 point;
    glm::vec3 normal;
};

glm::vec3 projectPointOnPlane(Plane plane, glm::vec3 point);

AABBVertices collectAABBVertices(glm::vec3 center, glm::vec3 extent);

glm::mat4x4 lookAtVk(glm::vec3 eye, glm::vec3 center, glm::vec3 up);

// Creates a look at matrix, with a fallback up direction in case
// the forward is already up.
glm::mat4x4 lookAtVkSafe(glm::vec3 eye, glm::vec3 center);

struct PerspectiveProjectionParameters
{
    float fov_y;
    float aspectRatio;
    float near;
    float far;
};

glm::mat4x4 projectionVk(PerspectiveProjectionParameters parameters);

glm::mat4x4 projectionOrthoVk(glm::vec3 min, glm::vec3 max);

// Creates an orthographic projection that contains the entirety of a AABB.
// TODO: support aspect ratios.
glm::mat4x4 projectionOrthoAABBVk(
    glm::mat4x4 view, glm::vec3 geometryCenter, glm::vec3 geometryExtent
);

glm::vec3 forwardFromEulers(glm::vec3 eulerAngles);

glm::mat4x4 transformVk(glm::vec3 position, glm::vec3 eulerAngles);

glm::mat4x4 viewVk(glm::vec3 position, glm::vec3 eulerAngles);

auto randomQuat() -> glm::quat;
} // namespace szg_geometry