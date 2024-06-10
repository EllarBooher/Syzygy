#pragma once

#include "gputypes.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "geometrystatics.hpp"
#include "geometryhelpers.hpp"

struct AtmosphereParameters 
{
    struct AnimationParameters
    {
        bool animateSun{ false };
        float animationSpeed{ 0.2f };
        bool skipNight{ false };
    };

    AnimationParameters animation{};

    glm::vec3 sunEulerAngles{ 0.0, 0.0, 0.0 };
    
    float earthRadiusMeters{ 0.0 };
    float atmosphereRadiusMeters{ 0.0 };

    glm::vec3 groundColor{ 1.0 };

    glm::vec3 scatteringCoefficientRayleigh{ 1.0 };
    float altitudeDecayRayleigh{ 1.0 };

    glm::vec3 scatteringCoefficientMie{ 1.0 };
    float altitudeDecayMie{ 1.0 };

    glm::vec3 directionToSun() const
    {
        return -geometry::forwardFromEulers(sunEulerAngles);
    }

    // Returns an estimate of the color of sunlight that has reached the 
    // origin.
    glm::vec4 computeSunlight() const
    {
        float const surfaceCosine{ 
            glm::dot(directionToSun()
            , glm::vec3{0.0,-1.0,0.0})
        };
        if (surfaceCosine <= 0.0)
        {
            return glm::vec4(0.0, 0.0, 0.0, 1.0);
        }

        glm::vec3 const start{ 0.0, -earthRadiusMeters, 0.0 };
        float outDistance{ 0.0 };
        if(!glm::intersectRaySphere(
            start
            , directionToSun()
            , glm::vec3(0.0)
            , atmosphereRadiusMeters * atmosphereRadiusMeters
            , outDistance
        ))
        {
            glm::vec4 constexpr RAW_SUNLIGHT_COLOR{ 1.0, 1.0, 1.0, 1.0 };
            return RAW_SUNLIGHT_COLOR;
        };

        float const atmosphereThickness{ outDistance };

        // Calculations derived from sky.comp, we do a single ray straight up 
        // to get an idea of the ambient color
        float const opticalDepthRayleigh{ 
            altitudeDecayRayleigh 
            / surfaceCosine 
            * (1.0f - std::exp(-atmosphereThickness / altitudeDecayRayleigh)) 
        };
        float const opticalDepthMie{ 
            altitudeDecayMie 
            / surfaceCosine 
            * (1.0f - std::exp(-atmosphereThickness / altitudeDecayMie)) 
        };

        glm::vec3 const tau{
            scatteringCoefficientRayleigh * opticalDepthRayleigh
            + 1.1f * scatteringCoefficientMie * opticalDepthMie
        };
        glm::vec3 const attenuation{
            glm::exp(-tau)
        };

        return glm::vec4(attenuation, 1.0);
    };

    gputypes::Atmosphere toDeviceEquivalent() const
    {
        // TODO: move these computations out to somewhere more sensible

        glm::vec4 const sunlight{ computeSunlight() };
        glm::vec3 const sunDirection{ glm::normalize(directionToSun()) };

        return gputypes::Atmosphere{
            .directionToSun = sunDirection,
            .earthRadiusMeters = earthRadiusMeters,
            .scatteringCoefficientRayleigh = scatteringCoefficientRayleigh,
            .altitudeDecayRayleigh = altitudeDecayRayleigh,
            .scatteringCoefficientMie = scatteringCoefficientMie,
            .altitudeDecayMie = altitudeDecayMie,
            .ambientColor = 
                glm::vec3(sunlight) 
                * groundColor 
                * glm::dot(sunDirection, glm::vec3(0.0,-1.0,0.0)),
            .atmosphereRadiusMeters = atmosphereRadiusMeters,
            .sunlightColor = glm::vec3(sunlight),
            .groundColor = groundColor,
        };
    }
};

struct CameraParameters {
    glm::vec3 cameraPosition{ 0.0f, 0.0f, 0.0f };
    glm::vec3 eulerAngles{ 0.0f, 0.0f, 0.0f };
    float fov{ 90.0f };
    float near{ 0.0f };
    float far{ 1.0f };

