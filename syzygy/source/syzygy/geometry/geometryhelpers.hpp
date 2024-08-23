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
// The creation of a right-handed projection matrix with flipped depth
// values is difficult, and this is just one difficulty that these helpers aid
// on.
namespace syzygy
{
using AABBVertices = std::array<glm::vec3, 8>;

struct Plane
{
    glm::vec3 point;
    glm::vec3 normal;
};

auto projectPointOnPlane(Plane plane, glm::vec3 point) -> glm::vec3;

auto collectAABBVertices(glm::vec3 center, glm::vec3 extent) -> AABBVertices;

auto lookAtVk(glm::vec3 eye, glm::vec3 center, glm::vec3 up) -> glm::mat4x4;

// Creates a look at matrix, with a fallback up direction in case
// the forward (center - eye) is already WORLD_UP.
auto lookAtVkSafe(glm::vec3 eye, glm::vec3 center) -> glm::mat4x4;

struct PerspectiveProjectionParameters
{
    float fov_y;
    float aspectRatio;
    float near;
    float far;
};

auto projectionVk(PerspectiveProjectionParameters parameters) -> glm::mat4x4;

auto projectionOrthoVk(glm::vec3 min, glm::vec3 max) -> glm::mat4x4;

// Creates an orthographic projection that contains the entirety of a AABB.
// TODO: support aspect ratios.
auto projectionOrthoAABBVk(
    glm::mat4x4 view, glm::vec3 geometryCenter, glm::vec3 geometryExtent
) -> glm::mat4x4;

auto forwardFromEulers(glm::vec3 eulerAngles) -> glm::vec3;

auto transformVk(glm::vec3 position, glm::vec3 eulerAngles) -> glm::mat4x4;

auto viewVk(glm::vec3 position, glm::vec3 eulerAngles) -> glm::mat4x4;

auto randomQuat() -> glm::quat;
} // namespace syzygy