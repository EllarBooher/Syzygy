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

#include "initializers.hpp"
#include "helpers.h"
#include "images.hpp"
#include "descriptors.hpp"
#include "pipelines.hpp"

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
    initDrawTarget();
    
    initCommands();
    initSyncStructures();
    initDescriptors();

    updateDescriptors();

    initPipelines();
    initTrianglePipeline();
    initMeshPipeline();
    initDefaultMeshData();

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

void Engine::initDrawTarget()
{
    // Initialize the image used for rendering outside of the swapchain.

    m_drawImage = vkutil::allocateImage(
        m_allocator,
        m_device,
        m_swapchainExtent,
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

    m_globalDescriptorAllocator.initPool(m_device, 10, sizes);

    { // compute draw binding, see gradient.comp
        DescriptorLayoutBuilder builder{};
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        m_drawImageDescriptorLayout = builder.build(m_device, VK_SHADER_STAGE_COMPUTE_BIT);
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

void Engine::initPipelines()
{
    std::vector<std::string> computeShaders{
        "shaders/gradient.comp.spv",
        "shaders/gradient_color.comp.spv",
        "shaders/booleanpush.comp.spv"
    };
    initBackgroundPipelines(computeShaders);
}

void Engine::initBackgroundPipelines(std::span<std::string const> shaders)
{
    for (auto& oldShader : m_computeShaders)
    {
        oldShader.cleanup(m_device);
    }
    m_computeShaders.clear();
    for (std::string const& shaderName : shaders)
    {
        ShaderWrapper shader = vkutil::loadShaderModule(shaderName, m_device);
        if (!shader.isValid())
        {
            shader = ShaderWrapper::Invalid();
            Error("Error when building compute shader.");
        }

        std::vector<VkPushConstantRange> pushConstants{};
        if (shader.reflectionData().defaultEntryPointHasPushConstant())
        {
            pushConstants.push_back(shader.pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT));
        }
        VkPipelineLayoutCreateInfo const computeLayout{
            .sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
            .pNext{ nullptr },

            .setLayoutCount{ 1 },
            .pSetLayouts{ &m_drawImageDescriptorLayout },

            .pushConstantRangeCount{ static_cast<uint32_t>(pushConstants.size()) },
            .pPushConstantRanges{ pushConstants.data()},
        };

        VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
        CheckVkResult(vkCreatePipelineLayout(m_device, &computeLayout, nullptr, &pipelineLayout));

        VkPipelineShaderStageCreateInfo const stageInfo{
            .sType{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
            .pNext{ nullptr },

            .stage{ VK_SHADER_STAGE_COMPUTE_BIT },
            .module{ shader.shaderModule() },
            .pName{ shader.reflectionData().defaultEntryPoint.c_str()},
        };

        VkComputePipelineCreateInfo const computePipelineCreateInfo{
            .sType{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO },
            .pNext{ nullptr },

            .stage{ stageInfo },
            .layout{ pipelineLayout },
        };

        VkPipeline pipeline{ VK_NULL_HANDLE };
        CheckVkResult(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));

        m_computeShaders.push_back(ComputeShaderWrapper{
            .computeShader{ shader },
            .pipeline{ pipeline },
            .pipelineLayout{ pipelineLayout },
        });
    }
}

void Engine::initTrianglePipeline()
{
    ShaderWrapper const vertShader{ vkutil::loadShaderModule("shaders/colored_triangle.vert.spv", m_device) };
    ShaderWrapper const fragShader{ vkutil::loadShaderModule("shaders/colored_triangle.frag.spv", m_device) };

    std::vector<VkPushConstantRange> pushConstantRanges{};
    if (vertShader.reflectionData().defaultEntryPointHasPushConstant())
    {
        pushConstantRanges.push_back(vertShader.pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT));
    }
    if (fragShader.reflectionData().defaultEntryPointHasPushConstant())
    {
        pushConstantRanges.push_back(fragShader.pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT));
    }

    for (uint32_t i{ 0 }; i < pushConstantRanges.size(); i++)
    {
        for (uint32_t j{ i + 1 }; j < pushConstantRanges.size(); j++)
        {
            VkPushConstantRange const& rangeOne{ pushConstantRanges[i] };
            VkPushConstantRange const& rangeTwo{ pushConstantRanges[j] };

            if ((rangeOne.stageFlags & rangeTwo.stageFlags) == 0)
            {
                continue;
            }

            if (!(rangeOne.offset + rangeOne.size <= rangeTwo.offset
                || rangeOne.offset >= rangeTwo.offset + rangeTwo.size))
            {
                Warning(fmt::format("Overlapping push constant ranges discovered between \"{}\" and \"{}\""
                    , vertShader.name()
                    , fragShader.name()
                ));
            }
        }
    }

    VkPipelineLayoutCreateInfo const layoutInfo{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
        .pNext{ nullptr },

        .flags{ 0 },

        .setLayoutCount{ 0 },
        .pSetLayouts{ nullptr },

        .pushConstantRangeCount{ static_cast<uint32_t>(pushConstantRanges.size()) },
        .pPushConstantRanges{ pushConstantRanges.data() },
    };

    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    CheckVkResult(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &pipelineLayout));

    PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShaders(vertShader, fragShader);
    pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.setMultisamplingNone();
    pipelineBuilder.disableBlending();
    pipelineBuilder.disableDepthTest();

    pipelineBuilder.setColorAttachmentFormat(m_drawImage.imageFormat);
    pipelineBuilder.setDepthFormat(VK_FORMAT_UNDEFINED);

    m_trianglePipeline = GraphicsPipelineWrapper{
        .vertexShader{ vertShader },
        .fragmentShader{ fragShader },
        .pipeline{ pipelineBuilder.buildPipeline(m_device, pipelineLayout) },
        .pipelineLayout{ pipelineLayout }
    };
}

