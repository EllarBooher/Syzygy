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

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <implot.h>

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/random.hpp>

#include <glm/gtx/string_cast.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtx/quaternion.hpp>

#include "initializers.hpp"
#include "helpers.hpp"
#include "images.hpp"
#include "descriptors.hpp"
#include "pipelines.hpp"

#include "ui/engineui.hpp"
#include "ui/pipelineui.hpp"

#define VKRENDERER_COMPILE_WITH_TESTING 0

CameraParameters const Engine::m_defaultCameraParameters = CameraParameters{
    .cameraPosition{ glm::vec3(0.0f,-4.0f,-8.0f) },
    .eulerAngles{ glm::vec3(-0.3f,0.0f,0.0f) },
    .fov{ 70.0f },
    .near{ 0.1f },
    .far{ 10000.0f },
};

AtmosphereParameters const Engine::m_defaultAtmosphereParameters = AtmosphereParameters{
    .sunEulerAngles{ glm::vec3(0.0, 0.0, 0.0) },

    .earthRadiusMeters{ 6378000 },
    .atmosphereRadiusMeters{ 6420000 },

    .groundColor{ 0.9, 0.8, 0.6 },

    .scatteringCoefficientRayleigh{ glm::vec3(0.0000038, 0.0000135, 0.0000331) },
    .altitudeDecayRayleigh{ 7994.0 },

    .scatteringCoefficientMie{ glm::vec3(0.000021) },
    .altitudeDecayMie{ 1200.0 },
};

Engine::Engine()
{
    init();
}

void Engine::run()
{
    mainLoop();
    cleanup();
}

std::unique_ptr<Engine> Engine::loadEngine()
{
    return std::make_unique<Engine>(Engine{});
}

void Engine::init()
{
    assert(m_loadedEngine == nullptr);
    m_loadedEngine = this;

    DebugUtils::init();
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

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
    initDrawTargets();
    
    initCommands();
    initSyncStructures();
    initDescriptors();

    updateDescriptors();

    initDefaultMeshData();
    initWorld();
    initDebug();
    initBackgroundPipeline();
    initInstancedPipeline();
    initGenericComputePipelines();

    initImgui();

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

    VkPhysicalDeviceVulkan13Features const features13
    {
        .synchronization2{ VK_TRUE },
        .dynamicRendering{ VK_TRUE },
    };

    VkPhysicalDeviceVulkan12Features const features12
    {
        .descriptorIndexing{ VK_TRUE },
        .bufferDeviceAddress{ VK_TRUE },
    };
    
    VkPhysicalDeviceFeatures const features
    {
        .wideLines{ VK_TRUE },
    };

    VkPhysicalDeviceShaderObjectFeaturesEXT const shaderObjectFeature
    {
        .sType{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT },
        .pNext{ nullptr },

        .shaderObject{ VK_TRUE },
    };

    vkb::Result<vkb::PhysicalDevice> const physicalDeviceBuildResult = vkb::PhysicalDeviceSelector{ vkbInstance }
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_required_features(features)
        .add_required_extension_features(shaderObjectFeature)
        .add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME)
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
}

void Engine::initDrawTargets()
{
    // Initialize the image used for rendering outside of the swapchain.

    m_drawImage = vkutil::allocateImage(
        m_allocator,
        m_device,
        m_swapchainExtent,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT // copy to swapchain
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT // during render passes
    );

    m_depthImage = vkutil::allocateImage(
        m_allocator,
        m_device,
        m_drawImage.imageExtent,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
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

void Engine::cleanupDrawTargets()
{
    m_drawImage.cleanup(m_device, m_allocator);
    m_depthImage.cleanup(m_device, m_allocator);
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

    // Immediate command structures

    CheckVkResult(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_immCommandPool));
    VkCommandBufferAllocateInfo const immCmdAllocInfo{
        .sType{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO },
        .pNext{ nullptr },

        .commandPool{ m_immCommandPool },
        .level{ VK_COMMAND_BUFFER_LEVEL_PRIMARY },
        .commandBufferCount{ 1 },
    };
    CheckVkResult(vkAllocateCommandBuffers(m_device, &immCmdAllocInfo, &m_immCommandBuffer));
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

    // Immediate sync structures

    CheckVkResult(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_immFence));
}

