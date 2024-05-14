#pragma once

#include "gputypes.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/intersect.hpp>

#include "geometrystatics.hpp"
#include "geometryhelpers.hpp"

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
        return -geometry::forwardFromEulers(sunEulerAngles);
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
        glm::mat4x4 const projViewInverse{ glm::inverse(projection(aspectRatio) * view()) };

        return GPUTypes::Camera{
            .projection{ projection(aspectRatio) },
            .inverseProjection{ glm::inverse(projection(aspectRatio)) },
            .view{ view() },
            .viewInverseTranspose{ glm::inverseTranspose(view()) },
            .rotation{ rotation() },
            .projViewInverse{ projViewInverse },
            .forwardWorld{ rotation() * glm::vec4(geometry::forward,0.0) },
            .position{ glm::vec4{ cameraPosition, 1.0 } },
        };
    }

    // Make a projection camera that tightly contains 
    GPUTypes::Camera makeShadowpassCamera(
        float const aspectRatio
        , glm::vec3 forward
        , glm::vec3 const geometryCenter
        , glm::vec3 geometryExtent
    )
    {
        forward = glm::normalize(forward);
        geometryExtent = glm::abs(geometryExtent);

        glm::vec3 const cameraPosition{ geometryCenter + (-1.0f * glm::length(geometryExtent) * forward) };

        glm::mat4x4 const cameraView{
            geometry::lookAtVkSafe(
                cameraPosition
                , geometryCenter
            )
        };

        glm::mat4x4 const projection{
            geometry::projectionOrthoAABBVk(cameraView, geometryCenter, geometryExtent)
        };
        glm::mat4x4 const projViewInverse{ glm::inverse(projection * view()) };

        return GPUTypes::Camera{
            .projection{ projection },
            .inverseProjection{ glm::inverse(projection) },
            .view{ cameraView },
            .viewInverseTranspose{ glm::inverseTranspose(cameraView) },
            .rotation{ glm::mat4x4(glm::mat3x3(glm::inverse(cameraView))) },
            .projViewInverse{ projViewInverse },
            .forwardWorld{ glm::vec4(forward, 0.0) },
            .position{ glm::vec4(cameraPosition,1.0) }
        };
    }

    GPUTypes::Camera toDeviceEquivalentOrthographic(
        float aspectRatio
        , float planeDistance
    ) const
    {
        glm::mat4x4 const projection{ projectionOrthographic(aspectRatio, planeDistance) };
        glm::mat4x4 const projViewInverse{ glm::inverse(projection * view()) };

        return GPUTypes::Camera{
            .projection{ projection },
            .inverseProjection{ glm::inverse(projection) },
            .view{ view() },
            .viewInverseTranspose{ glm::inverseTranspose(view()) },
            .rotation{ rotation() },
            .projViewInverse{ projViewInverse },
            .forwardWorld{ rotation() * glm::vec4(geometry::forward, 0.0) },
            .position{ glm::vec4(cameraPosition, 1.0) },
        };
    }

    /** Returns the matrix that transforms from camera-local space to world space */
    glm::mat4 transform() const
    {
        return geometry::transformVk(cameraPosition, eulerAngles);
    }

    glm::mat4 view() const
    {
        return geometry::viewVk(cameraPosition, eulerAngles);
    }

    glm::mat4 rotation() const
    {
        return glm::orientate4(eulerAngles);
    }

    glm::mat4 projection(float aspectRatio) const
    {
        return geometry::projectionVk(fov, aspectRatio, near, far);
    }

    glm::mat4 projectionOrthographic(float aspectRatio, float distance) const
    {
        // An orthographic projection has one view plane, so we compute it from the fov and distance.

        float const height = glm::tan(fov / 2.0) * distance;
        
        glm::vec3 const min{ -aspectRatio * height, -height, near };
        glm::vec3 const max{ aspectRatio * height, height, far };

        return geometry::projectionOrthoVk(min, max);
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
};

struct ShadowPassParameters
{
    float depthBiasConstant{ 2.00f };
    float depthBiasSlope{ -1.75f };
};