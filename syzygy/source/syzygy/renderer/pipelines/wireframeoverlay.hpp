#pragma once

#include "syzygy/geometry/geometryhelpers.hpp"
#include "syzygy/geometry/geometrytypes.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/gputypes.hpp" // IWYU pragma: keep
#include "syzygy/renderer/pipelines.hpp"
#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>

namespace syzygy
{
struct AABB;
struct Transform;
struct TickTiming;
struct SceneTexture;
} // namespace syzygy

namespace syzygy
{
enum class WireframeLayers
{
    Gizmo,
    Debug
};

struct WireframeRenderInfo
{
    uint64_t indicesOnGPU;
    uint64_t verticesOnGPU;
};

struct WireframeArguments
{
    WireframeLayers layer{WireframeLayers::Debug};

    glm::vec3 colorRGB{0.0F, 1.0F, 0.0F};

    // 0 or less seconds indicates existance for a single frame.
    float lifetimeSeconds{0.0F};
};

// Provides a mechanism to draw wireframe geometry over the scene using world
// coordinates.
struct WireframeOverlay
{
public:
    WireframeOverlay(WireframeOverlay const&) = delete;
    auto operator=(WireframeOverlay const&) noexcept
        -> WireframeOverlay& = delete;

    auto operator=(WireframeOverlay&&) -> WireframeOverlay&;
    WireframeOverlay(WireframeOverlay&&) noexcept;
    ~WireframeOverlay();

private:
    WireframeOverlay() = default;

public:
    static auto create(VkDevice, VmaAllocator)
        -> std::unique_ptr<WireframeOverlay>;

    // TODO: Some of these overloads/signatures are messy or unnecessary. The
    // most useful methods should be kept and clarified.

    void push(glm::vec3 start, glm::vec3 end, WireframeArguments args = {});
    void push(
        glm::vec3 localStart,
        glm::vec3 localEnd,
        glm::mat4x4 worldMatrix,
        WireframeArguments args = {}
    );

    // Adds 4 line segmants defined by AB, BC, CD, DA.
    // Winding does not matter since these are added as separate line segments.
    void pushQuad(
        glm::vec3 a,
        glm::vec3 b,
        glm::vec3 c,
        glm::vec3 d,
        WireframeArguments args = {}
    );

    // Adds an arrow with a tip. The arrow is default pointing towards the world
    // forward.
    void
    pushArrow(Transform transform, float length, WireframeArguments args = {});
    // Overload that's simpler to specify, but the rotation of the arrow is
    // ambiguous.
    void pushArrow(Ray ray, WireframeArguments args = {});

    // Push a rectangle with possibly non-axis-aligned extents.
    void pushRectangleAxes(
        glm::vec3 center,
        glm::vec3 extentA,
        glm::vec3 extentB,
        WireframeArguments args = {}
    );

    // Push a rectangle, stretched along the (x,z) axes by extents.
    void pushRectangleOriented(
        glm::vec3 center,
        glm::quat orientation,
        glm::vec2 extents,
        WireframeArguments args = {}
    );

    // Push a rectangular prism, stretched along the (x,y,z) axes by extents.
    void pushBox(
        glm::vec3 center,
        glm::quat orientation,
        glm::vec3 extents,
        WireframeArguments args = {}
    );
    void pushBox(glm::mat4x4, AABB, WireframeArguments args = {});
    void pushBox(Transform, AABB, WireframeArguments args = {});

    void clear();

    // Should be called early each frame, before pushing any new geometry.
    void tick(TickTiming const&);

    [[nodiscard]] auto getLayerEnabled(WireframeLayers) const -> bool;
    void setLayerEnabled(WireframeLayers, bool);

    [[nodiscard]] auto lastFrameDrawResults() -> DrawResultsGraphics;

    [[nodiscard]] auto renderInfo() const -> WireframeRenderInfo;

    void recordDraw(
        VkCommandBuffer cmd,
        VkRect2D sceneSubregion,
        SceneTexture& sceneTexture,
        TStagedBuffer<CameraPacked> const& camerasBuffer,
        uint32_t cameraIndex
    );

private:
    struct WireframeSegment
    {
        VertexPacked start{};
        VertexPacked end{};
        // Assumes monotonic time
        float endTimeSeconds{};
    };

    VkDevice m_device{VK_NULL_HANDLE};

    std::unique_ptr<TStagedBuffer<VertexPacked>> m_vertices{};
    std::unique_ptr<TStagedBuffer<uint32_t>> m_indices{};
    std::unique_ptr<DebugLineGraphicsPipeline> m_pipeline{};

    std::unordered_map<WireframeLayers, bool> m_layerEnableFlags{};

    std::unordered_map<WireframeLayers, std::vector<WireframeSegment>>
        m_segments{};

    DrawResultsGraphics m_lastFrameDrawResults{};

    float m_currentFrameStartSeconds{};
};
} // namespace syzygy