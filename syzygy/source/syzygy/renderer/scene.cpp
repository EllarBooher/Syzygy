#include "scene.hpp"

#include "syzygy/assets/assets.hpp"
#include "syzygy/core/input.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/core/timing.hpp"
#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/geometry/geometrystatics.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/renderer/lights.hpp"
#include <array>
#include <cassert>
#include <functional>
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <glm/ext/vector_relational.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/projection.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <span>
#include <spdlog/fmt/bundled/core.h>
#include <utility>

namespace syzygy
{
struct DescriptorAllocator;
} // namespace syzygy

namespace
{
void pushDefaultAtmosphereLights(syzygy::Scene& scene)
{
    glm::vec3 SUNLIGHT_RGB{1.0F};

    glm::vec3 constexpr STRAIGHT_DOWN_EULER_ANGLES{
        -glm::half_pi<float>(), 0.0F, 0.0F
    };

    float constexpr SUNLIGHT_STRENGTH{10.0F};
    float constexpr SUN_ANGULAR_RADIUS{glm::radians(16.0F / 60.0F)};
    float constexpr SUN_ORBITAL_PERIOD_DAYS = 365.0F;

    float constexpr SUNSET_COSINE{0.06};

    float constexpr MOONRISE_LENGTH{0.12};

    // TODO: Compute moonlight strength and color based on sun position
    float const MOONLIGHT_STRENGTH{1.0F};
    float constexpr MOON_ANGULAR_RADIUS{glm::radians(16.0F / 60.0F)};
    float constexpr MOON_ORBITAL_PERIOD_DAYS = 27.3F;

    glm::vec3 constexpr MOONLIGHT_COLOR_RGB{0.3, 0.4, 0.6};

    scene.addAtmosphereLight(syzygy::DirectionalLight{
        .color = SUNLIGHT_RGB,
        .strength = SUNLIGHT_STRENGTH,
        .name = "Sun",
        .angularRadius = SUN_ANGULAR_RADIUS,
        .orbitalPeriodDays = SUN_ORBITAL_PERIOD_DAYS,
        .zenith = 0.0F,
        .azimuth = 0.0F,
    });
    scene.addAtmosphereLight(syzygy::DirectionalLight{
        .color = MOONLIGHT_COLOR_RGB,
        .strength = MOONLIGHT_STRENGTH,
        .name = "Moon",
        .angularRadius = MOON_ANGULAR_RADIUS,
        .orbitalPeriodDays = MOON_ORBITAL_PERIOD_DAYS,
        .zenith = 0.0F,
        .azimuth = 1.0F,
    });
}
} // namespace