void Engine::initDescriptors()
{
    // Set up the image used by the compute shader.

    std::vector<DescriptorAllocator::PoolSizeRatio> const sizes{
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f }
    };

    m_globalDescriptorAllocator.initPool(m_device, 10, sizes, 0);

    { // compute draw binding, see gradient.comp
        DescriptorLayoutBuilder builder{};
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1, 0);
        m_drawImageDescriptorLayout = builder.build(m_device, 0);
    }

    m_drawImageDescriptors = m_globalDescriptorAllocator.allocate(m_device, m_drawImageDescriptorLayout);
}

void Engine::updateDescriptors()
{
    VkDescriptorImageInfo const drawImageInfo{
        .sampler{ VK_NULL_HANDLE },
        .imageView{ m_drawImage.imageView },
        .imageLayout{ VK_IMAGE_LAYOUT_GENERAL },
    };

    VkWriteDescriptorSet const drawImageWrite{
        .sType{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
        .pNext{ nullptr },

        .dstSet{ m_drawImageDescriptors },
        .dstBinding{ 0 },
        .dstArrayElement{ 0 },
        .descriptorCount{ 1 },
        .descriptorType{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },

        .pImageInfo{ &drawImageInfo },
        .pBufferInfo{ nullptr },
        .pTexelBufferView{ nullptr },
    };

    vkUpdateDescriptorSets(m_device, 1, &drawImageWrite, 0, nullptr);
}

void Engine::initDefaultMeshData()
{
    m_testMeshes = loadGltfMeshes(this, "assets/vkguide/basicmesh.glb").value();

    assert(m_testMeshes.size() > 2);
}

glm::quat randomQuat()
{
    // https://stackoverflow.com/a/56794499

    glm::vec2 const xy{ glm::diskRand(1.0f) };
    glm::vec2 const uv{ glm::diskRand(1.0f) };

    float const s{ glm::sqrt((1 - glm::length2(xy)) / glm::length2(uv)) };

    return glm::quat(s * uv.y, xy.x, xy.y, s * uv.x);
}

void Engine::initWorld()
{
    int32_t const coordinateMin{ -40 };
    int32_t const coordinateMax{ 40 };

    if (m_meshInstances.models != nullptr || m_meshInstances.modelInverseTransposes != nullptr)
    {
        Warning("initWorld called when World already initialized");
        return;
    }

    { // Mesh Instances
        for (int32_t x{ coordinateMin }; x <= coordinateMax; x++)
        {
            for (int32_t z{ coordinateMin }; z <= coordinateMax; z++)
            {
                m_meshInstances.originals.push_back(
                    glm::translate(glm::vec3(x, 0, z))
                    * glm::toMat4(randomQuat())
                    * glm::scale(glm::vec3(0.2f))
                );
            }
        }

        VkDeviceSize const maxInstanceCount{ m_meshInstances.originals.size() };
        m_meshInstances.models = std::make_unique<TStagedBuffer<glm::mat4x4>>(TStagedBuffer<glm::mat4x4>::allocate(
            m_device,
            m_allocator,
            maxInstanceCount,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ));
        m_meshInstances.modelInverseTransposes = std::make_unique<TStagedBuffer<glm::mat4x4>>(TStagedBuffer<glm::mat4x4>::allocate(
            m_device,
            m_allocator,
            maxInstanceCount,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ));

        std::vector<glm::mat4x4> modelInverseTransposes{};
        for (glm::mat4x4 const& model : m_meshInstances.originals)
        {
            modelInverseTransposes.push_back(glm::inverseTranspose(model));
        }

        m_meshInstances.models->stage(m_meshInstances.originals);
        m_meshInstances.modelInverseTransposes->stage(modelInverseTransposes);

        immediateSubmit([&](VkCommandBuffer cmd) {
            m_meshInstances.models->recordCopyToDevice(cmd, m_allocator);
            m_meshInstances.modelInverseTransposes->recordCopyToDevice(cmd, m_allocator);
        });
    }

    { // Camera
        m_camerasBuffer = std::make_unique<TStagedBuffer<GPUTypes::Camera>>(TStagedBuffer<GPUTypes::Camera>::allocate(
            m_device,
            m_allocator,
            20,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ));
        m_camerasBuffer->push(GPUTypes::Camera{});
        m_camerasBuffer->push(GPUTypes::Camera{});

        m_cameraIndexMain = 0;
        m_cameraIndexShadowpass = 1;
    }

    { // Atmosphere
        m_atmospheresBuffer = std::make_unique<TStagedBuffer<GPUTypes::Atmosphere>>(TStagedBuffer<GPUTypes::Atmosphere>::allocate(
            m_device,
            m_allocator,
            1,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ));
        std::vector<GPUTypes::Atmosphere> const atmospheres{ m_atmosphereParameters.toDeviceEquivalent() };
        m_atmospheresBuffer->stage(atmospheres);

        immediateSubmit([&](VkCommandBuffer cmd) {
            m_atmospheresBuffer->recordCopyToDevice(cmd, m_allocator);
        });
    }
}

void Engine::initDebug()
{
    m_debugLines.pipeline = std::make_unique<DebugLineComputePipeline>(
        m_device,
        m_drawImage.imageFormat,
        m_depthImage.imageFormat
    );
    m_debugLines.indices = std::make_unique<TStagedBuffer<uint32_t>>(
        TStagedBuffer<uint32_t>::allocate(
            m_device,
            m_allocator,
            1000,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        )
    );
    m_debugLines.vertices = std::make_unique<TStagedBuffer<Vertex>>(
        TStagedBuffer<Vertex>::allocate(
            m_device,
            m_allocator,
            1000,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        )
    );
}

void Engine::initInstancedPipeline()
{
    m_instancePipeline = std::make_unique<InstancedMeshGraphicsPipeline>(
        m_device,
        m_drawImage.imageFormat,
        m_depthImage.imageFormat
    );
}

void Engine::initBackgroundPipeline()
{
    m_atmospherePipeline = std::make_unique<AtmosphereComputePipeline>(
        m_device,
        m_drawImageDescriptorLayout
    );
}

void Engine::initGenericComputePipelines()
{
    std::vector<std::string> const shaderPaths
    {
        "shaders/booleanpush.comp.spv"
        , "shaders/gradient_color.comp.spv"
        , "shaders/sparse_push_constant.comp.spv"
        , "shaders/matrix_color.comp.spv"
    };
    m_genericComputePipeline = std::make_unique<GenericComputeCollectionPipeline>(
        m_device,
        m_drawImageDescriptorLayout,
        shaderPaths
    );
}

void Engine::initImgui()
{
    Log("Initializing ImGui...");

    std::vector<VkDescriptorPoolSize> const poolSizes{
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo const poolInfo{
        .sType{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO },
        .flags{ VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT },

        .maxSets{ 1000 },
        .poolSizeCount{ static_cast<uint32_t>(poolSizes.size())},
        .pPoolSizes{ poolSizes.data() },
    };

    VkDescriptorPool imguiDescriptorPool{ VK_NULL_HANDLE };
    CheckVkResult(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &imguiDescriptorPool));

    ImGui::CreateContext();
    ImPlot::CreateContext();

    assert(m_swapchainImageFormat != VK_FORMAT_UNDEFINED);
    std::vector<VkFormat> const colorAttachmentFormats{ m_swapchainImageFormat };
    VkPipelineRenderingCreateInfo const dynamicRenderingInfo{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO },
        .pNext{ nullptr },

        .viewMask{ 0 }, // Not sure on this value
        .colorAttachmentCount{ static_cast<uint32_t>(colorAttachmentFormats.size()) },
        .pColorAttachmentFormats{ colorAttachmentFormats.data() },

        .depthAttachmentFormat{ VK_FORMAT_UNDEFINED },
        .stencilAttachmentFormat{ VK_FORMAT_UNDEFINED },
    };

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    // Load functions since we are using volk, and not the built-in vulkan loader
    ImGui_ImplVulkan_LoadFunctions([](const char* functionName, void* vkInstance) {
        return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vkInstance)), functionName);
        }, &m_instance);
    
    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance{ m_instance },
        .PhysicalDevice{ m_physicalDevice },
        .Device{ m_device },

        .QueueFamily{ m_graphicsQueueFamily },
        .Queue{ m_graphicsQueue },

        .DescriptorPool{ imguiDescriptorPool },

        .MinImageCount{ 3 },
        .ImageCount{ 3 },
        .MSAASamples{ VK_SAMPLE_COUNT_1_BIT }, // No MSAA

        // Dynamic rendering
        .UseDynamicRendering{ true },
        .PipelineRenderingCreateInfo{ dynamicRenderingInfo },

        // Allocation/Debug
        .Allocator{ nullptr },
        .CheckVkResultFn{ CheckVkResult_Imgui },
        .MinAllocationSize{ 1024 * 1024 },
    };
    m_imguiDescriptorPool = imguiDescriptorPool;

    ImGui_ImplVulkan_Init(&initInfo);

    // Handle DPI

    float const fontBaseSize{ 13.0f };
    std::string const fontPath{ DebugUtils::getLoadedDebugUtils().makeAbsolutePath(std::filesystem::path{"assets/proggyfonts/ProggyClean.ttf"}).string() };
    ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath.c_str(), fontBaseSize * m_dpiScale);

    ImGui::GetStyle().ScaleAllSizes(m_dpiScale);

    Log("ImGui initialized.");
}

