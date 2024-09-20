#pragma once

#include "syzygy/assets/assets.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/geometry/transform.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/gputypes.hpp"
#include "syzygy/renderer/material.hpp"
#include <functional>
#include <glm/mat4x4.hpp>
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
} // namespace syzygy

namespace syzygy
{
struct AtmosphereBaked
{
    AtmospherePacked atmosphere{};
    std::optional<DirectionalLightPacked> sunlight{};
    std::optional<DirectionalLightPacked> moonlight{};
};

struct Atmosphere
{
    glm::vec3 sunEulerAngles{0.0, 0.0, 0.0};

    float earthRadiusMeters{0.0};
    float atmosphereRadiusMeters{0.0};

    // Used to attenuate sunlight to provide an estimate of ambient lighting.
    glm::vec3 groundColor{1.0};

    glm::vec3 scatteringCoefficientRayleigh{1.0};
    float altitudeDecayRayleigh{1.0};

    glm::vec3 scatteringCoefficientMie{1.0};
    float altitudeDecayMie{1.0};

    [[nodiscard]] auto directionToSun() const -> glm::vec3;

    [[nodiscard]] auto toDeviceEquivalent() const -> AtmospherePacked;
    [[nodiscard]] auto baked(AABB) const -> AtmosphereBaked;
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

// Some hardcoded animations for demo purposes
enum class InstanceAnimation
{
    None,
    FIRST = None,

    Diagonal_Wave,

    Spin_Along_World_Up,
    LAST = Spin_Along_World_Up
};

// TODO: encapsulate all fields
// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct MeshInstanced
{
    bool render{false};
    bool castsShadow{true};
    std::string name{};

    InstanceAnimation animation{InstanceAnimation::None};

    // This transform data + gpu buffers requires manual management for now

    std::vector<Transform> originals{};
    std::vector<Transform> transforms{};

    std::unique_ptr<TStagedBuffer<glm::mat4x4>> models{};
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> modelInverseTransposes{};

    void setMesh(AssetPtr<Mesh>);
    void prepareDescriptors(VkDevice, DescriptorAllocator&);

    [[nodiscard]] auto getMesh() const -> std::optional<AssetRef<Mesh>>;
    [[nodiscard]] auto getMeshDescriptors() const
        -> std::span<MaterialDescriptors const>;

private:
    bool m_surfaceDescriptorsDirty{false};

    AssetPtr<Mesh> m_mesh{};
    std::vector<MaterialDescriptors> m_surfaceDescriptors{};
};
// NOLINTEND(misc-non-private-member-variables-in-classes)

struct SunAnimation
{
    static float const DAY_LENGTH_SECONDS;

    bool frozen{false};
    float time{0.0F};
    float speed{1.0F};
    bool skipNight{false};
};

// NOLINTBEGIN(misc-non-private-member-variables-in-classes)
struct Scene
{
public:
    static Atmosphere const DEFAULT_ATMOSPHERE_EARTH;
    static Camera const DEFAULT_CAMERA;
    static float const DEFAULT_CAMERA_CONTROLLED_SPEED;
    static SunAnimation const DEFAULT_SUN_ANIMATION;

    SunAnimation sunAnimation{DEFAULT_SUN_ANIMATION};

    Atmosphere atmosphere{DEFAULT_ATMOSPHERE_EARTH};
    Camera camera{DEFAULT_CAMERA};

    float cameraControlledSpeed{DEFAULT_CAMERA_CONTROLLED_SPEED};

    bool spotlightsRender{false};
    std::vector<SpotLightPacked> spotlights{};

    void calculateShadowBounds();
    // The bounds of the scene that are intended to cast shadows.
    [[nodiscard]] auto shadowBounds() const -> AABB;

    [[nodiscard]] auto geometry() const -> std::span<MeshInstanced const>;
    [[nodiscard]] auto geometry() -> std::span<MeshInstanced>;

    void addMeshInstance(
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        std::optional<AssetPtr<Mesh>> const&,
        InstanceAnimation,
        std::string const& name,
        std::span<Transform const> transforms,
        bool castsShadow = true
    );
    void addSpotlight(glm::vec3 color, Transform transform);

    static auto defaultScene(
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        std::optional<AssetPtr<Mesh>> const& initialMesh
    ) -> Scene;
    static auto diagonalWaveScene(
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        std::optional<AssetPtr<Mesh>> const& initialMesh
    ) -> Scene;

    void handleInput(TickTiming, InputSnapshot const&);
    void tick(TickTiming);

private:
    AABB m_shadowBounds{};
    std::vector<MeshInstanced> m_geometry;
};
// NOLINTEND(misc-non-private-member-variables-in-classes)
} // namespace syzygy