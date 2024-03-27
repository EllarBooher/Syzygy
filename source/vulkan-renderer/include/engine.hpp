#pragma once

#include "engine_types.h"

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
public:

    Engine();

    void run();

    static Engine* getLoadedEngine() { return m_loadedEngine; }

private:

    void init();
    
    void initWindow();

    void initVulkan();

    void mainLoop();

    void draw();
    void recordDrawBackground(VkCommandBuffer cmd, VkImage image);

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

    void cleanupSwapchain();

    void initCommands();
    void initSyncStructures();

    VkInstance m_instance{ VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };
    VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
    VkDevice m_device{ VK_NULL_HANDLE };
    VkSurfaceKHR m_surface{ VK_NULL_HANDLE };

    VkQueue m_graphicsQueue{ VK_NULL_HANDLE };
    uint32_t m_graphicsQueueFamily{ 0 };

    DeletionQueue m_engineDeletionQueue{};

    VmaAllocator m_allocator{ VK_NULL_HANDLE };

    // Swapchain Resources

    VkSwapchainKHR m_swapchain{ VK_NULL_HANDLE };
    VkFormat m_swapchainImageFormat{ VK_FORMAT_UNDEFINED };

    std::vector<VkImage> m_swapchainImages{};
    std::vector<VkImageView> m_swapchainImageViews{};
    VkExtent3D m_swapchainExtent{};

    // Draw Resources

    AllocatedImage m_drawImage{};

    std::array<FrameData, FRAME_OVERLAP> m_frames {};
    FrameData& getCurrentFrame() { return m_frames[m_frameNumber % m_frames.size()]; }

    // End Vulkan
};