#include "scene.hpp"

#include "syzygy/assets.hpp"
#include "syzygy/core/input.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/core/timing.hpp"
#include "syzygy/geometryhelpers.hpp"
#include "syzygy/geometrystatics.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/lights.hpp"
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>
#include <span>
#include <utility>

/*
 * Values derived from:
 * https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html
 * Which is based on the original paper "Display of the Earth Taking into
 * Account Atmospheric Scattering" by Tomoyuki Nishita, Takao Sirai, Katsumi
 * Tadamura, Eihachiro Nakamae
 */
scene::Atmosphere const scene::Scene::DEFAULT_ATMOSPHERE_EARTH{
    scene::Atmosphere{
        .sunEulerAngles = glm::vec3(1.0, 0.0, 0.0),

        .earthRadiusMeters = 6378000,
        .atmosphereRadiusMeters = 6420000,

        .groundColor = glm::vec3{0.9, 0.8, 0.6},

        .scatteringCoefficientRayleigh =
            glm::vec3(0.0000038, 0.0000135, 0.0000331),
        .altitudeDecayRayleigh = 7994.0,

        .scatteringCoefficientMie = glm::vec3(0.000021),
        .altitudeDecayMie = 1200.0,
    }
};

scene::Camera const scene::Scene::DEFAULT_CAMERA{scene::Camera{
    .cameraPosition = glm::vec3(0.0F, -8.0F, -8.0F),
    .eulerAngles = glm::vec3(-0.3F, 0.0F, 0.0F),
    .fovDegrees = 70.0F,
    .near = 0.1F,
    .far = 10000.0F,
}};

float const scene::Scene::DEFAULT_CAMERA_CONTROLLED_SPEED{20.0F};

namespace
{
auto tickSunEulerAngles(
    scene::Atmosphere const& atmosphere, TickTiming const timing
) -> glm::vec3
{
    scene::Atmosphere::SunAnimation const animation{atmosphere.animation};

    if (!animation.animateSun)
    {
        return atmosphere.sunEulerAngles;
    }

    // position of sun as proxy for time
    float const time{glm::dot(geometry::up, atmosphere.directionToSun())};

    float constexpr TIME_NIGHT_COSINE{-0.11F};
    float const SUNRISE_ANGLE_RADIANS{glm::asin(0.1F)};

    bool const isNight{time < TIME_NIGHT_COSINE};

    glm::vec3 finalAngles{atmosphere.sunEulerAngles};
    if (isNight && animation.skipNight)
    {
        // Skip to the right horizon along the sun's great circle path
        if (animation.animationSpeed > 0.0)
        {
            finalAngles.x = glm::pi<float>() - SUNRISE_ANGLE_RADIANS;
        }
        else
        {
            finalAngles.x = SUNRISE_ANGLE_RADIANS;
        }
    }
    else
    {
        finalAngles.x += static_cast<float>(timing.deltaTimeSeconds)
                       * animation.animationSpeed;
    }

    glm::vec3 constexpr MAX_ANGLES_RADIANS{glm::vec3(glm::two_pi<float>())};
    return glm::mod(finalAngles, MAX_ANGLES_RADIANS);
}
} // namespace

