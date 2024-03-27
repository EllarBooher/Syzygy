#include "engine.hpp"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <iostream>

#include <chrono>
#include <thread>

#include "initializers.hpp"
#include "helpers.h"
#include "images.hpp"

Engine::Engine()
{
    init();
}

void Engine::run()
{
    mainLoop();
    cleanup();
}

void Engine::init()
{
    assert(m_loadedEngine == nullptr);
    m_loadedEngine = this;

    initWindow();
    initVulkan();

    m_initialized = true;

    Log("Engine Initialized.");
}

void Engine::initWindow()
{
    Log("Initializing Window...");

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    char const* const WINDOW_TITLE = "Renderer";
    m_window = glfwCreateWindow(
        m_windowExtent.width, 
        m_windowExtent.height, 
        WINDOW_TITLE, 
        nullptr, 
        nullptr
    );

    Log("Window Initialized.");
}

void Engine::initVulkan()
{
    Log("Initializing Vulkan...");

    volkInitialize();

    initInstanceSurfaceDevices();

    volkLoadDevice(m_device);

    initAllocator();

    initSwapchain();
    initCommands();
    initSyncStructures();

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    Log("Vulkan Initialized.");
}

void Engine::initInstanceSurfaceDevices()
{
    // create VkInstance and VkDebugUtilsMessengerEXT

    vkb::InstanceBuilder instanceBuilder;
    vkb::Result<vkb::Instance> const instanceBuildResult = instanceBuilder.set_app_name("Renderer")
        .request_validation_layers()
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();
    vkb::Instance const vkbInstance = UnwrapVkbResult(instanceBuildResult);

    volkLoadInstance(vkbInstance.instance);

    m_instance = vkbInstance.instance;
    m_debugMessenger = vkbInstance.debug_messenger;

    // create VkSurfaceKHR

    glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);

    // create VkPhysicalDevice and VkDevice

    VkPhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector{ vkbInstance };
    vkb::Result<vkb::PhysicalDevice> physicalDeviceBuildResult = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(m_surface)
        .select();
    vkb::PhysicalDevice vkbPhysicalDevice = UnwrapVkbResult(physicalDeviceBuildResult);

    vkb::DeviceBuilder deviceBuilder{ vkbPhysicalDevice };
    vkb::Result<vkb::Device> deviceBuildResult = deviceBuilder.build();
    vkb::Device vkbDevice = UnwrapVkbResult(deviceBuildResult);

    volkLoadDevice(vkbDevice.device);

    m_device = vkbDevice.device;
    m_physicalDevice = vkbDevice.physical_device;

    // queues

    vkb::Result<VkQueue> graphicsQueueResult = vkbDevice.get_queue(vkb::QueueType::graphics);
    vkb::Result<uint32_t> graphicsQueueFamilyResult = vkbDevice.get_queue_index(vkb::QueueType::graphics);

    m_graphicsQueue = UnwrapVkbResult(graphicsQueueResult);
    m_graphicsQueueFamily = UnwrapVkbResult(graphicsQueueFamilyResult);
}

void Engine::initAllocator()
{
    VmaAllocatorCreateInfo const allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = m_physicalDevice,
        .device = m_device,
        .instance = m_instance,
    };
    vmaCreateAllocator(&allocatorInfo, &m_allocator);

    m_engineDeletionQueue.pushFunction([&]() {
        vmaDestroyAllocator(m_allocator);
    });
}

void Engine::initSwapchain()
{
    vkb::SwapchainBuilder swapchainBuilder{ m_physicalDevice, m_device, m_surface };

    m_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    VkSurfaceFormatKHR const surfaceFormat{
        .format = m_swapchainImageFormat,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    uint32_t const width = m_windowExtent.width;
    uint32_t const height = m_windowExtent.height;

    vkb::Result<vkb::Swapchain> const swapchainResult = swapchainBuilder
        .set_desired_format(surfaceFormat)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build();
    vkb::Swapchain vkbSwapchain = UnwrapVkbResult(swapchainResult);

    m_swapchainExtent = { 
        .width = vkbSwapchain.extent.width,
        .height = vkbSwapchain.extent.height,
        .depth = 1,
    };
    m_swapchain = vkbSwapchain.swapchain;
    m_swapchainImages = vkbSwapchain.get_images().value();
    m_swapchainImageViews = vkbSwapchain.get_image_views().value();

    // Initialize the singular image used for rendering.

    VkExtent3D const drawImageExtent{
        .width{ m_windowExtent.width },
        .height{ m_windowExtent.height },
        .depth{ 1 }
    };

    m_drawImage = vkutil::allocateImage(
        m_allocator,
        m_device,
        m_engineDeletionQueue,
        drawImageExtent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT // copy to swapchain
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT 
        | VK_IMAGE_USAGE_STORAGE_BIT 
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT // during render passes
    );
}

void Engine::cleanupSwapchain()
{
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    for (VkImageView const& imageView : m_swapchainImageViews)
    {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
}

void Engine::initCommands()
{
    VkCommandPoolCreateInfo const commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_graphicsQueueFamily,
    };

    for (FrameData& frameData : m_frames)
    {
        CheckVkResult(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &frameData.commandPool));

        VkCommandBufferAllocateInfo const cmdAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = frameData.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        CheckVkResult(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &frameData.mainCommandBuffer));
    }
}

