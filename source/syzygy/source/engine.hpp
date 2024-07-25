#pragma once

#include "assets.hpp"
#include "buffers.hpp"
#include "core/scene.hpp"
#include "core/scenetexture.hpp"
#include "core/timing.hpp"
#include "debuglines.hpp"
#include "deferred/deferred.hpp"
#include "descriptors.hpp"
#include "editor/window.hpp"
#include "enginetypes.hpp"
#include "imgui.h"
#include "pipelines.hpp"
#include "shaders.hpp"
#include "shadowpass.hpp"
#include "ui/engineui.hpp"
#include <functional>

struct GLFWwindow;

class Engine
{
private:
    Engine(
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        scene::SceneTexture const& scene
    );

public:
    static Engine* loadEngine(
        PlatformWindow const&,
        VkInstance,
        VkPhysicalDevice,
        VkDevice,
        VmaAllocator,
        DescriptorAllocator&,
        scene::SceneTexture const&,
        VkQueue const generalQueue,
        uint32_t const generalQueueFamilyIndex
    );

    void tickWorld(TickTiming);

    // TODO: These methods are part of a rewrite to decouple UI from this engine
    // code, and should be removed eventually

    struct UIResults
    {
        HUDState hud;
        DockingLayout dockingLayout;
        bool reloadRequested;
    };

    // Draw HUD and possibly build docking layout
    static auto uiBegin(
        UIPreferences& currentPreferences,
        UIPreferences const& defaultPreferences
    ) -> UIResults;
    void uiRenderOldWindows(DockingLayout const&);
    static void uiEnd();

    // END TODO

    struct DrawResults
    {
        AllocatedImage& renderTarget;
        VkRect2D renderArea;
    };
    auto recordDraw(
        VkCommandBuffer,
        scene::Scene const& scene,
        scene::SceneTexture& sceneTexture,
        std::optional<scene::SceneViewport> const& sceneViewport
    ) -> DrawResults;

    void cleanup(VkDevice, VmaAllocator);

private:
    static auto recordDrawImgui(VkCommandBuffer cmd, VkImageView view)
        -> VkRect2D;
    void recordDrawDebugLines(
        VkCommandBuffer cmd,
        uint32_t cameraIndex,
        scene::SceneTexture& sceneTexture,
        scene::SceneViewport const& sceneViewport,
        TStagedBuffer<gputypes::Camera> const& camerasBuffer
    );

    bool m_initialized{false};
    inline static Engine* m_loadedEngine{nullptr};

    // Begin Vulkan

private:
    void initDrawTargets(VkDevice, VmaAllocator);

    void initWorld(VkDevice, VmaAllocator);
    void initDebug(VkDevice, VmaAllocator);
    void
    initDeferredShadingPipeline(VkDevice, VmaAllocator, DescriptorAllocator&);

    void initGenericComputePipelines(VkDevice, scene::SceneTexture const&);

    // Draw Resources

    // Instead of resizing all resources to be exactly the window size, we draw
    // into a limited scissor. This constant defines the max size, to inform
    // the creation of resources that can contain any requested draw extent
    static VkExtent2D constexpr MAX_DRAW_EXTENTS{4096, 4096};

    // Depth image used for graphics passes
    std::unique_ptr<AllocatedImage> m_sceneDepthTexture{};

    // The final image output, blitted to the swapchain
    std::unique_ptr<AllocatedImage> m_drawImage{};

    VkDescriptorPool m_imguiDescriptorPool{VK_NULL_HANDLE};

    // Pipelines

    static uint32_t constexpr DEBUGLINES_CAPACITY{1000};
    DebugLines m_debugLines{};

    RenderingPipelines m_activeRenderingPipeline{RenderingPipelines::DEFERRED};
    std::unique_ptr<ComputeCollectionPipeline> m_genericComputePipeline{};
    std::unique_ptr<DeferredShadingPipeline> m_deferredShadingPipeline{};

private:
    // Scene

    static uint32_t constexpr CAMERA_CAPACITY{20};
    std::unique_ptr<TStagedBuffer<gputypes::Camera>> m_camerasBuffer{};

    static uint32_t constexpr ATMOSPHERE_CAPACITY{1};
    std::unique_ptr<TStagedBuffer<gputypes::Atmosphere>> m_atmospheresBuffer{};

    // End Vulkan
};