    gputypes::Camera toDeviceEquivalent(float aspectRatio) const
    {
        glm::mat4x4 const projViewInverse{
            glm::inverse(projection(aspectRatio) * view())
        };

        return gputypes::Camera{
            .projection = projection(aspectRatio),
            .inverseProjection = glm::inverse(projection(aspectRatio)),
            .view = view(),
            .viewInverseTranspose = glm::inverseTranspose(view()),
            .rotation = rotation(),
            .projViewInverse = projViewInverse,
            .forwardWorld = rotation() * glm::vec4(geometry::forward,0.0),
            .position = glm::vec4{ cameraPosition, 1.0 },
        };
    }

    // Make a projection camera that tightly contains 
    gputypes::Camera makeShadowpassCamera(
        float const aspectRatio
        , glm::vec3 forward
        , glm::vec3 const geometryCenter
        , glm::vec3 geometryExtent
    )
    {
        forward = glm::normalize(forward);
        geometryExtent = glm::abs(geometryExtent);

        glm::vec3 const cameraPosition{ 
            geometryCenter 
            + (-1.0f * glm::length(geometryExtent) * forward) 
        };

        glm::mat4x4 const cameraView{
            geometry::lookAtVkSafe(
                cameraPosition
                , geometryCenter
            )
        };

        glm::mat4x4 const projection{
            geometry::projectionOrthoAABBVk(
                cameraView
                , geometryCenter
                , geometryExtent
            )
        };
        glm::mat4x4 const projViewInverse{ 
            glm::inverse(projection * view()) 
        };

        return gputypes::Camera{
            .projection = projection,
            .inverseProjection = glm::inverse(projection),
            .view = cameraView,
            .viewInverseTranspose = glm::inverseTranspose(cameraView),
            .rotation = glm::mat4x4(glm::mat3x3(glm::inverse(cameraView))),
            .projViewInverse = projViewInverse,
            .forwardWorld = glm::vec4(forward, 0.0),
            .position = glm::vec4(cameraPosition,1.0),
        };
    }

    gputypes::Camera toDeviceEquivalentOrthographic(
        float const aspectRatio
        , float const planeDistance
    ) const
    {
        glm::mat4x4 const projection{ 
            projectionOrthographic(aspectRatio, planeDistance) 
        };
        glm::mat4x4 const projViewInverse{ 
            glm::inverse(projection * view()) 
        };

        return gputypes::Camera{
            .projection = projection,
            .inverseProjection = glm::inverse(projection),
            .view = view(),
            .viewInverseTranspose = glm::inverseTranspose(view()),
            .rotation = rotation(),
            .projViewInverse = projViewInverse,
            .forwardWorld = rotation() * glm::vec4(geometry::forward, 0.0),
            .position = glm::vec4(cameraPosition, 1.0),
        };
    }

    // The matrix that transforms from camera to world space
    glm::mat4 transform() const
    {
        return geometry::transformVk(cameraPosition, eulerAngles);
    }

    // The inverse of transform
    glm::mat4 view() const
    {
        return geometry::viewVk(cameraPosition, eulerAngles);
    }

    // Rotates, but does not translate from camera to world space
    glm::mat4 rotation() const
    {
        return glm::orientate4(eulerAngles);
    }

    // Projects from camera space to clip space
    glm::mat4 projection(float const aspectRatio) const
    {
        return geometry::projectionVk(
            geometry::PerspectiveProjectionParameters{
                .fov_y = fov,
                .aspectRatio = aspectRatio,
                .near = near,
                .far = far,
            }
        );
    }

    // Projects from camera space to clip space
    glm::mat4 projectionOrthographic(
        float const aspectRatio
        , float const distance
    ) const
    {
        // An orthographic projection has one view plane, 
        // so we compute it from the fov and distance.

        float const height{
            glm::tan(fov / 2.0f) * distance
        };
        
        glm::vec3 const min{ -aspectRatio * height, -height, near };
        glm::vec3 const max{ aspectRatio * height, height, far };

        return geometry::projectionOrthoVk(min, max);
    }

    // Generates the projection * view matrix that transforms from world to 
    // clip space. Aspect ratio is a function of the drawn surface, so it is 
    // passed in at generation time.
    glm::mat4 toProjView(float const aspectRatio) const
    {
        return projection(aspectRatio) * view();
    }
};