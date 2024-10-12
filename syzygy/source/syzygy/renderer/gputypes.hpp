#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

// These types are used in shaders on the GPU.
// They contain padding and must faithfully represent their counterparts.
// These are intended to be trivially copyable to GPU memory, not for
// manipulation by UI or the engine.

namespace syzygy
{
// For ease of reading, group members by 16 bytes, which is a size of
// a single-precision vec4.
// The gpu equivalents will likely use std430 packing.

struct CameraPacked
{
    glm::mat4x4 projection;

    glm::mat4x4 inverseProjection;

    glm::mat4x4 view;

    glm::mat4x4 viewInverseTranspose;

    glm::mat4x4 rotation;

    glm::mat4x4 projViewInverse;

    glm::vec4 forwardWorld;

    glm::vec4 position;
};
// NOLINTNEXTLINE(readability-magic-numbers)
static_assert(sizeof(CameraPacked) == 416ULL);

// Right handed coordinates with +X right, +Y up, and +Z forward
struct AtmospherePacked
{
    auto operator==(AtmospherePacked const&) const -> bool = default;

    glm::vec3 scatteringRayleighPerMm;
    float densityScaleRayleighMm;
    glm::vec3 absorptionRayleighPerMm;

    float planetRadiusMm;

    glm::vec3 scatteringMiePerMm;
    float densityScaleMieMm;
    glm::vec3 absorptionMiePerMm;

    float atmosphereRadiusMm;

    glm::vec3 groundAlbedo;

    // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
    uint8_t padding0[4]{};

    glm::vec3 scatteringOzonePerMm;

    // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
    uint8_t padding1[4]{};

    glm::vec3 absorptionOzonePerMm;

    // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
    uint8_t padding2[4]{};

    // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
    uint8_t padding3[16]{};
};
// NOLINTNEXTLINE(readability-magic-numbers)
static_assert(sizeof(AtmospherePacked) == 128ULL);

struct DirectionalLightPacked
{
    glm::vec4 color;

    glm::vec4 forward;

    glm::mat4x4 projection;

    glm::mat4x4 view;

    float strength;

    float angularRadius;

    // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
    uint8_t padding0[8]{};
};
// NOLINTNEXTLINE(readability-magic-numbers)
static_assert(sizeof(DirectionalLightPacked) == 176ULL);

struct SpotLightPacked
{
    glm::vec4 color;

    glm::vec4 forward;

    glm::mat4x4 projection;

    glm::mat4x4 view;

    glm::vec4 position;

    float strength;
    // The factor by which light falls off per unit distance,
    // usually derived from the tangent of half the fov
    float falloffFactor;
    // The distance that light starts to fall off
    float falloffDistance;

    // NOLINTNEXTLINE(modernize-avoid-c-arrays, readability-magic-numbers)
    uint8_t padding0[4]{};
};
// NOLINTNEXTLINE(readability-magic-numbers)
static_assert(sizeof(SpotLightPacked) == 192ULL);

struct VertexPacked
{
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};
// NOLINTNEXTLINE(readability-magic-numbers)
static_assert(sizeof(VertexPacked) == 48ULL);
} // namespace syzygy
