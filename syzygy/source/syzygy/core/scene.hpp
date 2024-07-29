#pragma once

#include "syzygy/buffers.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/gputypes.hpp"
#include "syzygy/vulkanusage.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <vector>

struct MeshAsset;
struct MeshAssetLibrary;
struct TickTiming;

namespace scene
{
struct SceneBounds
{
    glm::vec3 center{};
    glm::vec3 extent{};
};

struct AtmosphereBaked
{
    gputypes::Atmosphere atmosphere{};
    std::optional<gputypes::LightDirectional> sunlight{};
    std::optional<gputypes::LightDirectional> moonlight{};
};

struct Atmosphere
{
    struct SunAnimation
    {
        bool animateSun{false};
        float animationSpeed{0.2f};
        bool skipNight{false};
    };

    SunAnimation animation{};

    glm::vec3 sunEulerAngles{0.0, 0.0, 0.0};

    float earthRadiusMeters{0.0};
    float atmosphereRadiusMeters{0.0};

    glm::vec3 groundColor{1.0};

    glm::vec3 scatteringCoefficientRayleigh{1.0};
    float altitudeDecayRayleigh{1.0};

    glm::vec3 scatteringCoefficientMie{1.0};
    float altitudeDecayMie{1.0};

    auto directionToSun() const -> glm::vec3;

    auto toDeviceEquivalent() const -> gputypes::Atmosphere;
    auto baked(SceneBounds) const -> AtmosphereBaked;
};

struct Camera
{
    bool orthographic{false};
    glm::vec3 cameraPosition{0.0F, 0.0F, 0.0F};
    glm::vec3 eulerAngles{0.0F, 0.0F, 0.0F};
    float fovDegrees{90.0F};
    float near{0.0F};
    float far{1.0F};

    // In all of these methods, aspect ratio is passed as a parameter since it
    // may depend on the drawn surface.

    // Rotates (but does not translate) from camera to world space
    auto rotation() const -> glm::mat4x4;
    // The matrix that transforms from camera to world space
    auto transform() const -> glm::mat4x4;
    // The inverse of transform, transforms from world to camera space
    auto view() const -> glm::mat4x4;
    // Projects from camera space to clip space
    auto projection(float aspectRatio) const -> glm::mat4x4;

    // Gives the projection * view matrix that transforms from world to
    // clip space.
    auto toProjView(float aspectRatio) const -> glm::mat4x4;

    auto toDeviceEquivalent(float aspectRatio) const -> gputypes::Camera;
};

struct MeshInstanced
{
    bool render{false};
    std::string name{};
    std::shared_ptr<MeshAsset> mesh{};

    std::vector<glm::mat4x4> originals{};

    std::unique_ptr<TStagedBuffer<glm::mat4x4>> models{};
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> modelInverseTransposes{};
};

struct Scene
{
    static Atmosphere const DEFAULT_ATMOSPHERE_EARTH;
    static Camera const DEFAULT_CAMERA;

    Atmosphere atmosphere{DEFAULT_ATMOSPHERE_EARTH};
    Camera camera{DEFAULT_CAMERA};

    bool spotlightsRender{false};
    std::vector<gputypes::LightSpot> spotlights{};

    std::optional<size_t> cubesIndex{};
    std::vector<MeshInstanced> geometry;

    // TODO: compute this on demand instead of making it a tweakable parameter
    // This is used to compute the necessary dimensions of various resource e.g.
    // shadowmaps
    SceneBounds bounds{};

    static auto
    defaultScene(VkDevice, VmaAllocator, MeshAssetLibrary const& meshes)
        -> Scene;
    void tick(TickTiming);
};
} // namespace scene