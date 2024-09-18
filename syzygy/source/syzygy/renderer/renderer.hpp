#pragma once

#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/buffers.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/renderer/pipelines/debuglines.hpp"
#include "syzygy/renderer/pipelines/deferred.hpp"
#include <memory>
#include <optional>

namespace syzygy
{
struct DescriptorAllocator;
struct AtmospherePacked;
struct CameraPacked;
struct Scene;
struct DockingLayout;
struct SceneTexture;
} // namespace syzygy

namespace syzygy
{
struct Renderer
{
public:
    auto operator=(Renderer&&) -> Renderer& = delete;
    Renderer(Renderer const&) = delete;
    auto operator=(Renderer const&) -> Renderer& = delete;

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
        VkDescriptorSetLayout computeImageDescriptorLayout
    ) -> std::optional<Renderer>;

    // TODO: Remove this, but right now relies on internal state.
    void uiEngineControls(syzygy::DockingLayout const&);

    void recordDraw(
        VkCommandBuffer,
        syzygy::Scene const& scene,
        syzygy::SceneTexture& sceneTexture,
        VkRect2D sceneSubregion
    );

private:
    void recordDrawDebugLines(
        VkCommandBuffer cmd,
        uint32_t cameraIndex,
        syzygy::SceneTexture& sceneTexture,
        VkRect2D sceneSubregion,
        TStagedBuffer<syzygy::CameraPacked> const& camerasBuffer
    );

    // Begin Vulkan

    void initDrawTargets(VkDevice, VmaAllocator);

    void initWorld(VkDevice, VmaAllocator);
    void initDebug(VkDevice, VmaAllocator);
    void
    initDeferredShadingPipeline(VkDevice, VmaAllocator, DescriptorAllocator&);

    // TODO: This should be changed. Passing the layout is wrong since the
    // pipeline/shaders know what layout they want. Compatibility with the
    // passed set should be checked at rendering time.

    // imageDescriptorLayout is the layout of the set that will be bound at
    // rendering time, containing the image that is drawn to
    void initGenericComputePipelines(
        VkDevice, VkDescriptorSetLayout imageDescriptorLayout
    );

    VkDevice m_device{VK_NULL_HANDLE};
    VmaAllocator m_allocator{VK_NULL_HANDLE};
    bool m_initialized{false};

    // Draw Resources

    // Instead of resizing all resources to be exactly the window size, we draw
    // into a limited scissor. This constant defines the max size, to inform
    // the creation of resources that can contain any requested draw extent
    static VkExtent2D constexpr MAX_DRAW_EXTENTS{4096, 4096};

    // Depth image used for graphics passes
    std::unique_ptr<syzygy::ImageView> m_sceneDepthTexture{};

    // Pipelines

    static uint32_t constexpr DEBUGLINES_CAPACITY{1000};
    DebugLines m_debugLines{};

    RenderingPipelines m_activeRenderingPipeline{RenderingPipelines::DEFERRED};
    std::unique_ptr<ComputeCollectionPipeline> m_genericComputePipeline{};
    std::unique_ptr<DeferredShadingPipeline> m_deferredShadingPipeline{};

    // Scene

    static uint32_t constexpr CAMERA_CAPACITY{20};
    std::unique_ptr<TStagedBuffer<syzygy::CameraPacked>> m_camerasBuffer{};

    static uint32_t constexpr ATMOSPHERE_CAPACITY{1};
    std::unique_ptr<TStagedBuffer<syzygy::AtmospherePacked>>
        m_atmospheresBuffer{};

    // End Vulkan
};
} // namespace syzygy