auto scene::Scene::defaultScene(
    VkDevice const device,
    VmaAllocator const allocator,
    MeshAssetLibrary const& meshes
) -> Scene
{
    std::vector<gputypes::LightSpot> const spotlights{
        lights::makeSpot(
            glm::vec4(0.0, 1.0, 0.0, 1.0),
            30.0,
            1.0,
            1.0,
            60,
            1.0,
            glm::vec3(-1.0, 0.0, 1.0),
            glm::vec3(-8.0, -10.0, -2.0),
            0.1,
            1000.0
        ),
        lights::makeSpot(
            glm::vec4(1.0, 0.0, 0.0, 1.0),
            30.0,
            1.0,
            1.0,
            60,
            1.0,
            glm::vec3(-1.0, 0.0, -1.0),
            glm::vec3(8.0, -10.0, 2.0),
            0.1,
            1000.0
        ),
    };

    std::vector<MeshInstanced> geometry{};
    geometry.reserve(2);
    std::optional<size_t> cubesIndex{};
    SceneBounds bounds{};
    if (!meshes.loadedMeshes.empty())
    {
        int32_t constexpr COORDINATE_MIN{-40};
        int32_t constexpr COORDINATE_MAX{40};

        { // Floor
            MeshInstanced floorGeometry{};
            floorGeometry.name = "meshInstanced_Floor";
            floorGeometry.mesh = meshes.loadedMeshes[0];
            floorGeometry.render = true;

            for (int32_t x{COORDINATE_MIN}; x <= COORDINATE_MAX; x++)
            {
                for (int32_t z{COORDINATE_MIN}; z <= COORDINATE_MAX; z++)
                {
                    glm::vec3 const position{
                        static_cast<float>(x) * 20.0F,
                        1.0F,
                        static_cast<float>(z) * 20.0F
                    };
                    glm::vec3 const scale{10.0F, 2.0F, 10.0F};

                    floorGeometry.originals.push_back(
                        glm::translate(position) * glm::scale(scale)
                    );
                }
            }

            VkDeviceSize const bufferSize{
                static_cast<VkDeviceSize>(floorGeometry.originals.size())
            };

            floorGeometry.models = std::make_unique<TStagedBuffer<glm::mat4x4>>(
                TStagedBuffer<glm::mat4x4>::allocate(
                    device,
                    allocator,
                    bufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                )
            );
            floorGeometry.modelInverseTransposes =
                std::make_unique<TStagedBuffer<glm::mat4x4>>(
                    TStagedBuffer<glm::mat4x4>::allocate(
                        device,
                        allocator,
                        bufferSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                    )
                );

            std::vector<glm::mat4x4> modelInverseTransposes{};
            modelInverseTransposes.reserve(floorGeometry.originals.size());
            for (glm::mat4x4 const& model : floorGeometry.originals)
            {
                modelInverseTransposes.push_back(glm::inverseTranspose(model));
            }

            floorGeometry.models->stage(floorGeometry.originals);
            floorGeometry.modelInverseTransposes->stage(modelInverseTransposes);

            geometry.push_back(std::move(floorGeometry));
        }

        { // Cubes
            MeshInstanced cubeGeometry{};
            cubeGeometry.name = "meshInstanced_Cubes";
            cubeGeometry.mesh = meshes.loadedMeshes[0];
            cubeGeometry.render = true;

            for (int32_t x{COORDINATE_MIN}; x <= COORDINATE_MAX; x++)
            {
                for (int32_t z{COORDINATE_MIN}; z <= COORDINATE_MAX; z++)
                {
                    glm::vec3 const position{
                        static_cast<float>(x), -4.0, static_cast<float>(z)
                    };
                    glm::quat const orientation{geometry::randomQuat()};
                    glm::vec3 const scale{0.2F};

                    cubeGeometry.originals.push_back(
                        glm::translate(position) * glm::toMat4(orientation)
                        * glm::scale(scale)
                    );
                }
            }

            VkDeviceSize const bufferSize{
                static_cast<VkDeviceSize>(cubeGeometry.originals.size())
            };

            cubeGeometry.models = std::make_unique<TStagedBuffer<glm::mat4x4>>(
                TStagedBuffer<glm::mat4x4>::allocate(
                    device,
                    allocator,
                    bufferSize,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                )
            );
            cubeGeometry.modelInverseTransposes =
                std::make_unique<TStagedBuffer<glm::mat4x4>>(
                    TStagedBuffer<glm::mat4x4>::allocate(
                        device,
                        allocator,
                        bufferSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                    )
                );

            std::vector<glm::mat4x4> modelInverseTransposes{};
            modelInverseTransposes.reserve(cubeGeometry.originals.size());
            for (glm::mat4x4 const& model : cubeGeometry.originals)
            {
                modelInverseTransposes.push_back(glm::inverseTranspose(model));
            }

            cubeGeometry.models->stage(cubeGeometry.originals);
            cubeGeometry.modelInverseTransposes->stage(modelInverseTransposes);

            cubesIndex = geometry.size();
            geometry.push_back(std::move(cubeGeometry));
        }

        SceneBounds constexpr DEFAULT_SCENE_BOUNDS{
            .center = glm::vec3{0.0, -4.0, 0.0},
            .extent = glm::vec3{45.0, 5.0, 45.0},
        };
        bounds = DEFAULT_SCENE_BOUNDS;
    }

    return Scene{
        .spotlightsRender = true,
        .spotlights = spotlights,
        .cubesIndex = cubesIndex,
        .geometry = std::move(geometry),
        .bounds = bounds,
    };
}

void scene::Scene::handleInput(
    TickTiming const lastFrame, szg_input::InputSnapshot const& input
)
{
    glm::vec3 accumulatedMovement{};
    if (input.getStatus(szg_input::KeyCode::W).down)
    {
        accumulatedMovement += geometry::forward;
    }
    if (input.getStatus(szg_input::KeyCode::S).down)
    {
        accumulatedMovement -= geometry::forward;
    }
    if (input.getStatus(szg_input::KeyCode::D).down)
    {
        accumulatedMovement += geometry::right;
    }
    if (input.getStatus(szg_input::KeyCode::A).down)
    {
        accumulatedMovement -= geometry::right;
    }
    if (input.getStatus(szg_input::KeyCode::E).down)
    {
        accumulatedMovement += geometry::up;
    }
    if (input.getStatus(szg_input::KeyCode::Q).down)
    {
        accumulatedMovement -= geometry::up;
    }

    camera.cameraPosition += cameraControlledSpeed
                           * static_cast<float>(lastFrame.deltaTimeSeconds)
                           * accumulatedMovement;
}