void Engine::initMeshPipeline()
{
    ShaderWrapper const vertShader{ vkutil::loadShaderModule("shaders/colored_triangle.vert.spv", m_device) };
    ShaderWrapper const fragShader{ vkutil::loadShaderModule("shaders/colored_triangle.frag.spv", m_device) };

    assert(vertShader.reflectionData().defaultEntryPointHasPushConstant());
    assert(fragShader.reflectionData().defaultEntryPointHasPushConstant());

    auto const& vertPushConstant{ vertShader.reflectionData().defaultPushConstant() };
    auto const& fragPushConstant{ fragShader.reflectionData().defaultPushConstant() };

    assert(
        vertPushConstant.type.logicallyCompatible(
            fragPushConstant.type
        )
    );

    assert(
        vertPushConstant.layoutOffsetBytes == 0
        && fragPushConstant.layoutOffsetBytes == 0
        && vertPushConstant.type.sizeBytes == fragPushConstant.type.sizeBytes
    );

    VkPushConstantRange const pushConstantRange{
        .stageFlags{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
        .offset{ 0 }, // Offset is zero since we know the exact two shaders
        .size{ vertPushConstant.type.paddedSizeBytes },
    };

    VkPipelineLayoutCreateInfo const layoutInfo{
        .sType{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO },
        .pNext{ nullptr },

        .flags{ 0 },

        .setLayoutCount{ 0 },
        .pSetLayouts{ nullptr },

        .pushConstantRangeCount{ 1 },
        .pPushConstantRanges{ &pushConstantRange },
    };

    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    CheckVkResult(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &pipelineLayout));

    PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShaders(vertShader, fragShader);
    pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.setMultisamplingNone();
    pipelineBuilder.disableBlending();
    pipelineBuilder.disableDepthTest();

    pipelineBuilder.setColorAttachmentFormat(m_drawImage.imageFormat);
    pipelineBuilder.setDepthFormat(VK_FORMAT_UNDEFINED);

    m_meshPipeline = GraphicsPipelineWrapper{
        .vertexShader{ vertShader },
        .fragmentShader{ fragShader },
        .pipeline{ pipelineBuilder.buildPipeline(m_device, pipelineLayout) },
        .pipelineLayout{ pipelineLayout },
        .pushConstant{ PipelinePushConstant{
            .engineDriven{ true },
            .pipelineStages{ pushConstantRange.stageFlags },
            .buffer{ std::vector<uint8_t>(pushConstantRange.size)},
            .reflectionData{ vertPushConstant },
        } },
    };
}