void Engine::initSyncStructures()
{
    // Signaled so first frame can occur
    VkFenceCreateInfo const fenceCreateInfo = vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo const semaphoreCreateInfo = vkinit::semaphoreCreateInfo();

    for (FrameData& frameData : m_frames)
    {
        CheckVkResult(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &frameData.renderFence));
        CheckVkResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &frameData.swapchainSemaphore));
        CheckVkResult(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &frameData.renderSemaphore));
    }
}


void Engine::mainLoop()
{
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) == GLFW_TRUE)
        {
            m_bRender = false;
        }
        else
        {
            m_bRender = true;
        }

        if (!m_bRender)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}

void Engine::draw()
{
    FrameData& currentFrame = getCurrentFrame();
    
    uint64_t const timeoutNanoseconds = 1'000'000'000; // 1 second
    CheckVkResult(vkWaitForFences(m_device, 1, &currentFrame.renderFence, VK_TRUE, timeoutNanoseconds));

    currentFrame.deletionQueue.flush();

    CheckVkResult(vkResetFences(m_device, 1, &currentFrame.renderFence));

    VkCommandBuffer const& cmd = currentFrame.mainCommandBuffer;
    CheckVkResult(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo const cmdBeginInfo = vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    CheckVkResult(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    vkutil::transitionImage(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // Begin scene drawing

    recordDrawBackground(cmd, m_drawImage.image);

    // End scene drawing

    // Copy image to swapchain

    uint32_t swapchainImageIndex;
    CheckVkResult(vkAcquireNextImageKHR(m_device,
        m_swapchain,
        timeoutNanoseconds,
        currentFrame.swapchainSemaphore,
        VK_NULL_HANDLE, // No fence to signal
        &swapchainImageIndex)
    );
    VkImage const& swapchainImage = m_swapchainImages[swapchainImageIndex];

    vkutil::transitionImage(cmd, 
        m_drawImage.image, 
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );
    vkutil::transitionImage(cmd, 
        swapchainImage, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    vkutil::recordCopyImageToImage(cmd,
        m_drawImage.image, swapchainImage,
        m_drawImage.imageExtent, m_swapchainExtent
    );

    vkutil::transitionImage(cmd, 
        swapchainImage, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    );

    CheckVkResult(vkEndCommandBuffer(cmd));

    // Submit commands

    VkCommandBufferSubmitInfo const cmdSubmitInfo = vkinit::commandBufferSubmitInfo(cmd);
    VkSemaphoreSubmitInfo const waitInfo = vkinit::semaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
        currentFrame.swapchainSemaphore
    );
    VkSemaphoreSubmitInfo const signalInfo = vkinit::semaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        currentFrame.renderSemaphore
    );

    auto const cmdSubmitInfos = std::vector<VkCommandBufferSubmitInfo>{ cmdSubmitInfo };
    auto const waitInfos = std::vector<VkSemaphoreSubmitInfo>{ waitInfo };
    auto const signalInfos = std::vector<VkSemaphoreSubmitInfo>{ signalInfo };
    VkSubmitInfo2 const submitInfo = vkinit::submitInfo(
        cmdSubmitInfos,
        waitInfos,
        signalInfos
    );

    CheckVkResult(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, currentFrame.renderFence));

    VkPresentInfoKHR const presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame.renderSemaphore,

        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,

        .pImageIndices = &swapchainImageIndex,
        .pResults = nullptr, // Only one swapchain
    };
    CheckVkResult(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));

    m_frameNumber++;
}

void Engine::recordDrawBackground(VkCommandBuffer cmd, VkImage image)
{
    // Clear the frame with a flat color
    VkClearColorValue const clearValue{
        .float32 = {
            0.0f,
            0.0f,
            abs(sin(m_frameNumber / 120.f)),
            1.0f,
        }
    };

    VkImageSubresourceRange const clearRange = vkinit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

}

void Engine::cleanup()
{
    if (m_initialized)
    {
        Log("Engine cleaning up.");

        vkDeviceWaitIdle(m_device);

        m_engineDeletionQueue.flush();

        for (FrameData const& frameData : m_frames)
        {
            vkDestroyCommandPool(m_device, frameData.commandPool, nullptr);

            vkDestroyFence(m_device, frameData.renderFence, nullptr);
            vkDestroySemaphore(m_device, frameData.renderSemaphore, nullptr);
            vkDestroySemaphore(m_device, frameData.swapchainSemaphore, nullptr);
        }

        cleanupSwapchain();

        vkDestroyDevice(m_device, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        vkDestroyInstance(m_instance, nullptr);

        glfwTerminate();
        glfwDestroyWindow(m_window);

        m_initialized = false;
    }
}