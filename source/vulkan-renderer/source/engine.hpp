#pragma once

#include "engine_types.h"
#include "shaders.hpp"
#include "pipelines.hpp"
#include "descriptors.hpp"

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

    void draw();
    void recordDrawBackground(VkCommandBuffer cmd, VkImage image);
    void recordDrawGeometry(VkCommandBuffer cmd);
    void recordDrawImgui(VkCommandBuffer cmd, VkImageView view);

    void cleanup();

    bool m_initialized{ false };
    inline static Engine* m_loadedEngine{ nullptr };

    uint32_t m_frameNumber{ 0 };
    bool m_bRender{ true };
    VkExtent2D m_windowExtent{ 1700, 900 };

    GLFWwindow* m_window{ nullptr };

    // Begin Vulkan

private:
    void initInstanceSurfaceDevices();
    void initAllocator();
    void initSwapchain();
    void initDrawTarget();

    void cleanupSwapchain();

    void initCommands();
    void initSyncStructures();
    void initDescriptors();

    void updateDescriptors();

    void initPipelines();
    void initBackgroundPipelines(std::span<std::string const> shaders);
    void initTrianglePipeline();
    
    void initMeshPipeline();
    void initDefaultMeshData();

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

    float m_dpiScale{ 2.0f };

    bool m_resizeRequested{ false };
    void resizeSwapchain();

    // Draw Resources

    VkDescriptorPool m_imguiDescriptorPool{ VK_NULL_HANDLE };

    /** This image is used as the render target, then copied onto the swapchain. */
    AllocatedImage m_drawImage{};

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

    GraphicsPipelineWrapper m_trianglePipeline{};
    GraphicsPipelineWrapper m_meshPipeline{};

    // Buffers

    AllocatedBuffer createBuffer(size_t allocationSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    GPUMeshBuffers uploadedMeshToGPU(std::span<uint32_t const> indices, std::span<Vertex const> vertices);

    // Meshes

    GPUMeshBuffers m_rectangleMesh{};

    // End Vulkan

    // Begin UI

    /**
        @param backingData The data to read/write to for the given structure. It should span the entire padded size,
        even parts members do not overlap with.
    */
    void imguiPushStructureControl(
        ShaderReflectionData::Structure const& structure, 
        std::span<uint8_t> backingData    
    );

    /** Creates a imgui window that controls a shader. Will break when not in the right context in a draw loop. */
    void imguiPushShaderControl(ShaderWrapper& shader);
    void imguiPushGraphicsPipelineControl(GraphicsPipelineWrapper& pipeline);
};