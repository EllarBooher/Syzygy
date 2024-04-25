#pragma once

#include "gputypes.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/intersect.hpp>

struct AtmosphereParameters {
    struct AnimationParameters
    {
        bool animateSun{ false };
        float animationSpeed{ 1.0f };
        bool skipNight{ false };
    };

    AnimationParameters animation{};

    glm::vec3 sunEulerAngles{ 0.0, 0.0, 0.0 };

    glm::vec3 directionToSun() const
    {
        return glm::vec3(
            glm::orientate3(sunEulerAngles) * glm::vec3(0.0, -1.0, 0.0)
        );
    }

    float earthRadiusMeters{ 0.0 };
    float atmosphereRadiusMeters{ 0.0 };

    glm::vec3 groundColor{ 1.0 };

    glm::vec3 scatteringCoefficientRayleigh{ 1.0 };
    float altitudeDecayRayleigh{ 1.0 };

    glm::vec3 scatteringCoefficientMie{ 1.0 };
    float altitudeDecayMie{ 1.0 };

    // Returns an estimate of the color of sunlight that has reached the origin.
    glm::vec4 computeSunlight() const
    {
        float const surfaceCosine{ glm::dot(directionToSun(), glm::vec3{0.0,-1.0,0.0})};
        if (surfaceCosine <= 0.0)
        {
            return glm::vec4(0.0, 0.0, 0.0, 1.0);
        }

        glm::vec3 const start{ 0.0, -earthRadiusMeters, 0.0 };
        float outDistance{ 0.0 };
        glm::intersectRaySphere(
            start
            , directionToSun()
            , glm::vec3(0.0)
            , atmosphereRadiusMeters * atmosphereRadiusMeters
            , outDistance
        );

        float const atmosphereThickness{ outDistance };
        // Calculations derived from sky.comp, we do a single ray straight up to get an idea of the ambient color
        float const opticalDepthRayleigh{ altitudeDecayRayleigh / surfaceCosine * (1.0f - std::exp(-atmosphereThickness / altitudeDecayRayleigh)) };
        float const opticalDepthMie{ altitudeDecayMie / surfaceCosine * (1.0f - std::exp(-atmosphereThickness / altitudeDecayMie)) };

        glm::vec3 const tau{
            scatteringCoefficientRayleigh * opticalDepthRayleigh
            + 1.1f * scatteringCoefficientMie * opticalDepthMie
        };
        glm::vec3 const attenuation{
            glm::exp(-tau)
        };

        return glm::vec4(attenuation, 1.0);
    };

    GPUTypes::Atmosphere toDeviceEquivalent() const
    {
        // TODO: move these computations out to somewhere more sensible

        glm::vec4 const sunlight{ computeSunlight() };
        glm::vec3 const sunDirection{ glm::normalize(directionToSun()) };

        return GPUTypes::Atmosphere{
            .directionToSun{ sunDirection },
            .earthRadiusMeters{ earthRadiusMeters },
            .scatteringCoefficientRayleigh{ scatteringCoefficientRayleigh },
            .altitudeDecayRayleigh{ altitudeDecayRayleigh },
            .scatteringCoefficientMie{ scatteringCoefficientMie },
            .altitudeDecayMie{ altitudeDecayMie },
            .ambientColor{ glm::vec3(sunlight) * groundColor * glm::dot(sunDirection, glm::vec3(0.0,-1.0,0.0))},
            .atmosphereRadiusMeters{ atmosphereRadiusMeters },
            .sunlightColor{ glm::vec3(sunlight) },
            .groundColor{ groundColor },
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
            .viewInverseTranspose{ glm::inverseTranspose(view()) },
            .rotation{ rotation() },
            .position{ cameraPosition }
        };
    }

    GPUTypes::Camera toDeviceEquivalentOrthographic(
        float aspectRatio
        , float planeDistance
    ) const
    {
        glm::mat4x4 const projection{ projectionOrthographic(aspectRatio, planeDistance) };

        return GPUTypes::Camera{
            .projection{ projection },
            .inverseProjection{ glm::inverse(projection) },
            .view{ view() },
            .viewInverseTranspose{ glm::inverseTranspose(view()) },
            .rotation{ rotation() },
            .position{ cameraPosition }
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

    glm::mat4 projectionOrthographic(float aspectRatio, float distance) const
    {
        // An orthographic projection has one view plane, so we compute it from the fov and distance.

        float const height = glm::tan(fov / 2.0) * distance;
        
        return glm::orthoLH_ZO(
            -aspectRatio * height
            , aspectRatio * height
            , -height
            , height
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