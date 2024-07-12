#pragma once

#include <functional>

#include "assets.hpp"
#include "buffers.hpp"
#include "debuglines.hpp"
#include "deferred/deferred.hpp"
#include "descriptors.hpp"
#include "editor/window.hpp"
#include "engineparams.hpp"
#include "enginetypes.hpp"
#include "imgui.h"
#include "pipelines.hpp"
#include "shaders.hpp"
#include "shadowpass.hpp"

struct GLFWwindow;

struct TickTiming
{
    double timeElapsedSeconds;
    double deltaTimeSeconds;
};

class Engine
{
private:
    Engine(
        PlatformWindow const& window,
        VkInstance const instance,
        VkPhysicalDevice const physicalDevice,
        VkDevice const device,
        VmaAllocator const allocator,
        VkQueue const generalQueue,
        uint32_t const generalQueueFamilyIndex
    );

public:
    static Engine* loadEngine(
        PlatformWindow const& window,
        VkInstance const instance,
        VkPhysicalDevice const physicalDevice,
        VkDevice const device,
        VmaAllocator const allocator,
        VkQueue const generalQueue,
        uint32_t const generalQueueFamilyIndex
    );

    void tickWorld(TickTiming);

    struct UIResults
    {
        bool reloadRequested;
    };
    auto renderUI(
        UIPreferences& currentPreferences,
        UIPreferences const& defaultPreferences
    ) -> UIResults;

    struct DrawResults
    {
        AllocatedImage& renderTarget;
        VkRect2D renderArea;
    };
    auto recordDraw(VkCommandBuffer) -> DrawResults;

    void cleanup(VkDevice, VmaAllocator);

private:
    void init(
        PlatformWindow const& window,
        VkInstance,
        VkPhysicalDevice,
        VkDevice,
        VmaAllocator,
        VkQueue generalQueue,
        uint32_t const generalQueueFamilyIndex
    );

private:
    static auto recordDrawImgui(VkCommandBuffer cmd, VkImageView view)
        -> VkRect2D;
    void recordDrawDebugLines(
        VkCommandBuffer cmd,
        uint32_t cameraIndex,
        TStagedBuffer<gputypes::Camera> const& camerasBuffer
    );

    bool m_initialized{false};
    inline static Engine* m_loadedEngine{nullptr};

    RingBuffer m_fpsValues{};

    // Begin Vulkan

private:
    void initDrawTargets(VkDevice, VmaAllocator);

    // queueFamilyIndex must support all operations: graphics, compute, present,
    // and transfer.
    void initCommands(VkDevice, uint32_t queueFamilyIndex);
    void initSyncStructures(VkDevice);
    void initDescriptors(VkDevice);

    void updateDescriptors(VkDevice);

    void initDefaultMeshData(VkDevice, VmaAllocator, VkQueue transferQueue);
    void initWorld(VkDevice, VmaAllocator, VkQueue transferQueue);
    void initDebug(VkDevice, VmaAllocator);
    void initDeferredShadingPipeline(VkDevice, VmaAllocator);

    void initGenericComputePipelines(VkDevice);

    void initImgui(
        VkInstance,
        VkPhysicalDevice,
        VkDevice,
        uint32_t graphicsQueueFamily,
        VkQueue graphicsQueue,
        GLFWwindow* const window
    );

    // Draw Resources

    // Instead of resizing all resources to be exactly the window size, we draw
    // into a limited scissor. This constant defines the max size, to inform
    // the creation of resources that can contain any requested draw extent
    static VkExtent2D constexpr MAX_DRAW_EXTENTS{4096, 4096};

    VkSampler m_imguiSceneTextureSampler{VK_NULL_HANDLE};
    VkDescriptorSet m_imguiSceneTextureDescriptor{VK_NULL_HANDLE};
    VkDescriptorPool m_imguiDescriptorPool{VK_NULL_HANDLE};

    VkRect2D m_sceneRect{};

    // Rendered into by most render passes. Used as an image by UI rendering,
    // to render properly as a window.
    std::unique_ptr<AllocatedImage> m_sceneColorTexture{};
    // Depth image used for graphics passes
    std::unique_ptr<AllocatedImage> m_sceneDepthTexture{};

    // The final image output, blitted to the swapchain
    std::unique_ptr<AllocatedImage> m_drawImage{};

    // Immediate submit structures

    VkFence m_immFence{VK_NULL_HANDLE};
    VkCommandBuffer m_immCommandBuffer{VK_NULL_HANDLE};
    VkCommandPool m_immCommandPool{VK_NULL_HANDLE};

    // Immediately opens and submits a command buffer. Use for one-off things
    // outside the render loop or when hangs are okay.
    void immediateSubmit(
        VkDevice, VkQueue, std::function<void(VkCommandBuffer cmd)>&& function
    );

    // Descriptor

    static uint32_t constexpr DESCRIPTOR_SET_CAPACITY_DEFAULT{10};

    DescriptorAllocator m_globalDescriptorAllocator{};

    VkDescriptorSetLayout m_sceneTextureDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_sceneTextureDescriptors{VK_NULL_HANDLE};

    // Pipelines

    static uint32_t constexpr DEBUGLINES_CAPACITY{1000};
    DebugLines m_debugLines{};

    RenderingPipelines m_activeRenderingPipeline{RenderingPipelines::DEFERRED};
    std::unique_ptr<ComputeCollectionPipeline> m_genericComputePipeline{};
    std::unique_ptr<DeferredShadingPipeline> m_deferredShadingPipeline{};

public:
    std::unique_ptr<GPUMeshBuffers> uploadMeshToGPU(
        VkDevice,
        VmaAllocator,
        VkQueue transferQueue,
        std::span<uint32_t const> indices,
        std::span<Vertex const> vertices
    );

    float targetFPS() const { return m_targetFPS; }

private:
    // Meshes

    std::vector<std::shared_ptr<MeshAsset>> m_testMeshes{};

    // Scene

    float m_targetFPS{160.0};
    uint32_t m_cameraIndexMain{0};
    size_t m_testMeshUsed{0};

    bool m_showSpotlights{true};
    bool m_renderMeshInstances{true};

    MeshInstances m_meshInstances{};

    // These scene bounds help inform shadow map generation
    // TODO: compute this from the scene
    static SceneBounds constexpr DEFAULT_SCENE_BOUNDS{
        .center = glm::vec3{0.0, -4.0, 0.0},
        .extent = glm::vec3{40.0, 5.0, 40.0},
    };
    SceneBounds m_sceneBounds{DEFAULT_SCENE_BOUNDS};

    bool m_useOrthographicProjection{false};
    static CameraParameters const m_defaultCameraParameters;
    CameraParameters m_cameraParameters{m_defaultCameraParameters};

    uint32_t m_atmosphereIndex{0};
    static AtmosphereParameters const m_defaultAtmosphereParameters;
    AtmosphereParameters m_atmosphereParameters{m_defaultAtmosphereParameters};

    static uint32_t constexpr CAMERA_CAPACITY{20};
    std::unique_ptr<TStagedBuffer<gputypes::Camera>> m_camerasBuffer{};

    static uint32_t constexpr ATMOSPHERE_CAPACITY{1};
    std::unique_ptr<TStagedBuffer<gputypes::Atmosphere>> m_atmospheresBuffer{};

    // End Vulkan
};
