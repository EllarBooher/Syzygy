#pragma once

#include "syzygy/core/integer.hpp"
#include "syzygy/core/scenetexture.hpp"
#include "syzygy/enginetypes.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/renderer/pipelines/debuglines.hpp"
#include "syzygy/renderer/pipelines/deferred.hpp"
#include "syzygy/vulkanusage.hpp"
#include <memory>
#include <optional>

class DescriptorAllocator;
namespace szg_renderer
{
struct Atmosphere;
struct Camera;
} // namespace szg_renderer
namespace scene
{
struct Scene;
}
namespace ui
{
struct DockingLayout;
}
struct PlatformWindow;

class Renderer
{
public:
    Renderer& operator=(Renderer&&) = delete;
    Renderer(Renderer const&) = delete;
    Renderer& operator=(Renderer const&) = delete;

    Renderer(Renderer&&) noexcept;
    ~Renderer();

private:
    Renderer() = default;
    void destroy();

public:
    static auto create(
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        scene::SceneTexture const& sceneTexture
    ) -> std::optional<Renderer>;

    // TODO: Remove this, but right now relies on internal state.
    void uiEngineControls(ui::DockingLayout const&);

    void recordDraw(
        VkCommandBuffer,
        scene::Scene const& scene,
        scene::SceneTexture& sceneTexture,
        std::optional<scene::SceneViewport> const& sceneViewport
    );

private:
    void recordDrawDebugLines(
        VkCommandBuffer cmd,
        uint32_t cameraIndex,
        scene::SceneTexture& sceneTexture,
        scene::SceneViewport const& sceneViewport,
        TStagedBuffer<szg_renderer::Camera> const& camerasBuffer
    );

    // Begin Vulkan

private:
    void initDrawTargets(VkDevice, VmaAllocator);

    void initWorld(VkDevice, VmaAllocator);
    void initDebug(VkDevice, VmaAllocator);
    void
    initDeferredShadingPipeline(VkDevice, VmaAllocator, DescriptorAllocator&);

    void initGenericComputePipelines(VkDevice, scene::SceneTexture const&);

    VkDevice m_device{VK_NULL_HANDLE};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    bool m_initialized{false};

    // Draw Resources

    // Instead of resizing all resources to be exactly the window size, we draw
    // into a limited scissor. This constant defines the max size, to inform
    // the creation of resources that can contain any requested draw extent
    static VkExtent2D constexpr MAX_DRAW_EXTENTS{4096, 4096};

    // Depth image used for graphics passes
    std::unique_ptr<szg_renderer::ImageView> m_sceneDepthTexture{};

    // Pipelines

    static uint32_t constexpr DEBUGLINES_CAPACITY{1000};
    DebugLines m_debugLines{};

    RenderingPipelines m_activeRenderingPipeline{RenderingPipelines::DEFERRED};
    std::unique_ptr<ComputeCollectionPipeline> m_genericComputePipeline{};
    std::unique_ptr<DeferredShadingPipeline> m_deferredShadingPipeline{};

private:
    // Scene

    static uint32_t constexpr CAMERA_CAPACITY{20};
    std::unique_ptr<TStagedBuffer<szg_renderer::Camera>> m_camerasBuffer{};

    static uint32_t constexpr ATMOSPHERE_CAPACITY{1};
    std::unique_ptr<TStagedBuffer<szg_renderer::Atmosphere>>
        m_atmospheresBuffer{};

    // End Vulkan
};