namespace syzygy
{
float constexpr METERS_PER_MEGAMETER{1'000'000.0};
float constexpr METERS_PER_KILOMETER{1'000.0};
float constexpr KILOMETERS_PER_MEGAMETER{1'000.0};

// Values derived from:
// "A Scalable and Production Ready Sky and Atmosphere Rendering Technique"
// by Sébastien Hillaire (2020) Available at
// https://sebh.github.io/publications/egsr2020.pdf
Atmosphere const Scene::DEFAULT_ATMOSPHERE_EARTH{
    Atmosphere{
        .planetRadiusMegameters = 6.360F,
        .atmosphereRadiusMegameters = 6.420F,

        .groundColor = glm::vec3{1.0, 1.0, 1.0},

        .scatteringRayleighPerMegameter = glm::vec3{5.802F, 13.558F, 33.1F},
        .absorptionRayleighPerMegameter = glm::vec3{0.0F},
        .altitudeDecayRayleighMegameters = 8.0F / KILOMETERS_PER_MEGAMETER,

        .scatteringMiePerMegameter = glm::vec3{3.996F},
        .absorptionMiePerMegameter = glm::vec3{4.40F},
        .altitudeDecayMieMegameters = 1.2F / KILOMETERS_PER_MEGAMETER,

        .scatteringOzonePerMegameter = glm::vec3{0.0F},
        .absorptionOzonePerMegameter = glm::vec3{0.650F, 1.881F, 0.085F},
    } // namespace syzygy
};

Camera const Scene::DEFAULT_CAMERA{Camera{
    .cameraPosition = glm::vec3(0.0F, -15.0F, -20.0F),
    .eulerAngles = glm::vec3(0.0F, 0.0F, 0.0F),
    .fovDegrees = 70.0F,
    .near = 0.1F,
    .far = 10000.0F,
}};

float const Scene::DEFAULT_CAMERA_CONTROLLED_SPEED{20.0F};

SceneTime const Scene::DEFAULT_SUN_ANIMATION{SceneTime{
    .frozen = false, .time = 0.0F, .speed = 100.0F, .skipNight = false
}};

float const SceneTime::DAY_LENGTH_SECONDS{60.0F * 60.0F * 24.0F};

auto Scene::shadowBounds() const -> AABB { return m_shadowBounds; }

auto Scene::bakeAtmosphere(AABB sceneBounds) const -> AtmosphereBaked
{
    syzygy::AtmosphereBaked result{};
    result.atmosphere = atmosphere.toDeviceEquivalent();

    for (auto const& atmosphereLight : m_atmosphereLights)
    {
        result.atmosphereLights.push_back(
            atmosphereLight.toDeviceEquivalent(sceneBounds)
        );
    }

    return result;
}

void Scene::calculateShadowBounds()
{
    m_shadowBounds = {};

    // Go over every transformed vertex of instances' AABBs
    // TODO: find a simpler algorithm, there is lots of symmetry to take
    // advantage of

    glm::vec3 minimumPoint{std::numeric_limits<float>::max()};
    glm::vec3 maximumPoint{std::numeric_limits<float>::lowest()};

    for (MeshInstanced const& instance : m_geometry)
    {
        if (!instance.castsShadow || !instance.render)
        {
            continue;
        }

        std::optional<syzygy::AssetRef<Mesh>> meshRef{instance.getMesh()};

        if (!meshRef.has_value() || meshRef.value().get().data == nullptr)
        {
            continue;
        }

        Mesh const& mesh{*meshRef.value().get().data};

        AABB::Vertices const vertices{mesh.vertexBounds.collectVertices()};

        for (Transform const& transform : instance.transforms)
        {
            glm::mat4x4 const transformation{transform.toMatrix()};

            for (glm::vec3 const vertex : vertices)
            {
                glm::vec3 const worldPosition{
                    transformation * glm::vec4{vertex, 1.0F}
                };

                minimumPoint = glm::min(worldPosition, minimumPoint);
                maximumPoint = glm::max(worldPosition, maximumPoint);
            }
        }
    }

    if (glm::any(glm::greaterThan(minimumPoint, maximumPoint)))
    {
        // This only occurs when not a single valid vertex was found
        // Either the mesh vertices or transforms are bad
        return;
    }

    m_shadowBounds = AABB::create(minimumPoint, maximumPoint);
}

auto Scene::geometry() const -> std::span<MeshInstanced const>
{
    return m_geometry;
}

auto Scene::geometry() -> std::span<MeshInstanced> { return m_geometry; }

auto Scene::atmosphereLights() const -> std::span<DirectionalLight const>
{
    return m_atmosphereLights;
}

auto Scene::atmosphereLights() -> std::span<DirectionalLight>
{
    return m_atmosphereLights;
}

void Scene::addMeshInstance(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    std::optional<AssetPtr<Mesh>> const& mesh,
    InstanceAnimation const animation,
    std::string const& name,
    std::span<Transform const> const transforms,
    bool const castsShadow
)
{
    MeshInstanced instance{};
    instance.render = true;
    instance.castsShadow = castsShadow;
    // TODO: name deduplication
    instance.name = fmt::format("meshInstanced_{}", name);

    if (mesh.has_value())
    {
        instance.setMesh(mesh.value());
    }

    instance.prepareDescriptors(device, descriptorAllocator);
    instance.animation = animation;

    instance.originals.insert(
        instance.originals.begin(), transforms.begin(), transforms.end()
    );
    instance.transforms.insert(
        instance.transforms.begin(), transforms.begin(), transforms.end()
    );

    VkDeviceSize const bufferSize{
        static_cast<VkDeviceSize>(instance.originals.size())
    };

    instance.models = std::make_unique<TStagedBuffer<glm::mat4x4>>(
        TStagedBuffer<glm::mat4x4>::allocate(
            device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, allocator, bufferSize
        )
    );
    instance
        .modelInverseTransposes = std::make_unique<TStagedBuffer<glm::mat4x4>>(
        TStagedBuffer<glm::mat4x4>::allocate(
            device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, allocator, bufferSize
        )
    );

    for (Transform const& model : instance.originals)
    {
        glm::mat4x4 const matrix{model.toMatrix()};

        instance.models->push(matrix);
        instance.modelInverseTransposes->push(glm::inverseTranspose(matrix));
    }

    m_geometry.push_back(std::move(instance));
}

void Scene::addAtmosphereLight(DirectionalLight const light)
{
    m_atmosphereLights.push_back(light);
}

void Scene::addSpotlight(glm::vec3 const color, Transform const transform)
{
    SpotlightParams const lightParams{
        .color = glm::vec4{color, 1.0},
        .strength = 1000.0F,
        .falloffFactor = 1.0F,
        .falloffDistance = 1.0F,
        .verticalFOVDegrees = 30.0F,
        .horizontalScale = 1.0F,
        .eulerAngles = transform.eulerAnglesRadians,
        .position = transform.translation,
        .near = 0.1F,
        .far = 1000.0F
    };

    spotlights.push_back(makeSpot(lightParams));

    spotlightsRender = true;
}

auto Scene::defaultScene(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    AssetLibrary& library
) -> Scene
{
    Scene scene{};

    pushDefaultAtmosphereLights(scene);

    glm::vec3 const floatingPosition{glm::vec3{0.0F, -8.0F, 0.0F}};
    glm::vec3 constexpr MESH_SCALE{5.0F};
    glm::vec3 constexpr MESH_OFFSET{0.0F, 0.0F, 6.0F};

    scene.addMeshInstance(
        device,
        allocator,
        descriptorAllocator,
        library.defaultMesh(AssetLibrary::DefaultMeshAssets::Cube),
        InstanceAnimation::None,
        "Model_1",
        std::array<Transform, 1>{Transform{
            .translation = floatingPosition + MESH_OFFSET,
            .eulerAnglesRadians = glm::vec3{0.0F},
            .scale = MESH_SCALE
        }}
    );
    scene.addMeshInstance(
        device,
        allocator,
        descriptorAllocator,
        library.defaultMesh(AssetLibrary::DefaultMeshAssets::Cube),
        InstanceAnimation::None,
        "Model_2",
        std::array<Transform, 1>{Transform{
            .translation = floatingPosition - MESH_OFFSET,
            .eulerAnglesRadians = glm::vec3{0.0F},
            .scale = MESH_SCALE
        }}
    );

    Transform const floorTransform{
        .translation = glm::vec3{0.0F, -1.0F, 0.0F},
        .eulerAnglesRadians = glm::vec3{0.0F},
        .scale = glm::vec3{20.0F, 1.0F, 20.0F}
    };

    scene.addMeshInstance(
        device,
        allocator,
        descriptorAllocator,
        library.defaultMesh(AssetLibrary::DefaultMeshAssets::Plane),
        InstanceAnimation::None,
        "Floor",
        std::array<Transform, 1>{Transform{floorTransform}}
    );

    glm::vec3 const spotlightOffset{-20.0};

    scene.addSpotlight(
        glm::vec3{1.0, 0.0, 0.0},
        Transform::lookAt(
            Ray::create(floatingPosition + spotlightOffset, floatingPosition),
            glm::vec3{1.0F}
        )
    );

    scene.spotlightsRender = true;

    return scene;
}

auto Scene::diagonalWaveScene(
    VkDevice const device,
    VmaAllocator const allocator,
    DescriptorAllocator& descriptorAllocator,
    std::optional<AssetPtr<Mesh>> const& initialMesh
) -> Scene
{
    Scene scene{};

    pushDefaultAtmosphereLights(scene);

    int32_t constexpr COORDINATE_MIN{-40};
    int32_t constexpr COORDINATE_MAX{40};

    { // Floor
        std::array<Transform, 1> const transform{Transform{
            .translation = glm::vec3{0.0F},
            .eulerAnglesRadians = glm::vec3{0.0F},
            .scale = glm::vec3{400.0F, 1.0F, 400.0F}
        }};

        scene.addMeshInstance(
            device,
            allocator,
            descriptorAllocator,
            initialMesh,
            InstanceAnimation::None,
            "Floor",
            transform,
            false
        );
    }

    { // Cubes
        std::vector<Transform> transforms{};

        for (int32_t x{COORDINATE_MIN}; x <= COORDINATE_MAX; x++)
        {
            for (int32_t z{COORDINATE_MIN}; z <= COORDINATE_MAX; z++)
            {
                glm::vec3 const position{
                    static_cast<float>(x), -4.0, static_cast<float>(z)
                };
                glm::vec3 const eulerAngles{glm::eulerAngles(randomQuat())};
                glm::vec3 const scale{0.2F};

                transforms.push_back(Transform{
                    .translation = position,
                    .eulerAnglesRadians = eulerAngles,
                    .scale = scale,
                });
            }
        }

        scene.addMeshInstance(
            device,
            allocator,
            descriptorAllocator,
            initialMesh,
            InstanceAnimation::Diagonal_Wave,
            "DiagonalWave",
            transforms
        );
    }

    return scene;
}

void Scene::handleInput(TickTiming const lastFrame, InputSnapshot const& input)
{
    glm::vec2 const cursorDelta{input.cursor.delta()};

    glm::vec2 const adjustedCursorDelta{
        cursorDelta.x / 100.0F, cursorDelta.y / 200.0F
    };

    // left to right
    camera.eulerAngles.z += adjustedCursorDelta.x;

    // up and down, avoid flipping camera
    camera.eulerAngles.x = glm::clamp(
        camera.eulerAngles.x - adjustedCursorDelta.y,
        -glm::half_pi<float>(),
        glm::half_pi<float>()
    );

    glm::mat3x3 const transform{camera.transform()};

    glm::vec3 const forward{transform * WORLD_FORWARD};
    glm::vec3 const right{transform * WORLD_RIGHT};
    // We do not rotate up, since the controls would be disorienting
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
} // namespace syzygy

namespace
{
void tickMeshInstance(
    syzygy::TickTiming const lastFrame, syzygy::MeshInstanced& instance
)
{
    if (instance.models == nullptr
        || instance.modelInverseTransposes == nullptr)
    {
        return;
    }

    std::span<glm::mat4x4> const models{instance.models->mapValidStaged()};
    std::span<glm::mat4x4> const modelInverseTransposes{
        instance.modelInverseTransposes->mapValidStaged()
    };

    if (models.size() != modelInverseTransposes.size())
    {
        SZG_WARNING("models and modelInverseTransposes out of sync");
        return;
    }

    // TODO: extract and generalize these animations
    switch (instance.animation)
    {
    case syzygy::InstanceAnimation::Diagonal_Wave:
        for (size_t index{0}; index < instance.originals.size(); index++)
        {
            syzygy::Transform const& original{instance.originals[index]};
            syzygy::Transform& current{instance.transforms[index]};

            double const timeOffset{
                (original.translation.x - (-10) + original.translation.z - (-10)
                )
                / 3.1415
            };

            double const y{glm::sin(lastFrame.timeElapsedSeconds + timeOffset)};
            current.translation = original.translation
                                + glm::vec3{0.0F, static_cast<float>(y), 0.0F};
        }
        break;
    case syzygy::InstanceAnimation::Spin_Along_World_Up:
        for (size_t index{0}; index < instance.originals.size(); index++)
        {
            syzygy::Transform& current{instance.transforms[index]};

            current.eulerAnglesRadians.z +=
                static_cast<float>(lastFrame.deltaTimeSeconds);
        }
        break;
    default:
        break;
    }

    // TODO: this should be moved to a separate method that prepares all
    // rendering data for a scene
    for (size_t index{0}; index < instance.transforms.size(); index++)
    {
        syzygy::Transform const& transform{instance.transforms[index]};

        glm::mat4x4 const model{transform.toMatrix()};
        models[index] = model;
        modelInverseTransposes[index] = glm::inverseTranspose(model);
    }
}
} // namespace

namespace syzygy
{
void Scene::tick(TickTiming const lastFrame)
{
    if (!time.frozen)
    {
        time.time += time.speed * static_cast<float>(lastFrame.deltaTimeSeconds)
                   / SceneTime::DAY_LENGTH_SECONDS;
    }

    if (time.realisticOrbits && m_atmosphereLights.size() >= 2)
    {
        // Assume circular orbits with no precession

        float const timeDays{time.time};

        DirectionalLight& sun{m_atmosphereLights[0]};
        DirectionalLight& moon{m_atmosphereLights[1]};

        float const planetTheta{
            glm::two_pi<float>() * timeDays / sun.orbitalPeriodDays
        };

        glm::vec3 const sunToPlanetDelta{
            glm::vec3{glm::sin(planetTheta), 0.0F, glm::cos(planetTheta)}
        };

        float const moonTheta{
            glm::two_pi<float>() * timeDays / moon.orbitalPeriodDays
        };

        glm::mat3x3 const moonInclinationTransform{
            glm::rotate(time.inclinationLunarOrbit, WORLD_FORWARD)
        };

        glm::vec3 const planetToMoonDelta =
            moonInclinationTransform
            * glm::vec3{glm::sin(moonTheta), 0.0F, glm::cos(moonTheta)};

        glm::mat3x3 const planetTiltTransform{
            glm::rotate(time.tiltPlanet, WORLD_FORWARD)
        };

        float const viewTheta{glm::two_pi<float>() * timeDays};

        // Viewer on equator
        glm::vec3 const planetToViewDelta =
            planetTiltTransform
            * glm::vec3{glm::sin(viewTheta), 0.0F, glm::cos(viewTheta)};

        // Distances are in megameters
        float constexpr PLANET_ORBITAL_DISTANCE = 149'597.87F;
        float constexpr MOON_ORBITAL_DISTANCE = 382.500F;
        float const planetRadius = atmosphere.planetRadiusMegameters;

        glm::vec3 const sunPosition{glm::vec3{0.0F}};
        glm::vec3 const planetPosition{
            sunPosition + PLANET_ORBITAL_DISTANCE * sunToPlanetDelta
        };
        glm::vec3 const moonPosition{
            planetPosition + MOON_ORBITAL_DISTANCE * planetToMoonDelta
        };
        glm::vec3 const viewPosition{
            planetPosition + planetRadius * planetToViewDelta
        };

        // Compute apparent position of sun and moon in sky

        glm::vec3 const toSun{glm::normalize(sunPosition - viewPosition)};
        glm::vec3 const toMoon{glm::normalize(moonPosition - viewPosition)};
        // Eventually everything is rendered with WORLD_UP being up, but for the
        // simulation we need an actual up
        glm::vec3 const surfaceUp{glm::normalize(viewPosition - planetPosition)
        };

        glm::vec3 const surfaceForward{
            glm::normalize(planetTiltTransform * WORLD_UP)
        };
        glm::vec3 const surfaceRight{
            glm::normalize(glm::cross(surfaceForward, surfaceUp))
        };

        float const sunZenith{glm::acos(glm::dot(toSun, surfaceUp))};
        float const moonZenith{glm::acos(glm::dot(toMoon, surfaceUp))};

        glm::vec3 const toSunProjected{
            glm::normalize(toSun - glm::proj(toSun, surfaceUp))
        };
        glm::vec3 const toMoonProjected{
            glm::normalize(toMoon - glm::proj(toMoon, surfaceUp))
        };

        float const sunAzimuth{
            glm::sign(dot(toSunProjected, surfaceRight))
            * glm::acos(dot(toSunProjected, surfaceForward))
        };
        float const moonAzimuth{
            glm::sign(dot(toMoonProjected, surfaceRight))
            * glm::acos(dot(toMoonProjected, surfaceForward))
        };

        sun.zenith = sunZenith;
        sun.azimuth = sunAzimuth;

        moon.zenith = moonZenith;
        moon.azimuth = moonAzimuth;
    }
    else if (!m_atmosphereLights.empty())
    {
        if (time.skipNight && !time.frozen)
        {
            DirectionalLight const& sun{m_atmosphereLights[0]};

            float constexpr SUNSET_ANGLE_RADIANS{0.1F};

            // The times when the sun dips far enough the horizon for us to
            // consider it night
            // Normal and reverse are weird names, but help when you
            // consider that time can run backwards
            float constexpr NORMAL_SUNSET_ZENITH{
                glm::half_pi<float>() + SUNSET_ANGLE_RADIANS
            };
            float constexpr REVERSE_SUNSET_ZENITH{
                glm::three_over_two_pi<float>() - SUNSET_ANGLE_RADIANS
            };

            float tickedZenith{glm::mod(
                glm::two_pi<float>() * time.time / sun.orbitalPeriodDays,
                glm::two_pi<float>()
            )};

            bool const willBeNight{
                tickedZenith > NORMAL_SUNSET_ZENITH
                && tickedZenith < REVERSE_SUNSET_ZENITH
            };

            if (willBeNight)
            {
                float const deltaZenithToSunrise =
                    time.speed > 0.0F
                        ? glm::abs(REVERSE_SUNSET_ZENITH - tickedZenith)
                        : glm::abs(tickedZenith - NORMAL_SUNSET_ZENITH);

                float const daysUntilSunrise = sun.orbitalPeriodDays
                                             * deltaZenithToSunrise
                                             / glm::two_pi<float>();

                time.time += glm::sign(time.speed) * daysUntilSunrise;
            }
        }

        for (auto& light : m_atmosphereLights)
        {
            light.zenith =
                glm::two_pi<float>() * time.time / light.orbitalPeriodDays;

            light.zenith = glm::mod(light.zenith, glm::two_pi<float>());
        }
    }

    for (auto& instance : m_geometry)
    {
        tickMeshInstance(lastFrame, instance);
    }
}

namespace
{

// Returns an estimate of the color of sunlight that has reached the
// origin, attenuated due to scattering.
auto computeSunlightColor(
    Atmosphere const& atmosphere, glm::vec3 const directionToSun
) -> glm::vec4
{
    float const surfaceCosine{
        glm::dot(directionToSun, glm::vec3{0.0, -1.0, 0.0})
    };
    if (surfaceCosine <= 0.0)
    {
        return {0.0, 0.0, 0.0, 1.0};
    }

    float const atmosphereRadiusMeters =
        atmosphere.atmosphereRadiusMegameters * METERS_PER_MEGAMETER;
    float const planetRadiusMeters =
        atmosphere.planetRadiusMegameters * METERS_PER_MEGAMETER;

    glm::vec3 const start{0.0, -planetRadiusMeters, 0.0};
    float outDistance{0.0};
    if (!glm::intersectRaySphere(
            start,
            directionToSun,
            glm::vec3(0.0),
            atmosphereRadiusMeters * atmosphereRadiusMeters,
            outDistance
        ))
    {
        glm::vec4 constexpr RAW_SUNLIGHT_COLOR{1.0, 1.0, 1.0, 1.0};
        return RAW_SUNLIGHT_COLOR;
    };

    float const atmosphereThickness{outDistance};

    float const altitudeDecayRayleighMeters =
        atmosphere.altitudeDecayRayleighMegameters * METERS_PER_MEGAMETER;
    float const altitudeDecayMieMeters =
        atmosphere.altitudeDecayMieMegameters * METERS_PER_MEGAMETER;

    // Calculations derived from sky.comp, we do a single ray straight up
    // to get an idea of the ambient color
    float const opticalDepthRayleigh{
        altitudeDecayRayleighMeters / surfaceCosine
        * (1.0F - glm::exp(-atmosphereThickness / altitudeDecayRayleighMeters))
    };
    float const opticalDepthMie{
        altitudeDecayMieMeters / surfaceCosine
        * (1.0F - glm::exp(-atmosphereThickness / altitudeDecayMieMeters))
    };

    glm::vec3 const scatteringRayleighPerMeter =
        atmosphere.scatteringRayleighPerMegameter / METERS_PER_MEGAMETER;
    glm::vec3 const scatteringMiePerMeter =
        atmosphere.scatteringMiePerMegameter / METERS_PER_MEGAMETER;

    glm::vec3 const tau{
        scatteringRayleighPerMeter * opticalDepthRayleigh
        + 1.1F * scatteringMiePerMeter * opticalDepthMie
    };
    glm::vec3 const attenuation{glm::exp(-tau)};

    return {attenuation, 1.0};
}
} // namespace

auto DirectionalLight::forward() const -> glm::vec3
{
    return -(
        glm::sin(zenith) * glm::sin(azimuth) * WORLD_RIGHT
        + glm::cos(zenith) * WORLD_UP
        + glm::sin(zenith) * glm::cos(azimuth) * WORLD_FORWARD
    );
}
auto DirectionalLight::toDeviceEquivalent(AABB const capturedBounds) const
    -> DirectionalLightPacked
{
    glm::mat4x4 const view{viewVk(glm::vec3{0.0}, eulersFromForward(forward()))
    };
    glm::mat4x4 const projection{projectionOrthoAABBVk(view, capturedBounds)};

    return DirectionalLightPacked{
        .color = glm::vec4{color, 1.0},
        .forward = glm::vec4{forward(), 0.0},
        .projection = projection,
        .view = view,
        .strength = strength,
        .angularRadius = angularRadius,
    };
}

auto Atmosphere::toDeviceEquivalent() const -> AtmospherePacked
{

    return AtmospherePacked{
        .scatteringRayleighPerMm = scatteringRayleighPerMegameter,
        .densityScaleRayleighMm = altitudeDecayRayleighMegameters,
        .absorptionRayleighPerMm = absorptionRayleighPerMegameter,
        .planetRadiusMm = planetRadiusMegameters,
        .scatteringMiePerMm = scatteringMiePerMegameter,
        .densityScaleMieMm = altitudeDecayMieMegameters,
        .absorptionMiePerMm = absorptionMiePerMegameter,
        .atmosphereRadiusMm = atmosphereRadiusMegameters,
        .scatteringOzonePerMm = scatteringOzonePerMegameter,
        .absorptionOzonePerMm = absorptionOzonePerMegameter,
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
        .fov_y_degrees = fovDegrees,
        .aspectRatio = aspectRatio,
        .near = near,
        .far = far,
    });
}

void MeshInstanced::setMesh(AssetPtr<Mesh> meshAsset)
{
    m_mesh = std::move(meshAsset);
    m_surfaceDescriptorsDirty = true;

    if (AssetShared<Mesh> const pMesh{m_mesh.lock()};
        pMesh != nullptr && pMesh->data != nullptr)
    {
        Mesh const& mesh{*pMesh->data};
        AABB const meshBounds{mesh.vertexBounds};

        float const smallestDimension{glm::compMin(meshBounds.halfExtent)};
        float constexpr MINIMUM_DIMENSION{0.01F};

        float const scaleFactor{
            1.0F / glm::max(smallestDimension, MINIMUM_DIMENSION)
        };

        assert(transforms.size() == originals.size());
        for (size_t child{0}; child < transforms.size(); child++)
        {
            transforms[child].scale = originals[child].scale * scaleFactor;
        }
    }
}

void MeshInstanced::prepareDescriptors(
    VkDevice const device, DescriptorAllocator& descriptorAllocator
)
{
    if (!m_surfaceDescriptorsDirty || m_mesh.lock() == nullptr
        || m_mesh.lock()->data == nullptr)
    {
        return;
    }

    Mesh const& mesh{*m_mesh.lock()->data};

    m_surfaceDescriptorsDirty = false;

    while (m_surfaceDescriptors.size() < mesh.surfaces.size())
    {
        std::optional<MaterialDescriptors> descriptorsResult{
            MaterialDescriptors::create(device, descriptorAllocator)
        };
        if (!descriptorsResult.has_value())
        {
            SZG_ERROR(
                "Failed to allocate MaterialDescriptors while setting mesh."
            );
            m_mesh = {};
            return;
        }

        m_surfaceDescriptors.push_back(std::move(descriptorsResult).value());
    }

    m_surfaceMaterialOverrides.resize(mesh.surfaces.size());

    for (size_t index{0}; index < mesh.surfaces.size(); index++)
    {
        GeometrySurface const& surface{mesh.surfaces[index]};
        MaterialDescriptors const& descriptors{m_surfaceDescriptors[index]};
        MaterialData const& overrides{m_surfaceMaterialOverrides[index]};

        MaterialData const activeMaterials{
            .ORM = overrides.ORM.lock() != nullptr ? overrides.ORM
                                                   : surface.material.ORM,
            .normal = overrides.normal.lock() != nullptr
                        ? overrides.normal
                        : surface.material.normal,
            .color = overrides.color.lock() != nullptr ? overrides.color
                                                       : surface.material.color,
        };

        descriptors.write(activeMaterials);
    }
}

auto MeshInstanced::getMesh() const -> std::optional<AssetRef<Mesh>>
{
    if (m_mesh.lock() == nullptr)
    {
        return std::nullopt;
    }

    return *m_mesh.lock();
}

auto MeshInstanced::getMaterialOverrides() const
    -> std::span<MaterialData const>
{
    std::shared_ptr<Asset<Mesh> const> const mesh{m_mesh.lock()};
    if (mesh == nullptr || mesh->data == nullptr)
    {
        return {};
    }

    return std::span<MaterialData const>{
        m_surfaceMaterialOverrides.begin(),
        m_surfaceMaterialOverrides.begin()
            + static_cast<std::int64_t>(mesh->data->surfaces.size())
    };
}

void MeshInstanced::setMaterialOverrides(
    size_t const surface, MaterialData const& materialOverride
)
{
    m_surfaceDescriptorsDirty = true;
    if (surface >= m_surfaceMaterialOverrides.size())
    {
        return;
    }

    m_surfaceMaterialOverrides[surface] = materialOverride;
}

auto MeshInstanced::getMeshDescriptors() const
    -> std::span<MaterialDescriptors const>
{
    return m_surfaceDescriptors;
}
} // namespace syzygy