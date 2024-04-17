#pragma once

#include "engine_types.h"
#include "shaders.hpp"
#include "pipelines.hpp"
#include "descriptors.hpp"
#include "assets.hpp"
#include "buffers.hpp"
#include "engineparams.hpp"

struct GLFWwindow;

struct FrameData {
    VkCommandPool commandPool{ VK_NULL_HANDLE };
    VkCommandBuffer mainCommandBuffer{ VK_NULL_HANDLE };
    
    /** The semaphore that the swapchain signals when its image is ready to be written to. */
    VkSemaphore swapchainSemaphore{ VK_NULL_HANDLE };

    /** The semaphore that the swapchain waits on before presenting. */
    VkSemaphore renderSemaphore{ VK_NULL_HANDLE };
    
    /** The fence that the CPU waits on to ensure the frame is not in use. */
    VkFence renderFence{ VK_NULL_HANDLE };

    DeletionQueue deletionQueue{};
};

/** The number of frames in flight at once. */
constexpr uint32_t FRAME_OVERLAP = 2;

class Engine {
    Engine();

public:
    void run();

    static std::unique_ptr<Engine> loadEngine();

private:
    void init();
    
    void initWindow();

    void initVulkan();

    void mainLoop();

    void tickWorld(double totalTime, double deltaTimeSeconds);
    void draw();
    void recordDrawBackground(VkCommandBuffer cmd, VkImage image);
    void recordDrawImgui(VkCommandBuffer cmd, VkImageView view);

    void cleanup();

    bool m_initialized{ false };
    inline static Engine* m_loadedEngine{ nullptr };

    RingBuffer m_fpsValues{};

    uint32_t m_frameNumber{ 0 };
    bool m_bRender{ true };
    VkExtent2D m_windowExtent{ 1700, 900 };

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

    void initPipelines();
    void initBackgroundPipelines(std::span<std::string const> shaders);

    void initDefaultMeshData();
    void initWorld();
    void initInstancedPipeline();
    void initBackgroundPipeline();

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
    VkExtent3D m_swapchainExtent{};

    float m_dpiScale{ 1.0f };

    bool m_resizeRequested{ false };
    void resizeSwapchain();

    // Draw Resources

    VkDescriptorPool m_imguiDescriptorPool{ VK_NULL_HANDLE };

    /** This image is used as the render target, then copied onto the swapchain. */
    AllocatedImage m_drawImage{};
    float getAspectRatio() const {
        auto const width{ static_cast<float>(m_drawImage.imageExtent.width) };
        auto const height{ static_cast<float>(m_drawImage.imageExtent.height) };

        return width / height;
    }

    AllocatedImage m_depthImage{};

    std::array<FrameData, FRAME_OVERLAP> m_frames {};
    FrameData& getCurrentFrame() { return m_frames[m_frameNumber % m_frames.size()]; }

    // Immediate submit structures

    VkFence m_immFence{ VK_NULL_HANDLE };
    VkCommandBuffer m_immCommandBuffer{ VK_NULL_HANDLE };
    VkCommandPool m_immCommandPool{ VK_NULL_HANDLE };

    /** Immediately opens and submits a command buffer. Use for one-off things outside the render loop or when hangs are okay. */
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

    // Descriptor

    DescriptorAllocator m_globalDescriptorAllocator{};

    VkDescriptorSetLayout m_drawImageDescriptorLayout{ VK_NULL_HANDLE };
    VkDescriptorSet m_drawImageDescriptors{ VK_NULL_HANDLE };

    // Pipelines

    std::vector<ComputeShaderWrapper> m_computeShaders{};
    uint32_t m_computeShaderIndex{ 0 };

    std::unique_ptr<InstancedMeshGraphicsPipeline> m_instancePipeline{};
    std::unique_ptr<TStagedBuffer<glm::mat4x4>> m_meshInstances{};
    std::vector<glm::mat4x4> m_transformOriginals{};

    std::unique_ptr<TStagedBuffer<glm::mat4x4>> m_worldStaticTransforms{};

    std::unique_ptr<BackgroundComputePipeline> m_backgroundPipeline{};

public:
    std::unique_ptr<GPUMeshBuffers> uploadMeshToGPU(std::span<uint32_t const> indices, std::span<Vertex const> vertices);

private:
    // Meshes

    std::vector<std::shared_ptr<MeshAsset>> m_testMeshes{};

    // Scene

    float m_targetFPS{ 160.0 };
    uint32_t m_cameraIndex{ 0 };
    static CameraParameters const m_defaultCameraParameters;
    CameraParameters m_cameraParameters{ m_defaultCameraParameters };

    uint32_t m_atmosphereIndex{ 0 };
    AtmosphereParameters m_atmosphereParameters{};

    std::unique_ptr<TStagedBuffer<GPUTypes::Camera>> m_camerasBuffer{};
    std::unique_ptr<TStagedBuffer<GPUTypes::Atmosphere>> m_atmospheresBuffer{};

    // End Vulkan
};