void Engine::initDefaultMeshData()
{
    // Pipeline should be initialized with VK_FRONT_FACE_CLOCKWISE

    std::array<Vertex, 4> rectangleVertices{
        Vertex{
            .position{0.5,-0.5,0},
            .uv_x{},
            .normal{},
            .uv_y{},
            .color{0,0,0,1},
        },
        Vertex{
            .position{0.5,0.5,0},
            .uv_x{},
            .normal{},
            .uv_y{},
            .color{0.5,0.5,0.5,1},
        },
        Vertex{
            .position{-0.5,0.5,0},
            .uv_x{},
            .normal{},
            .uv_y{},
            .color{0,1,0,1},
        },
        Vertex{
            .position{-0.5,-0.5,0},
            .uv_x{},
            .normal{},
            .uv_y{},
            .color{1,0,0,1},
        },
    };

    std::array<uint32_t, 6> rectangleIndices{
        0, 1, 2,
        2, 3, 0,
    };

    m_rectangleMesh = uploadedMeshToGPU(rectangleIndices, rectangleVertices);

    struct MeshDrawPushConstant {
        glm::mat4x4 renderMatrix;
        VkDeviceAddress vertexBuffer;
        glm::vec4 tint;
    };
    MeshDrawPushConstant const pushConstant{
        .renderMatrix{ glm::mat4x4(1.0f) },
        .vertexBuffer{ m_rectangleMesh.vertexBufferAddress },
        .tint{ glm::vec4(1.0f) },
    };

    // Copies are okay, just to test the interface
    m_meshPipeline.setPushConstant(pushConstant);
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
    std::string const fontPath{ DebugUtils::getLoadedDebugUtils().makeAbsolutePath(std::filesystem::path{"assets/ProggyClean.ttf"}).string() };
    ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath.c_str(), fontBaseSize * m_dpiScale);

    ImGui::GetStyle().ScaleAllSizes(m_dpiScale);

    Log("ImGui initialized.");
}

