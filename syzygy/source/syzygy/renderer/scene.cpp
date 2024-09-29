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
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
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

    .earthRadiusMeters = 6360000,
    .atmosphereRadiusMeters = 6420000,

    .groundColor = glm::vec3{1.0, 1.0, 1.0},

    .scatteringCoefficientRayleigh = glm::vec3(0.0000058, 0.0000135, 0.0000331),
    .altitudeDecayRayleigh = 7994.0,

    .scatteringCoefficientMie = glm::vec3(0.000003996),
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
    .frozen = false, .time = 0.5F, .speed = 100.0F, .skipNight = false
}};

float const SunAnimation::DAY_LENGTH_SECONDS{60.0F * 60.0F * 24.0F};

auto Scene::shadowBounds() const -> AABB { return m_shadowBounds; }

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

void Scene::addSpotlight(glm::vec3 const color, Transform const transform)
{
    SpotlightParams const lightParams{
        .color = glm::vec4{color, 1.0},
        .strength = 300.0F,
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
    std::optional<AssetPtr<Mesh>> const& initialMesh
) -> Scene
{
    Scene scene{};

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

    glm::vec3 const floatingMeshPosition{4.0F * WORLD_UP};

    { // Single floating demo mesh
        std::array<Transform, 1> const transform{Transform{
            .translation = floatingMeshPosition,
            .eulerAnglesRadians = glm::vec3{0.0F},
            .scale = glm::vec3{1.0F}
        }};

        scene.addMeshInstance(
            device,
            allocator,
            descriptorAllocator,
            initialMesh,
            InstanceAnimation::None,
            "Floating",
            transform
        );
    }

    { // Lights to shine on mesh
        SpotlightParams const sharedParams{
            .strength = 30.0F,
            .falloffFactor = 1.0F,
            .falloffDistance = 1.0F,
            .verticalFOVDegrees = 60.0F,
            .horizontalScale = 1.0F,
            .near = 0.1F,
            .far = 1000.0F
        };

        glm::vec3 const lightsHeight{8.0F * WORLD_UP};
        glm::vec3 const lightsOffset{8.0F * (WORLD_FORWARD + WORLD_RIGHT)};

        {
            Transform const lightTransform{Transform::lookAt(
                Ray::create(
                    floatingMeshPosition + lightsHeight + lightsOffset,
                    floatingMeshPosition
                ),
                glm::vec3{1.0F}
            )};
            SpotlightParams lightParams{sharedParams};
            lightParams.color = glm::vec4(0.0, 1.0, 0.0, 1.0);
            lightParams.eulerAngles = lightTransform.eulerAnglesRadians;
            lightParams.position = lightTransform.translation;

            scene.spotlights.push_back(makeSpot(lightParams));
        }
        {
            Transform const lightTransform{Transform::lookAt(
                Ray::create(
                    floatingMeshPosition + lightsHeight - lightsOffset,
                    floatingMeshPosition
                ),
                glm::vec3{1.0F}
            )};
            SpotlightParams lightParams{sharedParams};
            lightParams.color = glm::vec4(1.0, 0.0, 0.0, 1.0);
            lightParams.eulerAngles = lightTransform.eulerAnglesRadians;
            lightParams.position = lightTransform.translation;

            scene.spotlights.push_back(makeSpot(lightParams));
        }
    };

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

    for (auto& instance : m_geometry)
    {
        tickMeshInstance(lastFrame, instance);
    }
}

namespace
{
auto createSunlight(
    AABB const sceneBounds,
    glm::vec3 const sunEulerAngles,
    glm::vec3 const sunlightRGB
) -> DirectionalLightPacked
{
    float constexpr SUNLIGHT_STRENGTH{4.0F};

    return makeDirectional(
        glm::vec4(sunlightRGB, 1.0),
        SUNLIGHT_STRENGTH,
        sunEulerAngles,
        sceneBounds
    );
}
auto createMoonlight(
    AABB const sceneBounds, float const sunCosine, float const sunsetCosine
) -> DirectionalLightPacked
{
    float constexpr MOONRISE_LENGTH{0.08};

    float const moonlightStrength{
        0.02F
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
        sceneBounds
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
                      * glm::dot(sunDirection, WORLD_UP) / glm::pi<float>(),
        .atmosphereRadiusMeters = atmosphereRadiusMeters,
        .sunlightColor = glm::vec3(sunlight),
        .groundColor = groundColor,
    };
}

auto Atmosphere::baked(AABB const sceneBounds) const -> AtmosphereBaked
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