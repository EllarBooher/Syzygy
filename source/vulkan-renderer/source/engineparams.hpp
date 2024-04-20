#pragma once

#include "gputypes.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

struct AtmosphereParameters {
    glm::vec3 directionToSun{ 1.0, 0.0, 0.0 };

    float earthRadiusMeters{ 0.0 };
    float atmosphereRadiusMeters{ 0.0 };

    glm::vec3 scatteringCoefficientRayleigh{ 1.0 };
    float altitudeDecayRayleigh{ 1.0 };

    glm::vec3 scatteringCoefficientMie{ 1.0 };
    float altitudeDecayMie{ 1.0 };

    GPUTypes::Atmosphere toDeviceEquivalent() const
    {
        return GPUTypes::Atmosphere{
            .directionToSun{ glm::normalize(directionToSun) },
            .earthRadiusMeters{ earthRadiusMeters },
            .scatteringCoefficientRayleigh{ scatteringCoefficientRayleigh },
            .altitudeDecayRayleigh{ altitudeDecayRayleigh },
            .scatteringCoefficientMie{ scatteringCoefficientMie },
            .altitudeDecayMie{ altitudeDecayMie },
            .atmosphereRadiusMeters{ atmosphereRadiusMeters },
        };
    }
};

struct CameraParameters {
    glm::vec3 cameraPosition{ 0.0f, 0.0f, 0.0f };
    glm::vec3 eulerAngles{ 0.0f, 0.0f, 0.0f };
    float fov{ 90.0f };
    float near{ 0.0f };
    float far{ 1.0f };

    GPUTypes::Camera toDeviceEquivalent(float aspectRatio) const
    {
        return GPUTypes::Camera{
            .projection{ projection(aspectRatio) },
            .inverseProjection{ glm::inverse(projection(aspectRatio)) },
            .view{ view() },
            .rotation{ rotation() },
            .position{ cameraPosition },
            .padding0{}
        };
    }

    /** Returns the matrix that transforms from camera-local space to world space */
    glm::mat4 transform() const
    {
        return glm::translate(cameraPosition) * glm::orientate4(eulerAngles);
    }

    glm::mat4 view() const
    {
        return glm::inverse(transform());
    }

    glm::mat4 rotation() const
    {
        return glm::orientate4(eulerAngles);
    }

    glm::mat4 projection(float aspectRatio) const
    {
        // We use LH perspective matrix since we swap the near and far plane for better 
        // distribution of floating point precision.
        return glm::perspectiveLH_ZO(
            glm::radians(fov)
            , aspectRatio
            , far
            , near
        );
    }

    /**
        Generates the projection * view matrix that transforms from world to clip space.
        Aspect ratio is a function of the drawn surface, so it is passed in at generation time.
        Produces a right handed matrix- x is right, y is down, and z is forward.
    */
    glm::mat4 toProjView(float aspectRatio) const
    {
        return projection(aspectRatio) * view();
    }

    /**
        Returns a vector that represents the position of the (+,+) corner of the near plane in local space.
    */
    glm::vec3 nearPlaneExtent(float aspectRatio) const
    {
        float const tanHalfFOV{ glm::tan(glm::radians(fov)) };

        return near * glm::vec3{ aspectRatio * tanHalfFOV, tanHalfFOV, 1.0 };
    }
};