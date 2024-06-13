#pragma once

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

struct FrameData
{
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer mainCommandBuffer{VK_NULL_HANDLE};

    // The semaphore that the swapchain signals when its
    // image is ready to be written to.
    VkSemaphore swapchainSemaphore{VK_NULL_HANDLE};

    // The semaphore that the swapchain waits on before presenting.
    VkSemaphore renderSemaphore{VK_NULL_HANDLE};

    // The fence that the CPU waits on to ensure the frame is not in use.
    VkFence renderFence{VK_NULL_HANDLE};

    DeletionQueue deletionQueue{};
};

size_t constexpr FRAMES_IN_FLIGHT = 2;

class Engine
{
private:
    Engine(PlatformWindow const& window);

public:
    static Engine* loadEngine(PlatformWindow const& window);

    void mainLoop(
        double elapsedTimeSeconds,
        double deltaTimeSeconds,
        bool shouldRender,
        glm::u16vec2 windowExtent
    );

    void cleanup();

private:
    void init(PlatformWindow const& window);

    void initVulkan(PlatformWindow const& window);

    struct TickTiming
    {
        double timeElapsed;
        double deltaTimeSeconds;
    };

    void tickWorld(TickTiming timing);

    bool renderUI(VkDevice device);
    void draw();

    void recordDrawImgui(VkCommandBuffer cmd, VkImageView view);
    void recordDrawDebugLines(
        VkCommandBuffer cmd,
        uint32_t cameraIndex,
        TStagedBuffer<gputypes::Camera> const& camerasBuffer
    );


    bool m_initialized{false};
    inline static Engine* m_loadedEngine{nullptr};

    RingBuffer m_fpsValues{};

    uint32_t m_frameNumber{0};

    bool m_bRender{true};

    // Begin Vulkan

private:
    void initInstanceSurfaceDevices(GLFWwindow* const window);
    void initAllocator();
    void initSwapchain(glm::u16vec2 extent);
    void initDrawTargets();

    void cleanupSwapchain();
    void cleanupDrawTargets();

    void initCommands();
    void initSyncStructures();
    void initDescriptors();

    void updateDescriptors();

    void initDefaultMeshData();
    void initWorld();
    void initDebug();
    void initDeferredShadingPipeline();

    void initGenericComputePipelines();

    void initImgui(GLFWwindow* const window);

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};

    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    uint32_t m_graphicsQueueFamily{0};

    VmaAllocator m_allocator{VK_NULL_HANDLE};

    // Swapchain Resources

    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkFormat m_swapchainImageFormat{VK_FORMAT_UNDEFINED};

    std::vector<VkImage> m_swapchainImages{};
    std::vector<VkImageView> m_swapchainImageViews{};
    VkExtent2D m_swapchainExtent{};

    ImGuiStyle m_imguiStyleDefault{};

    static VkExtent2D constexpr RESOLUTION_DEFAULT{1920, 1080};

    static UIPreferences constexpr m_uiPreferencesDefault{
        .dpiScale = 1.0f,
    };
    UIPreferences m_uiPreferences{m_uiPreferencesDefault};

    bool m_uiReloadRequested{false};

    bool m_resizeRequested{false};
    void resizeSwapchain(glm::u16vec2 extent);

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
    AllocatedImage m_sceneColorTexture{};
    // Depth image used for graphics passes
    AllocatedImage m_sceneDepthTexture{};

    // The rectangle drawn into, usually the window/swapchain/UI viewport
    // extents are all the same
    VkRect2D m_drawRect{};

    // The final image output, blitted to the swapchain
    AllocatedImage m_drawImage{};

    std::array<FrameData, FRAMES_IN_FLIGHT> m_frames{};
    FrameData& getCurrentFrame()
    {
        return m_frames[m_frameNumber % m_frames.size()];
    }

    // Immediate submit structures

    VkFence m_immFence{VK_NULL_HANDLE};
    VkCommandBuffer m_immCommandBuffer{VK_NULL_HANDLE};
    VkCommandPool m_immCommandPool{VK_NULL_HANDLE};

    // Immediately opens and submits a command buffer. Use for one-off things
    // outside the render loop or when hangs are okay.
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

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
        std::span<uint32_t const> indices, std::span<Vertex const> vertices
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
