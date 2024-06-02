#pragma once

#include "enginetypes.hpp"
#include "shaders.hpp"
#include "pipelines.hpp"
#include "descriptors.hpp"
#include "assets.hpp"
#include "buffers.hpp"
#include "engineparams.hpp"
#include "shadowpass.hpp"
#include "debuglines.hpp"
#include "deferred/deferred.hpp"
#include "imgui.h"

struct GLFWwindow;

struct FrameData 
{
    VkCommandPool commandPool{ VK_NULL_HANDLE };
    VkCommandBuffer mainCommandBuffer{ VK_NULL_HANDLE };
    
    // The semaphore that the swapchain signals when its 
    // image is ready to be written to.
    VkSemaphore swapchainSemaphore{ VK_NULL_HANDLE };

    // The semaphore that the swapchain waits on before presenting.
    VkSemaphore renderSemaphore{ VK_NULL_HANDLE };
    
    // The fence that the CPU waits on to ensure the frame is not in use.
    VkFence renderFence{ VK_NULL_HANDLE };

    DeletionQueue deletionQueue{};
};

/** The number of frames in flight at once. */
constexpr uint32_t FRAME_OVERLAP = 2;

class Engine 
{
    Engine();

public:
    void run();

    static std::unique_ptr<Engine> loadEngine();

private:
    void init();

    void initWindow();

    void initVulkan();

    void tickWorld(
        double totalTime
        , double deltaTimeSeconds
    );
    
    bool renderUI(VkDevice device);
    void draw();
    
    void recordDrawImgui(
        VkCommandBuffer cmd
        , VkImageView view
    );
    void recordDrawDebugLines(
        VkCommandBuffer cmd
        , uint32_t cameraIndex
        , TStagedBuffer<GPUTypes::Camera> const& camerasBuffer
    );

    void mainLoop();

    void cleanup();

    bool m_initialized{ false };
    inline static Engine* m_loadedEngine{ nullptr };

    RingBuffer m_fpsValues{};

    uint32_t m_frameNumber{ 0 };
    bool m_bRender{ true };
    VkExtent2D m_windowExtent{ 1920, 1080 };

    GLFWwindow* m_window{ nullptr };

    // Begin Vulkan

private:
    void initInstanceSurfaceDevices();
    void initAllocator();
    void initSwapchain();
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

    void initImgui();

    VkInstance m_instance{ VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };
    VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
    VkDevice m_device{ VK_NULL_HANDLE };
    VkSurfaceKHR m_surface{ VK_NULL_HANDLE };

    VkQueue m_graphicsQueue{ VK_NULL_HANDLE };
    uint32_t m_graphicsQueueFamily{ 0 };

    VmaAllocator m_allocator{ VK_NULL_HANDLE };

    // Swapchain Resources

    VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };
    VkFormat m_swapchainImageFormat{ VK_FORMAT_UNDEFINED };

    std::vector<VkImage> m_swapchainImages{};
    std::vector<VkImageView> m_swapchainImageViews{};
    VkExtent2D m_swapchainExtent{};

    ImGuiStyle m_imguiStyleDefault{};
    static UIPreferences constexpr m_uiPreferencesDefault{
        .dpiScale{ 1.0f }
    };
    UIPreferences m_uiPreferences{ m_uiPreferencesDefault };
    
    bool m_uiReloadRequested{ false };

    bool m_resizeRequested{ false };
    void resizeSwapchain();

    // Draw Resources

    // Instead of resizing all resources to be exactly the window size, we draw 
    // into a limited scissor. This constant defines the max size, to inform 
    // the creation of resources that can contain any requested draw extent
    static VkExtent2D constexpr MAX_DRAW_EXTENTS{ 4096, 4096 };

    VkSampler m_imguiSceneTextureSampler{ VK_NULL_HANDLE };
    VkDescriptorSet m_imguiSceneTextureDescriptor{ VK_NULL_HANDLE };
    VkDescriptorPool m_imguiDescriptorPool{ VK_NULL_HANDLE };

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

    std::array<FrameData, FRAME_OVERLAP> m_frames{};
    FrameData& getCurrentFrame() { 
        return m_frames[m_frameNumber % m_frames.size()]; 
    }

    // Immediate submit structures

    VkFence m_immFence{ VK_NULL_HANDLE };
    VkCommandBuffer m_immCommandBuffer{ VK_NULL_HANDLE };
    VkCommandPool m_immCommandPool{ VK_NULL_HANDLE };

    // Immediately opens and submits a command buffer. Use for one-off things 
    // outside the render loop or when hangs are okay.
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

    // Descriptor

    DescriptorAllocator m_globalDescriptorAllocator{};

    VkDescriptorSetLayout m_sceneTextureDescriptorLayout{ VK_NULL_HANDLE };
    VkDescriptorSet m_sceneTextureDescriptors{ VK_NULL_HANDLE };

    // Pipelines

    DebugLines m_debugLines{};

    RenderingPipelines m_activeRenderingPipeline{ 
        RenderingPipelines::DEFERRED 
    };
    std::unique_ptr<ComputeCollectionPipeline> m_genericComputePipeline{};
    std::unique_ptr<DeferredShadingPipeline> m_deferredShadingPipeline{};

public:
    std::unique_ptr<GPUMeshBuffers> uploadMeshToGPU(
        std::span<uint32_t const> indices
        , std::span<Vertex const> vertices
    );

private:
    // Meshes

    std::vector<std::shared_ptr<MeshAsset>> m_testMeshes{};

    // Scene

    float m_targetFPS{ 160.0 };
    uint32_t m_cameraIndexMain{ 0 };
    size_t m_testMeshUsed{ 0 };

    bool m_showSpotlights{ true };
    bool m_renderMeshInstances{ true };

    MeshInstances m_meshInstances{};

    // These scene bounds help inform shadow map generation
    // TODO: compute this from the scene
    SceneBounds m_sceneBounds{
        .center{ 0.0, -4.0, 0.0 },
        .extent{ 40.0, 5.0, 40.0 },
    };

    bool m_useOrthographicProjection{ false };
    static CameraParameters const m_defaultCameraParameters;
    CameraParameters m_cameraParameters{ m_defaultCameraParameters };

    uint32_t m_atmosphereIndex{ 0 };
    static AtmosphereParameters const m_defaultAtmosphereParameters;
    AtmosphereParameters m_atmosphereParameters{ 
        m_defaultAtmosphereParameters 
    };

    std::unique_ptr<TStagedBuffer<GPUTypes::Camera>> m_camerasBuffer{};
    std::unique_ptr<TStagedBuffer<GPUTypes::Atmosphere>> m_atmospheresBuffer{};

    // End Vulkan
};