void Engine::resizeSwapchain()
{
    vkDeviceWaitIdle(m_device);
    m_drawImage.cleanup(m_device, m_allocator);
    cleanupSwapchain();

    int32_t width{ 0 }, height{ 0 };
    glfwGetWindowSize(m_window, &width, &height);
    m_windowExtent.width = static_cast<uint32_t>(width);
    m_windowExtent.height = static_cast<uint32_t>(height);

    initSwapchain();
    initDrawTarget();

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

AllocatedBuffer Engine::createBuffer(size_t allocationSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo const bufferInfo{
        .sType{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO },
        .pNext{ nullptr },

        .size{ allocationSize },
        .usage{ usage },
    };

    VmaAllocationCreateInfo const vmaAllocInfo{
        .flags{ VMA_ALLOCATION_CREATE_MAPPED_BIT },
        .usage{ memoryUsage },
    };

    AllocatedBuffer newBuffer{};
    CheckVkResult(
        vmaCreateBuffer(m_allocator, &bufferInfo, &vmaAllocInfo, 
        &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info)
    );

    return newBuffer;
}

GPUMeshBuffers Engine::uploadedMeshToGPU(std::span<uint32_t const> indices, std::span<Vertex const> vertices)
{
    // Allocate buffer 

    size_t const indexBufferSize{ indices.size_bytes() };
    size_t const vertexBufferSize{ vertices.size_bytes() };

    AllocatedBuffer const indexBuffer{ createBuffer(
        indexBufferSize
        , VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        , VMA_MEMORY_USAGE_GPU_ONLY
    ) };

    AllocatedBuffer const vertexBuffer{ createBuffer(
        vertexBufferSize
        , VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        , VMA_MEMORY_USAGE_GPU_ONLY
    ) };
    VkBufferDeviceAddressInfo const addressInfo{
        .sType{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO },
        .pNext{ nullptr },

        .buffer{ vertexBuffer.buffer },
    };
    VkDeviceAddress const vertexBufferAddress{ vkGetBufferDeviceAddress(m_device, &addressInfo) };

    // Copy data into buffer

    AllocatedBuffer stagingBuffer{ createBuffer(
        vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
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

    stagingBuffer.cleanup(m_allocator);

    return GPUMeshBuffers{
        .indexBuffer{ indexBuffer },
        .vertexBuffer{ vertexBuffer },
        .vertexBufferAddress{ vertexBufferAddress },
    };
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
void Engine::imguiPushStructureControl(ShaderReflectionData::Structure const& structure, std::span<uint8_t> backingData)
{
    for (ShaderReflectionData::StructureMember const& member : structure.members)
    {
        std::visit(overloaded{
            [&](ShaderReflectionData::UnsupportedType const& unsupportedType) {
                ImGui::Text(fmt::format("Unsupported member \"{}\"", member.name).c_str());
            },
            [&](ShaderReflectionData::NumericType const& numericType) {

                // Gather format data
                uint32_t columns{ 0 };
                uint32_t rows{ 0 };
                std::visit(overloaded{
                    [&](ShaderReflectionData::Scalar const& scalar) {
                        columns = 1;
                        rows = 1;
                    },
                    [&](ShaderReflectionData::Vector const& vector) {
                        columns = 1;
                        rows = vector.componentCount;
                    },
                    [&](ShaderReflectionData::Matrix const& matrix) {
                        columns = matrix.columnCount;
                        rows = matrix.rowCount;
                    },
                }, numericType.format);

                bool bSupportedType{ true };
                ImGuiDataType imguiDataType{ ImGuiDataType_Float };
                std::visit(overloaded{
                    [&](ShaderReflectionData::Integer const& integerComponent) {
                        assert(integerComponent.signedness == 0 || integerComponent.signedness == 1);

                        switch (integerComponent.signedness)
                        {
                        case 0: //unsigned
                            switch (numericType.componentBitWidth)
                            {
                            case 8:
                                imguiDataType = ImGuiDataType_U8;
                                break;
                            case 16:
                                imguiDataType = ImGuiDataType_U16;
                                break;
                            case 32:
                                imguiDataType = ImGuiDataType_U32;
                                break;
                            case 64:
                                imguiDataType = ImGuiDataType_U64;
                                break;
                            default:
                                bSupportedType = false;
                                break;
                            }
                            break;
                        default: //signed
                            switch (numericType.componentBitWidth)
                            {
                            case 8:
                                imguiDataType = ImGuiDataType_S8;
                                break;
                            case 16:
                                imguiDataType = ImGuiDataType_S16;
                                break;
                            case 32:
                                imguiDataType = ImGuiDataType_S32;
                                break;
                            case 64:
                                imguiDataType = ImGuiDataType_S64;
                                break;
                            default:
                                bSupportedType = false;
                                break;
                            }
                            break;
                        }
                    },
                    [&](ShaderReflectionData::Float const& floatComponent) {
                        bool bSupportedFloat{ true };
                        ImGuiDataType imguiDataType{ ImGuiDataType_Float };
                        switch (numericType.componentBitWidth)
                        {
                        case 64:
                            imguiDataType = ImGuiDataType_Double;
                            break;
                        case 32:
                            imguiDataType = ImGuiDataType_Float;
                            break;
                        default:
                            bSupportedFloat = false;
                            break;
                        }
                    },
                }, numericType.componentType);

                if (!bSupportedType)
                {
                    ImGui::Text(fmt::format("Unsupported component bit width {} for member {}", numericType.componentBitWidth, member.name).c_str());
                }

                // SPIR-V aggregate types are column major. 
                // Columns are members, and rows are components of a vector/column.
                for (uint32_t column{ 0 }; column < columns; column++)
                {
                    size_t const byteOffset{ column * rows * numericType.componentBitWidth / 8 + member.offsetBytes };
                    uint8_t* const pDataPointer{ &backingData[byteOffset] };

                    // Check that ImGui won't modify out of bounds data
                    assert((byteOffset + rows * numericType.componentBitWidth / 8) <= backingData.size());

                    std::string const rowLabel{ fmt::format("{}##{}", member.name, column) };

                    // void pointer since it could be unsigned OR signed. Could be split up.
                    ImGui::InputScalarN(rowLabel.c_str(), imguiDataType, reinterpret_cast<void*>(pDataPointer), rows);
                }
            },
        }, member.type.typeData);
    }
}
void Engine::imguiPushShaderControl(ShaderWrapper& shader)
{
    if (ImGui::CollapsingHeader(fmt::format("{} controls", shader.name()).c_str()))
    {
        ImGui::Indent(16.0f);
        if (shader.reflectionData().pushConstantsByEntryPoint.empty())
        {
            ImGui::Text("No push constants.");
        }
        for (auto const& [entryPoint, pushConstant] : shader.reflectionData().pushConstantsByEntryPoint)
        {
            if (ImGui::CollapsingHeader(fmt::format("\"{}\", entry point \"{}\"", pushConstant.name, entryPoint).c_str()))
            {
                std::span<uint8_t> const pushConstantMappedData{ shader.mapRuntimePushConstant(entryPoint) };

                imguiPushStructureControl(pushConstant.type, pushConstantMappedData);
            }
        }
        ImGui::Unindent(16.0f);
    }
}

void Engine::imguiPushGraphicsPipelineControl(GraphicsPipelineWrapper& pipeline)
{
    auto const& pushConstant{ pipeline.pushConstant.reflectionData };

    std::span<uint8_t> backingData{ pipeline.mapPushConstant() };

    imguiPushStructureControl(pushConstant.type, backingData);
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

        if (m_resizeRequested)
        {
            Log("Resizing swapchain.");
            resizeSwapchain();
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ShaderWrapper& currentComputeShader{ 
            m_computeShaders[m_computeShaderIndex % m_computeShaders.size()].computeShader
        };

        if (ImGui::Begin("Pipeline Controls"))
        {
            ImGui::Text("Select background shader to use:");
            ImGui::Indent(32.0f);
            for (uint32_t index{ 0 }; index < m_computeShaders.size(); index++)
            {
                ComputeShaderWrapper const& shader{ m_computeShaders[index] };
                if (ImGui::Button(shader.computeShader.name().c_str()))
                {
                    m_computeShaderIndex = index;
                }
            }
            ImGui::Separator();
            imguiPushShaderControl(currentComputeShader);
            ImGui::Unindent(32.0f);
            ImGui::Separator();
            ImGui::Text("Mesh pipeline controls:");
            ImGui::Indent(32.0f);
            imguiPushGraphicsPipelineControl(m_meshPipeline);
            ImGui::Unindent(32.0f);
        }
        ImGui::End();

        ImGui::Render();
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

    vkutil::transitionImage(cmd, m_drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    recordDrawGeometry(cmd);

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

void Engine::recordDrawBackground(VkCommandBuffer cmd, VkImage image)
{
    ComputeShaderWrapper const& currentComputeShader{
        m_computeShaders[m_computeShaderIndex % m_computeShaders.size()]
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, currentComputeShader.pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, currentComputeShader.pipelineLayout, 0, 1, &m_drawImageDescriptors, 0, nullptr);

    ShaderReflectionData const& reflectionData{ currentComputeShader.computeShader.reflectionData() };
    if (reflectionData.defaultEntryPointHasPushConstant())
    {
        std::string const entryPoint{ reflectionData.defaultEntryPoint };
        std::span<uint8_t const> pushConstant{ currentComputeShader.computeShader.readRuntimePushConstant(entryPoint) };
        if (!currentComputeShader.computeShader.validatePushConstant(pushConstant, entryPoint))
        {
            Error(fmt::format("Invalid push constant data detected by \"{}\"", currentComputeShader.computeShader.name()));
        }
        vkCmdPushConstants(cmd, currentComputeShader.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstant.size(), pushConstant.data());
    }

    vkCmdDispatch(cmd, std::ceil(m_drawImage.imageExtent.width / 16.0), std::ceil(m_drawImage.imageExtent.height / 16.0), 1);
}

void Engine::recordDrawGeometry(VkCommandBuffer cmd)
{
    VkRenderingAttachmentInfo const colorAttachment{
        .sType{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO },
        .pNext{ nullptr },

        .imageView{ m_drawImage.imageView },
        .imageLayout{ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
        
        .resolveMode{ VK_RESOLVE_MODE_NONE },
        .resolveImageView{ VK_NULL_HANDLE },
        .resolveImageLayout{ VK_IMAGE_LAYOUT_UNDEFINED },

        .loadOp{ VK_ATTACHMENT_LOAD_OP_LOAD },
        .storeOp{ VK_ATTACHMENT_STORE_OP_STORE },

        .clearValue{},
    };

    VkExtent2D const drawExtent{
        .width{m_drawImage.imageExtent.width},
        .height{m_drawImage.imageExtent.height},
    };
    std::vector<VkRenderingAttachmentInfo> const colorAttachments{ colorAttachment };
    VkRenderingInfo const renderInfo{
        vkinit::renderingInfo(drawExtent, colorAttachments)
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_meshPipeline.pipeline);

    VkViewport const viewport{
        .x{ 0 },
        .y{ 0 },
        .width{ static_cast<float>(drawExtent.width) },
        .height{ static_cast<float>(drawExtent.height) },
        .minDepth{ 0.0f },
        .maxDepth{ 1.0f },
    };

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D const scissor{
        .offset{
            .x{ 0 },
            .y{ 0 },
        },
        .extent{
            .width{ drawExtent.width },
            .height{ drawExtent.height },
        },
    };

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    struct MeshDrawPushConstant {
        glm::mat4x4 renderMatrix;
        VkDeviceAddress vertexBuffer;
        glm::vec4 tint;
    };

    MeshDrawPushConstant const currentPushConstant{
        m_meshPipeline.readPushConstant<MeshDrawPushConstant>()
    };

    vkCmdPushConstants(cmd, m_meshPipeline.pipelineLayout,
        m_meshPipeline.pushConstant.pipelineStages,
        0, sizeof(MeshDrawPushConstant), &currentPushConstant
    );
    vkCmdBindIndexBuffer(cmd, m_rectangleMesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

    vkCmdEndRendering(cmd);
}

void Engine::recordDrawImgui(VkCommandBuffer cmd, VkImageView view)
{
    VkRenderingAttachmentInfo const colorAttachmentInfo{
        vkinit::renderingAttachmentInfo(view, {}, false, VK_IMAGE_LAYOUT_GENERAL)
    };

    std::vector<VkRenderingAttachmentInfo> colorAttachments{ colorAttachmentInfo };
    VkRenderingInfo const renderingInfo{
        vkinit::renderingInfo(VkExtent2D{ m_swapchainExtent.width, m_swapchainExtent.height }, colorAttachments)
    };

    vkCmdBeginRendering(cmd, &renderingInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void Engine::cleanup()
{
    if (!m_initialized)
    {
        return;
    }

    Log("Engine cleaning up.");

    CheckVkResult(vkDeviceWaitIdle(m_device));
        
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(m_device, m_imguiDescriptorPool, nullptr);

    m_trianglePipeline.cleanup(m_device);
    m_meshPipeline.cleanup(m_device);

    m_rectangleMesh.cleanup(m_allocator);

    for (auto& shader : m_computeShaders)
    {
        shader.cleanup(m_device);
    }

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

    m_drawImage.cleanup(m_device, m_allocator);
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