void scene::Scene::tick(TickTiming const lastFrame)
{
    atmosphere.sunEulerAngles = tickSunEulerAngles(atmosphere, lastFrame);

    if (!cubesIndex.has_value())
    {
        return;
    }

    MeshInstanced& cubes{geometry[cubesIndex.value()]};

    if (cubes.models == nullptr || cubes.modelInverseTransposes == nullptr)
    {
        return;
    }

    std::span<glm::mat4x4> const models{cubes.models->mapValidStaged()};
    std::span<glm::mat4x4> const modelInverseTransposes{
        cubes.modelInverseTransposes->mapValidStaged()
    };

    if (models.size() != modelInverseTransposes.size())
    {
        Warning("models and modelInverseTransposes out of sync");
        return;
    }

    for (size_t index{0}; index < cubes.originals.size(); index++)
    {
        glm::mat4x4 const modelOriginal{cubes.originals[index]};

        glm::vec4 const position{modelOriginal * glm::vec4(0.0, 0.0, 0.0, 1.0)};

        double const timeOffset{
            (position.x - (-10) + position.z - (-10)) / 3.1415
        };

        double const y{glm::sin(lastFrame.timeElapsedSeconds + timeOffset)};

        glm::mat4x4 const translation{glm::translate(glm::vec3(0.0, y, 0.0))};

        models[index] = translation * modelOriginal;

        // In general, the model inverse transposes only need to be
        // updated once per tick, before rendering and after the last
        // update of the model matrices. For now, we only update once
        // per tick, so we just compute it here.
        modelInverseTransposes[index] = glm::inverseTranspose(models[index]);
    }
}

namespace
{
auto createSunlight(
    scene::SceneBounds const sceneBounds,
    glm::vec3 const sunEulerAngles,
    glm::vec3 const sunlightRGB
) -> gputypes::LightDirectional
{
    float constexpr SUNLIGHT_STRENGTH{0.5F};

    return lights::makeDirectional(
        glm::vec4(sunlightRGB, 1.0),
        SUNLIGHT_STRENGTH,
        sunEulerAngles,
        sceneBounds.center,
        sceneBounds.extent
    );
}
auto createMoonlight(
    scene::SceneBounds const sceneBounds,
    float const sunCosine,
    float const sunsetCosine
) -> gputypes::LightDirectional
{
    float constexpr MOONRISE_LENGTH{0.08};

    float const moonlightStrength{
        0.1F
        * glm::clamp(
            0.0F, 1.0F, glm::abs(sunCosine - sunsetCosine) / MOONRISE_LENGTH
        )
    };

    glm::vec4 constexpr MOONLIGHT_COLOR_RGBA{0.3, 0.4, 0.6, 1.0};
    glm::vec3 constexpr STRAIGHT_DOWN_EULER_ANGLES{
        -glm::half_pi<float>(), 0.0F, 0.0F
    };

    return lights::makeDirectional(
        MOONLIGHT_COLOR_RGBA,
        moonlightStrength,
        STRAIGHT_DOWN_EULER_ANGLES,
        sceneBounds.center,
        sceneBounds.extent
    );
}

// Returns an estimate of the color of sunlight that has reached the
// origin, attenuated due to scattering.
auto computeSunlightColor(scene::Atmosphere const& atmosphere) -> glm::vec4
{
    float const surfaceCosine{
        glm::dot(atmosphere.directionToSun(), glm::vec3{0.0, -1.0, 0.0})
    };
    if (surfaceCosine <= 0.0)
    {
        return {0.0, 0.0, 0.0, 1.0};
    }

    glm::vec3 const start{0.0, -atmosphere.earthRadiusMeters, 0.0};
    float outDistance{0.0};
    if (!glm::intersectRaySphere(
            start,
            atmosphere.directionToSun(),
            glm::vec3(0.0),
            atmosphere.atmosphereRadiusMeters
                * atmosphere.atmosphereRadiusMeters,
            outDistance
        ))
    {
        glm::vec4 constexpr RAW_SUNLIGHT_COLOR{1.0, 1.0, 1.0, 1.0};
        return RAW_SUNLIGHT_COLOR;
    };

    float const atmosphereThickness{outDistance};

    // Calculations derived from sky.comp, we do a single ray straight up
    // to get an idea of the ambient color
    float const opticalDepthRayleigh{
        atmosphere.altitudeDecayRayleigh / surfaceCosine
        * (1.0F
           - glm::exp(-atmosphereThickness / atmosphere.altitudeDecayRayleigh))
    };
    float const opticalDepthMie{
        atmosphere.altitudeDecayMie / surfaceCosine
        * (1.0F - glm::exp(-atmosphereThickness / atmosphere.altitudeDecayMie))
    };

    glm::vec3 const tau{
        atmosphere.scatteringCoefficientRayleigh * opticalDepthRayleigh
        + 1.1F * atmosphere.scatteringCoefficientMie * opticalDepthMie
    };
    glm::vec3 const attenuation{glm::exp(-tau)};

    return {attenuation, 1.0};
}
} // namespace