void Engine::resizeSwapchain()
{
    vkDeviceWaitIdle(m_device);
    cleanupDrawTargets();
    cleanupSwapchain();

    int32_t width{ 0 }, height{ 0 };
    glfwGetWindowSize(m_window, &width, &height);
    m_windowExtent.width = static_cast<uint32_t>(width);
    m_windowExtent.height = static_cast<uint32_t>(height);

    initSwapchain();
    initDrawTargets();

    updateDescriptors();

    m_resizeRequested = false;
}

void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    CheckVkResult(vkResetFences(m_device, 1, &m_immFence));
    CheckVkResult(vkResetCommandBuffer(m_immCommandBuffer, 0));

    VkCommandBuffer const& cmd = m_immCommandBuffer;

    VkCommandBufferBeginInfo const cmdBeginInfo{
        vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
    };

    CheckVkResult(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    CheckVkResult(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo const cmdSubmitInfo{
        vkinit::commandBufferSubmitInfo(cmd)
    };
    std::vector<VkCommandBufferSubmitInfo> const cmdSubmitInfos{ cmdSubmitInfo };
    VkSubmitInfo2 const submitInfo{
        vkinit::submitInfo(cmdSubmitInfos, {}, {})
    };

    CheckVkResult(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, m_immFence));

    // 100 second timeout
    uint64_t const immediateSubmitTimeout{ 100'000'000'000 };
    CheckVkResult(vkWaitForFences(m_device, 1, &m_immFence, true, immediateSubmitTimeout));
}

std::unique_ptr<GPUMeshBuffers> Engine::uploadMeshToGPU(std::span<uint32_t const> indices, std::span<Vertex const> vertices)
{
    // Allocate buffer 

    size_t const indexBufferSize{ indices.size_bytes() };
    size_t const vertexBufferSize{ vertices.size_bytes() };

    AllocatedBuffer indexBuffer{ AllocatedBuffer::allocate(
        m_device,
        m_allocator,
        indexBufferSize
        , VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        , VMA_MEMORY_USAGE_GPU_ONLY
        , 0
    ) };

    AllocatedBuffer vertexBuffer{ AllocatedBuffer::allocate(
        m_device,
        m_allocator,
        vertexBufferSize
        , VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        , VMA_MEMORY_USAGE_GPU_ONLY
        , 0 
    ) };
    VkBufferDeviceAddressInfo const addressInfo{
        .sType{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO },
        .pNext{ nullptr },

        .buffer{ vertexBuffer.buffer },
    };
    VkDeviceAddress const vertexBufferAddress{ vkGetBufferDeviceAddress(m_device, &addressInfo) };

    // Copy data into buffer

    AllocatedBuffer stagingBuffer{ AllocatedBuffer::allocate(
        m_device,
        m_allocator,
        vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
        , VMA_ALLOCATION_CREATE_MAPPED_BIT
    ) };

    uint8_t* const data{ reinterpret_cast<uint8_t*>(stagingBuffer.allocation->GetMappedData()) };
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy(data + vertexBufferSize, indices.data(), indexBufferSize);

    immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy const vertexCopy{
            .srcOffset{ 0 },
            .dstOffset{ 0 },
            .size{ vertexBufferSize },
        };
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy const indexCopy{
            .srcOffset{ vertexBufferSize },
            .dstOffset{ 0 },
            .size{ indexBufferSize },
        };
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, indexBuffer.buffer, 1, &indexCopy);
    });

    return std::make_unique<GPUMeshBuffers>(std::move(indexBuffer), std::move(vertexBuffer));
}

