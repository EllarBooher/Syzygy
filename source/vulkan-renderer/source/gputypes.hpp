#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

/*
* This namespace contains types that are used in shaders on the GPU.
* They contain padding and must faithfully represent their counterparts.
* These are intended for byte-wise copying to the GPU, not for manipulation by UI or the engine.
*/
namespace GPUTypes
{
    /*
    * For ease of reading, group members by 16 bytes, which is a size of a single-precision vec4.
    * The gpu equivalents will likely use std430 packing.
    */

    struct Camera
    {
        glm::mat4x4 projection;

        glm::mat4x4 inverseProjection;

        glm::mat4x4 view;

        glm::mat4x4 viewInverseTranspose;

        glm::mat4x4 rotation;

        glm::vec3 position;
        uint8_t padding0[4];
    };

    struct Atmosphere
    {
        glm::vec3 directionToSun;
        float earthRadiusMeters;

        glm::vec3 scatteringCoefficientRayleigh;
        float altitudeDecayRayleigh;

        glm::vec3 scatteringCoefficientMie;
        float altitudeDecayMie;

        uint8_t padding0[4 * 3]; //vec3
        float atmosphereRadiusMeters;
    };
}