auto scene::Atmosphere::directionToSun() const -> glm::vec3
{
    return -geometry::forwardFromEulers(sunEulerAngles);
}

auto scene::Atmosphere::toDeviceEquivalent() const -> gputypes::Atmosphere
{
    // TODO: move these computations out to somewhere more sensible

    glm::vec4 const sunlight{computeSunlightColor(*this)};
    glm::vec3 const sunDirection{glm::normalize(directionToSun())};

    return gputypes::Atmosphere{
        .directionToSun = sunDirection,
        .earthRadiusMeters = earthRadiusMeters,
        .scatteringCoefficientRayleigh = scatteringCoefficientRayleigh,
        .altitudeDecayRayleigh = altitudeDecayRayleigh,
        .scatteringCoefficientMie = scatteringCoefficientMie,
        .altitudeDecayMie = altitudeDecayMie,
        .ambientColor = glm::vec3(sunlight) * groundColor
                      * glm::dot(sunDirection, glm::vec3(0.0, -1.0, 0.0)),
        .atmosphereRadiusMeters = atmosphereRadiusMeters,
        .sunlightColor = glm::vec3(sunlight),
        .groundColor = groundColor,
    };
}

auto scene::Atmosphere::baked(SceneBounds const sceneBounds) const
    -> AtmosphereBaked
{
    gputypes::Atmosphere const atmosphere{toDeviceEquivalent()};

    // position of sun as proxy for time
    float const sunCosine{glm::dot(geometry::up, atmosphere.directionToSun)};
    float constexpr SUNSET_COSINE{0.06};

    std::optional<gputypes::LightDirectional> sunlight{};
    std::optional<gputypes::LightDirectional> moonlight{};

    if (sunCosine > 0.0F)
    {
        sunlight = createSunlight(
            sceneBounds, sunEulerAngles, atmosphere.sunlightColor
        );
    }
    if (sunCosine < SUNSET_COSINE)
    {
        moonlight = createMoonlight(sceneBounds, sunCosine, SUNSET_COSINE);
    }

    return AtmosphereBaked{
        .atmosphere = atmosphere,
        .sunlight = sunlight,
        .moonlight = moonlight,
    };
}

auto scene::Camera::toDeviceEquivalent(float const aspectRatio) const
    -> gputypes::Camera
{
    glm::mat4x4 const proj{projection(aspectRatio)};
    glm::mat4x4 const projViewInverse{glm::inverse(toProjView(aspectRatio))};

    return gputypes::Camera{
        .projection = proj,
        .inverseProjection = glm::inverse(proj),
        .view = view(),
        .viewInverseTranspose = glm::inverseTranspose(view()),
        .rotation = rotation(),
        .projViewInverse = projViewInverse,
        .forwardWorld = rotation() * glm::vec4(geometry::forward, 0.0),
        .position = glm::vec4(cameraPosition, 1.0),
    };
}

auto scene::Camera::toProjView(float const aspectRatio) const -> glm::mat4x4
{
    return projection(aspectRatio) * view();
}

auto scene::Camera::rotation() const -> glm::mat4x4
{
    return glm::orientate4(eulerAngles);
}

auto scene::Camera::transform() const -> glm::mat4x4
{
    return geometry::transformVk(cameraPosition, eulerAngles);
}

auto scene::Camera::view() const -> glm::mat4x4
{
    return geometry::viewVk(cameraPosition, eulerAngles);
}

auto scene::Camera::projection(float const aspectRatio) const -> glm::mat4x4
{
    if (orthographic)
    {
        float const height{glm::tan(glm::radians(fovDegrees) / 2.0F)};

        glm::vec3 const min{-aspectRatio * height, -height, near};
        glm::vec3 const max{aspectRatio * height, height, far};

        return geometry::projectionOrthoVk(min, max);
    }

    return geometry::projectionVk(geometry::PerspectiveProjectionParameters{
        .fov_y = fovDegrees,
        .aspectRatio = aspectRatio,
        .near = near,
        .far = far,
    });
}