// TODO: Once scenes are made, extract this to a testing scene
#if VKRENDERER_COMPILE_WITH_TESTING
void testDebugLines(float currentTimeSeconds, DebugLines& debugLines)
{
    glm::quat const boxOrientation{
        glm::toQuat(glm::orientate3(glm::vec3(currentTimeSeconds, currentTimeSeconds * glm::euler<float>(),0.0)))
    };

    debugLines.pushBox(
        glm::vec3(3.0 * glm::cos(2.0 * currentTimeSeconds), -2.0, 3.0 * glm::sin(2.0 * currentTimeSeconds))
        , boxOrientation
        , glm::vec3{ 1.0, 1.0, 1.0 }
    );

    debugLines.pushRectangle(
        glm::vec3{ 2.0, -2.0, 0.0 }
        , glm::quatLookAt(glm::vec3(-1.0, -1.0, 1.0), glm::vec3(-1.0, -1.0, -1.0))
        , glm::vec2{ 3.0, 1.0 }
    );
}
#endif

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

        static double previousTimeSeconds{ 0 };

        if (glfwGetTime() >= previousTimeSeconds + 1.0 / m_targetFPS)
        {
            double const currentTimeSeconds{ glfwGetTime() };
            double const deltaTimeSeconds{ currentTimeSeconds - previousTimeSeconds };

            m_debugLines.clear();

            tickWorld(currentTimeSeconds, deltaTimeSeconds);
#if VKRENDERER_COMPILE_WITH_TESTING
            testDebugLines(currentTimeSeconds, m_debugLines);
#endif
            previousTimeSeconds = glfwGetTime();

            double const instantFPS{ 1.0f / deltaTimeSeconds };

            m_fpsValues.write(instantFPS);

            if (!m_bRender)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            if (m_resizeRequested)
            {
                Log("Resizing swapchain.");
                resizeSwapchain();
            }

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            imguiPerformanceWindow(m_fpsValues.values(), m_fpsValues.average(), m_fpsValues.current(), m_targetFPS);

            if (ImGui::Begin("Pipeline Controls"))
            {
                imguiMeshInstanceControls(
                    m_renderMeshInstances,
                    m_testMeshes,
                    m_testMeshUsed
                );
                ImGui::Separator();
                imguiBackgroundRenderingControls(
                    m_useAtmosphereCompute,
                    *m_atmospherePipeline,
                    *m_genericComputePipeline
                );
                ImGui::Separator();
                imguiPipelineControls<InstancedMeshGraphicsPipeline const>(*m_instancePipeline);
            }
            ImGui::End();

            if (ImGui::Begin("Engine Controls"))
            {
                ImGui::Checkbox("Use Orthographic Camera", &m_useOrthographicProjection);
                ImGui::Checkbox("Use Shadowpass Camera", &m_useShadowpassPerspective);
                imguiStructureControls(m_cameraParameters, m_defaultCameraParameters);
                ImGui::Separator();
                imguiStructureControls(m_atmosphereParameters, m_defaultAtmosphereParameters);
                ImGui::Separator();
                imguiStructureControls(m_debugLines);
            }
            ImGui::End();

            ImGui::Render();
            draw();
        }
    }
}

