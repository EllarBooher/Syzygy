#pragma once

#include "syzygy/assets/assets.hpp"
#include "syzygy/assets/assetstypes.hpp"
#include "syzygy/assets/mesh.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/scenemesh.hpp"
#include "syzygy/renderer/scenenode.hpp"
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace syzygy
{
struct InputSnapshot;
struct TickTiming;
struct DescriptorAllocator;
struct AssetLibrary;
struct Transform;
} // namespace syzygy

namespace syzygy
{
struct AtmosphereBaked
{
    AtmospherePacked atmosphere{};
    std::vector<DirectionalLightPacked> atmosphereLights{};
};

struct Atmosphere
{
    float planetRadiusMegameters{};
    float atmosphereRadiusMegameters{};

    // Used to attenuate sunlight to provide an estimate of ambient lighting.
    glm::vec3 groundColor{};

    glm::vec3 scatteringRayleighPerMegameter{};
    glm::vec3 absorptionRayleighPerMegameter{};
    float altitudeDecayRayleighMegameters{};

    glm::vec3 scatteringMiePerMegameter{};
    glm::vec3 absorptionMiePerMegameter{};
    float altitudeDecayMieMegameters{};

    glm::vec3 scatteringOzonePerMegameter{};
    glm::vec3 absorptionOzonePerMegameter{};

    [[nodiscard]] auto toDeviceEquivalent() const -> AtmospherePacked;
};

struct DirectionalLight
{
    glm::vec3 color{};
    float strength{};

    std::string name{};

    // Describes the angular radius of a celestial body creating this light
    float angularRadius{};
    float orbitalPeriodDays{};
    float zenith{};
    float azimuth{};

    [[nodiscard]] auto forward() const -> glm::vec3;

    // Requires the bounds that inform the scale of its projection matrix
    [[nodiscard]] auto toDeviceEquivalent(AABB capturedBounds) const
        -> DirectionalLightPacked;
};

struct Camera
{
    bool orthographic{false};
    glm::vec3 cameraPosition{0.0F, 0.0F, 0.0F};
    glm::vec3 eulerAngles{0.0F, 0.0F, 0.0F};
    float fovDegrees{90.0F}; // NOLINT(readability-magic-numbers)
    float near{0.0F};
    float far{1.0F};

    // In all of these methods, aspect ratio is passed as a parameter since it
    // may depend on the drawn surface.

    // Rotates (but does not translate) from camera to world space
    [[nodiscard]] auto rotation() const -> glm::mat4x4;
    // The matrix that transforms from camera to world space
    [[nodiscard]] auto transform() const -> glm::mat4x4;
    // The inverse of transform, transforms from world to camera space
    [[nodiscard]] auto view() const -> glm::mat4x4;
    // Projects from camera space to clip space
    [[nodiscard]] auto projection(float aspectRatio) const -> glm::mat4x4;

    // Gives the projection * view matrix that transforms from world to
    // clip space.
    [[nodiscard]] auto toProjView(float aspectRatio) const -> glm::mat4x4;

    [[nodiscard]] auto toDeviceEquivalent(float aspectRatio) const
        -> CameraPacked;
};

struct SceneTime
{
    static float const DAY_LENGTH_SECONDS;

    bool frozen{false};
    float time{0.0F};

    float speed{1.0F};
    bool skipNight{false};

    bool realisticOrbits{true};
    // Default values taken from Wikipedia
    float tiltPlanet{glm::radians(23.44F)};
    float inclinationLunarOrbit{glm::radians(5.14F)};
};

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct Scene
{
public:
    static Atmosphere const DEFAULT_ATMOSPHERE_EARTH;
    static Camera const DEFAULT_CAMERA;
    static float const DEFAULT_CAMERA_CONTROLLED_SPEED;
    static SceneTime const DEFAULT_SUN_ANIMATION;

    SceneTime time{DEFAULT_SUN_ANIMATION};

    Atmosphere atmosphere{DEFAULT_ATMOSPHERE_EARTH};
    Camera camera{DEFAULT_CAMERA};

    float cameraControlledSpeed{DEFAULT_CAMERA_CONTROLLED_SPEED};

    bool spotlightsRender{false};
    std::vector<SpotLightPacked> spotlights{};

    void calculateShadowBounds();
    // The bounds of the scene that are intended to cast shadows.
    [[nodiscard]] auto shadowBounds() const -> AABB;
    [[nodiscard]] auto bakeAtmosphere(AABB sceneBounds) const
        -> AtmosphereBaked;
    [[nodiscard]] auto
    collectMeshesForRendering(VkDevice, VmaAllocator, DescriptorAllocator&)
        -> std::vector<std::reference_wrapper<MeshRenderResources>>;

    [[nodiscard]] auto atmosphereLights() const
        -> std::span<DirectionalLight const>;
    [[nodiscard]] auto atmosphereLights() -> std::span<DirectionalLight>;

    [[nodiscard]] auto sceneRoot() -> SceneNode&;

    void addAtmosphereLight(DirectionalLight);
    void addSpotlight(glm::vec3 color, Transform transform);

    static auto defaultScene(AssetLibrary&) -> Scene;
    static auto
    diagonalWaveScene(std::optional<AssetPtr<Mesh>> const& initialMesh)
        -> Scene;

    void handleInput(TickTiming, InputSnapshot const&);
    void tick(TickTiming);

private:
    AABB m_shadowBounds{};
    std::unique_ptr<SceneNode> m_sceneRoot{};
    std::vector<DirectionalLight> m_atmosphereLights;
};
// NOLINTEND(misc-non-private-member-variables-in-classes)
} // namespace syzygy