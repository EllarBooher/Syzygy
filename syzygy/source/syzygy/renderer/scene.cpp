#include "scene.hpp"

#include "syzygy/assets/assets.hpp"
#include "syzygy/core/input.hpp"
#include "syzygy/core/timing.hpp"
#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/geometry/geometrystatics.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/renderer/lights.hpp"
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <span>
#include <utility>

namespace syzygy
{
/*
 * Values derived from:
 * https://www.scratchapixel.com/lessons/procedural-generation-virtual-worlds/simulating-sky/simulating-colors-of-the-sky.html
 * Which is based on the original paper "Display of the Earth Taking into
 * Account Atmospheric Scattering" by Tomoyuki Nishita, Takao Sirai, Katsumi
 * Tadamura, Eihachiro Nakamae
 */
Atmosphere const Scene::DEFAULT_ATMOSPHERE_EARTH{Atmosphere{
    .sunEulerAngles = glm::vec3(1.0, 0.0, 0.0),

    .earthRadiusMeters = 6378000,
    .atmosphereRadiusMeters = 6420000,

    .groundColor = glm::vec3{0.9, 0.8, 0.6},

    .scatteringCoefficientRayleigh = glm::vec3(0.0000038, 0.0000135, 0.0000331),
    .altitudeDecayRayleigh = 7994.0,

    .scatteringCoefficientMie = glm::vec3(0.000021),
    .altitudeDecayMie = 1200.0,
}};

Camera const Scene::DEFAULT_CAMERA{Camera{
    .cameraPosition = glm::vec3(0.0F, -10.0F, -13.0F),
    .eulerAngles = glm::vec3(0.0F, 0.0F, 0.0F),
    .fovDegrees = 70.0F,
    .near = 0.1F,
    .far = 10000.0F,
}};

float const Scene::DEFAULT_CAMERA_CONTROLLED_SPEED{20.0F};

SunAnimation const Scene::DEFAULT_SUN_ANIMATION{SunAnimation{
    .frozen = false, .time = 0.27F, .speed = 100.0F, .skipNight = false
}};

float const SunAnimation::DAY_LENGTH_SECONDS{60.0F * 60.0F * 24.0F};

auto Scene::defaultScene(
    VkDevice const device,
    VmaAllocator const allocator,
    std::span<AssetRef<MeshAsset> const> const meshes
) -> Scene
{
    std::vector<SpotLightPacked> const spotlights{
        makeSpot(
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
        makeSpot(
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
    if (!meshes.empty())
    {
        int32_t constexpr COORDINATE_MIN{-40};
        int32_t constexpr COORDINATE_MAX{40};

        { // Floor
            MeshInstanced floorGeometry{};
            floorGeometry.name = "meshInstanced_Floor";
            floorGeometry.mesh = meshes[0].get().data;
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
            cubeGeometry.mesh = meshes[0].get().data;
            cubeGeometry.render = true;

            for (int32_t x{COORDINATE_MIN}; x <= COORDINATE_MAX; x++)
            {
                for (int32_t z{COORDINATE_MIN}; z <= COORDINATE_MAX; z++)
                {
                    glm::vec3 const position{
                        static_cast<float>(x), -4.0, static_cast<float>(z)
                    };
                    glm::quat const orientation{randomQuat()};
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

void Scene::handleInput(TickTiming const lastFrame, InputSnapshot const& input)
{
    glm::vec2 const cursorDelta{input.cursor.delta()};

    glm::vec2 const adjustedCursorDelta{
        cursorDelta.x / 100.0F, cursorDelta.y / 200.0F
    };

    // left to WORLD_RIGHT
    camera.eulerAngles.z += adjustedCursorDelta.x;

    // WORLD_UP and down, avoid flipping camera
    camera.eulerAngles.x = glm::clamp(
        camera.eulerAngles.x - adjustedCursorDelta.y,
        -glm::half_pi<float>(),
        glm::half_pi<float>()
    );

    glm::mat3x3 const transform{camera.transform()};

    glm::vec3 const forward{transform * WORLD_FORWARD};
    glm::vec3 const right{transform * WORLD_RIGHT};
    // We do not rotate WORLD_UP, since the controls would be disorienting
    glm::vec3 const up{WORLD_UP};

    glm::vec3 accumulatedMovement{};

    KeySnapshot const& keys{input.keys};

    if (keys.getStatus(KeyCode::W).down)
    {
        accumulatedMovement += forward;
    }
    if (keys.getStatus(KeyCode::S).down)
    {
        accumulatedMovement -= forward;
    }
    if (keys.getStatus(KeyCode::D).down)
    {
        accumulatedMovement += right;
    }
    if (keys.getStatus(KeyCode::A).down)
    {
        accumulatedMovement -= right;
    }
    if (keys.getStatus(KeyCode::E).down)
    {
        accumulatedMovement += up;
    }
    if (keys.getStatus(KeyCode::Q).down)
    {
        accumulatedMovement -= up;
    }

    camera.cameraPosition += cameraControlledSpeed
                           * static_cast<float>(lastFrame.deltaTimeSeconds)
                           * accumulatedMovement;
}

void Scene::tick(TickTiming const lastFrame)
{
    if (!sunAnimation.frozen)
    {
        sunAnimation.time = glm::fract(
            sunAnimation.time
            + sunAnimation.speed
                  * static_cast<float>(lastFrame.deltaTimeSeconds)
                  / SunAnimation::DAY_LENGTH_SECONDS
        );
    }

    if (sunAnimation.skipNight && !sunAnimation.frozen)
    {
        float constexpr SUNSET_LENGTH_TIME{0.015F};

        // The times when the sun is at the respective horizons
        float constexpr HORIZON_A_TIME{0.25F - SUNSET_LENGTH_TIME};
        float constexpr HORIZON_B_TIME{0.75F + SUNSET_LENGTH_TIME};

        bool const isNight{
            sunAnimation.time < HORIZON_A_TIME
            || sunAnimation.time > HORIZON_B_TIME
        };

        if (isNight)
        {
            bool const sunRisesAtA{sunAnimation.speed > 0.0F};

            sunAnimation.time = sunRisesAtA ? HORIZON_A_TIME : HORIZON_B_TIME;
        }
    }

    // Sun starts straight down i.e. middle of the night
    float constexpr SUN_START_RADIANS{glm::half_pi<float>()};
    // Wrap around the planet once
    float constexpr SUN_END_RADIANS{SUN_START_RADIANS + glm::two_pi<float>()};

    atmosphere.sunEulerAngles = glm::vec3{
        glm::lerp(SUN_START_RADIANS, SUN_END_RADIANS, sunAnimation.time),
        atmosphere.sunEulerAngles.y,
        atmosphere.sunEulerAngles.z
    };

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
        SZG_WARNING("models and modelInverseTransposes out of sync");
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
    SceneBounds const sceneBounds,
    glm::vec3 const sunEulerAngles,
    glm::vec3 const sunlightRGB
) -> DirectionalLightPacked
{
    float constexpr SUNLIGHT_STRENGTH{0.5F};

    return makeDirectional(
        glm::vec4(sunlightRGB, 1.0),
        SUNLIGHT_STRENGTH,
        sunEulerAngles,
        sceneBounds.center,
        sceneBounds.extent
    );
}
auto createMoonlight(
    SceneBounds const sceneBounds,
    float const sunCosine,
    float const sunsetCosine
) -> DirectionalLightPacked
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

    return makeDirectional(
        MOONLIGHT_COLOR_RGBA,
        moonlightStrength,
        STRAIGHT_DOWN_EULER_ANGLES,
        sceneBounds.center,
        sceneBounds.extent
    );
}

// Returns an estimate of the color of sunlight that has reached the
// origin, attenuated due to scattering.
auto computeSunlightColor(Atmosphere const& atmosphere) -> glm::vec4
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

    // Calculations derived from sky.comp, we do a single ray straight WORLD_UP
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

auto Atmosphere::directionToSun() const -> glm::vec3
{
    return -forwardFromEulers(sunEulerAngles);
}

auto Atmosphere::toDeviceEquivalent() const -> AtmospherePacked
{
    // TODO: move these computations out to somewhere more sensible

    glm::vec4 const sunlight{computeSunlightColor(*this)};
    glm::vec3 const sunDirection{glm::normalize(directionToSun())};

    return AtmospherePacked{
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

auto Atmosphere::baked(SceneBounds const sceneBounds) const -> AtmosphereBaked
{
    AtmospherePacked const atmosphere{toDeviceEquivalent()};

    // position of sun as proxy for time
    float const sunCosine{glm::dot(WORLD_UP, atmosphere.directionToSun)};
    float constexpr SUNSET_COSINE{0.06};

    std::optional<DirectionalLightPacked> sunlight{};
    std::optional<DirectionalLightPacked> moonlight{};

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

auto Camera::toDeviceEquivalent(float const aspectRatio) const -> CameraPacked
{
    glm::mat4x4 const proj{projection(aspectRatio)};
    glm::mat4x4 const projViewInverse{glm::inverse(toProjView(aspectRatio))};

    return CameraPacked{
        .projection = proj,
        .inverseProjection = glm::inverse(proj),
        .view = view(),
        .viewInverseTranspose = glm::inverseTranspose(view()),
        .rotation = rotation(),
        .projViewInverse = projViewInverse,
        .forwardWorld = rotation() * glm::vec4(WORLD_FORWARD, 0.0),
        .position = glm::vec4(cameraPosition, 1.0),
    };
}

auto Camera::toProjView(float const aspectRatio) const -> glm::mat4x4
{
    return projection(aspectRatio) * view();
}

auto Camera::rotation() const -> glm::mat4x4
{
    return glm::orientate4(eulerAngles);
}

auto Camera::transform() const -> glm::mat4x4
{
    return transformVk(cameraPosition, eulerAngles);
}

auto Camera::view() const -> glm::mat4x4
{
    return viewVk(cameraPosition, eulerAngles);
}

auto Camera::projection(float const aspectRatio) const -> glm::mat4x4
{
    if (orthographic)
    {
        float const height{glm::tan(glm::radians(fovDegrees) / 2.0F)};

        glm::vec3 const min{-aspectRatio * height, -height, near};
        glm::vec3 const max{aspectRatio * height, height, far};

        return projectionOrthoVk(min, max);
    }

    return projectionVk(PerspectiveProjectionParameters{
        .fov_y = fovDegrees,
        .aspectRatio = aspectRatio,
        .near = near,
        .far = far,
    });
}
} // namespace syzygy