void Engine::tickWorld(double totalTime, double deltaTimeSeconds)
{
    std::span<glm::mat4x4> const models{ m_meshInstances.models->mapValidStaged() };
    std::span<glm::mat4x4> const modelInverseTransposes{ m_meshInstances.modelInverseTransposes->mapValidStaged() };

    if (models.size() != modelInverseTransposes.size())
    {
        Warning("models and modelInverseTransposes out of sync");
        return;
    }

    size_t index{ 0 };
    for (glm::mat4x4 const& modelOriginal : m_meshInstances.originals)
    {
        glm::vec4 const position = modelOriginal * glm::vec4(0.0, 0.0, 0.0, 1.0);

        double const y{ std::sin(totalTime + (position.x - (-10) + position.z - (-10)) / 3.1415) };

        models[index] = glm::translate(glm::vec3(0.0, y, 0.0)) * modelOriginal;
        // In general, the model inverse transposes only need to be updated once per tick,
        // before rendering and after the last update of the model matrices.
        // For now, we only update once per tick, so we just compute it here.
        modelInverseTransposes[index] = glm::inverseTranspose(models[index]);

        index += 1;
    }

    // Atmosphere
    {
        AtmosphereParameters::AnimationParameters const atmosphereAnimation{ m_atmosphereParameters.animation };
        if (atmosphereAnimation.animateSun)
        {
            bool const isNight{ m_atmosphereParameters.directionToSun().y > 0.2f };

            if (isNight && atmosphereAnimation.skipNight)
            {
                m_atmosphereParameters.sunEulerAngles.x = -1.0 * glm::sign(atmosphereAnimation.animationSpeed) * glm::radians(100.0f);
            }
            else
            {
                m_atmosphereParameters.sunEulerAngles.x += deltaTimeSeconds * atmosphereAnimation.animationSpeed;
            }
        }
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

    { // Copy cameras to gpu
        std::span<GPUTypes::Camera> cameras{ m_camerasBuffer->mapValidStaged() };
        cameras[m_cameraIndexMain] = { 
                m_useOrthographicProjection
            ? m_cameraParameters.toDeviceEquivalentOrthographic(getAspectRatio(), 5.0)
            : m_cameraParameters.toDeviceEquivalent(getAspectRatio()) 
        };
        cameras[m_cameraIndexShadowpass] = {
            m_cameraParameters.makeShadowpassCamera(
                        getAspectRatio()
                , -m_atmosphereParameters.directionToSun()
                , 20.0f
                    )
            };

        m_camerasBuffer->recordCopyToDevice(cmd, m_allocator);

    }

    { // Copy atmospheres to gpu
        std::span<GPUTypes::Atmosphere> const stagedAtmospheres{ m_atmospheresBuffer->mapValidStaged() };
        if (stagedAtmospheres.size() <= m_atmosphereIndex)
        {
            Warning("AtmosphereIndex does not point to valid atmosphere, resetting to 0.");
            m_atmosphereIndex = 0;
        }
        if (stagedAtmospheres.size() > 0 && m_atmosphereIndex < stagedAtmospheres.size())
        {
            stagedAtmospheres[m_atmosphereIndex] = m_atmosphereParameters.toDeviceEquivalent();
        }
        m_atmospheresBuffer->recordCopyToDevice(cmd, m_allocator);
    }

    {
        uint32_t const cameraIndex{
            m_useShadowpassPerspective
            ? m_cameraIndexShadowpass
            : m_cameraIndexMain
        };

    if (m_useAtmosphereCompute)
    {
        m_atmospherePipeline->recordDrawCommands(
            cmd,
                cameraIndex,
            *m_camerasBuffer,
            m_atmosphereIndex,
            *m_atmospheresBuffer,
            m_drawImageDescriptors,
            VkExtent2D{
                .width{ m_drawImage.imageExtent.width },
                .height{ m_drawImage.imageExtent.height },
            }
        );
    }
    else 
    {
        m_genericComputePipeline->recordDrawCommands(
            cmd,
            m_drawImageDescriptors,
            VkExtent2D{ m_drawImage.imageExtent.width, m_drawImage.imageExtent.height }
        );
    }

    vkutil::transitionImage(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transitionImage(cmd, m_depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    if (m_renderMeshInstances)
    {
        m_meshInstances.models->recordCopyToDevice(cmd, m_allocator);
        m_meshInstances.modelInverseTransposes->recordCopyToDevice(cmd, m_allocator);

        m_instancePipeline->recordDrawCommands(
            cmd
            , false
            , m_drawImage
            , m_depthImage
                , cameraIndex
            , *m_camerasBuffer
            , m_atmosphereIndex
            , *m_atmospheresBuffer
            , *m_testMeshes[m_testMeshUsed]
            , *m_meshInstances.models
            , *m_meshInstances.modelInverseTransposes
        );
    }

        recordDrawDebugLines(cmd, cameraIndex, *m_camerasBuffer);
    }

    // End scene drawing

    // Copy image to swapchain

    uint32_t swapchainImageIndex;
    VkResult const acquireResult{ vkAcquireNextImageKHR(m_device,
        m_swapchain,
        timeoutNanoseconds,
        currentFrame.swapchainSemaphore,
        VK_NULL_HANDLE, // No fence to signal
        &swapchainImageIndex
    ) };
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_resizeRequested = true;
        CheckVkResult(vkEndCommandBuffer(cmd));
        return;
    }
    CheckVkResult(acquireResult);

    VkImage const& swapchainImage = m_swapchainImages[swapchainImageIndex];
    VkImageView const& swapchainImageView = m_swapchainImageViews[swapchainImageIndex];

    vkutil::transitionImage(cmd, 
        m_drawImage.image, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
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
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    recordDrawImgui(cmd, swapchainImageView);

    vkutil::transitionImage(cmd, 
        swapchainImage, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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

    VkResult const presentResult{ vkQueuePresentKHR(m_graphicsQueue, &presentInfo) };
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_resizeRequested = true;
    }
    else
    {
        CheckVkResult(presentResult);
    }

    m_frameNumber++;
}

void Engine::recordDrawImgui(VkCommandBuffer cmd, VkImageView view)
{
    VkRenderingAttachmentInfo const colorAttachmentInfo{
        vkinit::renderingAttachmentInfo(view, {}, false, VK_IMAGE_LAYOUT_GENERAL)
    };

    std::vector<VkRenderingAttachmentInfo> colorAttachments{ colorAttachmentInfo };
    VkRenderingInfo const renderingInfo{
        vkinit::renderingInfo(VkExtent2D{ m_swapchainExtent.width, m_swapchainExtent.height }, colorAttachments, nullptr)
    };

    vkCmdBeginRendering(cmd, &renderingInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void Engine::recordDrawDebugLines(
    VkCommandBuffer cmd
    , uint32_t cameraIndex
    , TStagedBuffer<GPUTypes::Camera> const& camerasBuffer
)
{
    m_debugLines.lastFrameDrawResults = {};

    if (m_debugLines.enabled && m_debugLines.indices->stagedSize() > 0) {
        m_debugLines.recordCopy(cmd, m_allocator);

        DrawResultsGraphics const drawResults{ m_debugLines.pipeline->recordDrawCommands(
            cmd
            , false
            , m_debugLines.lineWidth
            , m_drawImage
            , m_depthImage
            , cameraIndex
            , camerasBuffer
            , *m_debugLines.vertices
            , *m_debugLines.indices
        ) };

        m_debugLines.lastFrameDrawResults = drawResults;
    }
}

void Engine::cleanup()
{
    if (!m_initialized)
    {
        return;
    }

    Log("Engine cleaning up.");

    CheckVkResult(vkDeviceWaitIdle(m_device));
        
    ImPlot::DestroyContext();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(m_device, m_imguiDescriptorPool, nullptr);

    m_instancePipeline->cleanup(m_device);
    m_atmospherePipeline->cleanup(m_device);
    m_genericComputePipeline->cleanup(m_device);

    m_meshInstances.models.reset();
    m_meshInstances.modelInverseTransposes.reset();

    m_atmospheresBuffer.reset();
    m_camerasBuffer.reset();

    m_testMeshes.clear();
    m_debugLines.cleanup(m_device, m_allocator);

    m_globalDescriptorAllocator.destroyPool(m_device);
    vkDestroyDescriptorSetLayout(m_device, m_drawImageDescriptorLayout, nullptr);

    for (FrameData const& frameData : m_frames)
    {
        vkDestroyCommandPool(m_device, frameData.commandPool, nullptr);

        vkDestroyFence(m_device, frameData.renderFence, nullptr);
        vkDestroySemaphore(m_device, frameData.renderSemaphore, nullptr);
        vkDestroySemaphore(m_device, frameData.swapchainSemaphore, nullptr);
    }

    vkDestroyFence(m_device, m_immFence, nullptr);
    vkDestroyCommandPool(m_device, m_immCommandPool, nullptr);

    cleanupDrawTargets();
    cleanupSwapchain();

    vmaDestroyAllocator(m_allocator);

    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        
    vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    vkDestroyInstance(m_instance, nullptr);

    glfwTerminate();
    glfwDestroyWindow(m_window);

    m_initialized = false;
}
