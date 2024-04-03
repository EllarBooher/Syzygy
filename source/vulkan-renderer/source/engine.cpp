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
    initDescriptors();
    initPipelines();
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

    m_engineDeletionQueue.pushFunction([=]() {
        vkDestroyCommandPool(m_device, m_immCommandPool, nullptr);
    });
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
    m_engineDeletionQueue.pushFunction([=]() { vkDestroyFence(m_device, m_immFence, nullptr); });
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
            pushConstants.push_back(shader.pushConstantRange());
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

    ImGui_ImplVulkan_Init(&initInfo);

    m_engineDeletionQueue.pushFunction([=]() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(m_device, imguiDescriptorPool, nullptr);
    });

    // Handle DPI

    float const fontBaseSize{ 13.0f };
    std::string const fontPath{ DebugUtils::getLoadedDebugUtils().makeAbsolutePath(std::filesystem::path{"assets/ProggyClean.ttf"}).string() };
    ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath.c_str(), fontBaseSize * m_dpiScale);

    ImGui::GetStyle().ScaleAllSizes(m_dpiScale);

    Log("ImGui initialized.");
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

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

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

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ShaderWrapper& currentComputeShader{ 
            m_computeShaders[m_computeShaderIndex % m_computeShaders.size()].computeShader
        };

        if (ImGui::Begin("Background Shader"))
        {
            ImGui::Text("Select shader to use:");
            for (uint32_t index{ 0 }; index < m_computeShaders.size(); index++)
            {
                ComputeShaderWrapper const& shader{ m_computeShaders[index] };
                if (ImGui::Button(shader.computeShader.name().c_str()))
                {
                    m_computeShaderIndex = index;
                }
            }
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Push constant controls"))
            {
                ImGui::Indent(16.0f);
                if (currentComputeShader.reflectionData().pushConstantsByEntryPoint.empty())
                {
                    ImGui::Text("No push constants.");
                }
                for (auto const& [entryPoint, pushConstant] : currentComputeShader.reflectionData().pushConstantsByEntryPoint)
                {
                    if (ImGui::CollapsingHeader(fmt::format("\"{}\", entry point \"{}\"", pushConstant.name, entryPoint).c_str()))
                    {
                        std::span<uint8_t> const pushConstantMappedData{ currentComputeShader.mapRuntimePushConstant(entryPoint) };
                        uint8_t* const pPushConstantData{ pushConstantMappedData.data() };

                        for (ShaderReflectionData::StructureMember const& pushConstantMember : pushConstant.members)
                        {
                            std::visit(overloaded{
                                [&](ShaderReflectionData::UnsupportedType const& unsupportedMember) {
                                    ImGui::Text(fmt::format("Unsupported member \"{}\"", pushConstantMember.name).c_str());
                                },
                                [&](ShaderReflectionData::NumericType const& numericMember) {

                                    // Gather format data
                                    uint32_t columns{ 0 };
                                    uint32_t rows{ 0 };
                                    std::visit(overloaded{
                                        [&](ShaderReflectionData::Scalar const& scalar) {
                                            columns = 1;
                                            rows = 1;
                                        },
                                        [&](ShaderReflectionData::Vector const& vector) {
                                            columns = vector.componentCount;
                                            rows = 1;
                                        },
                                        [&](ShaderReflectionData::Matrix const& matrix) {
                                            columns = matrix.columnCount;
                                            rows = matrix.rowCount;
                                        },
                                    }, numericMember.format);

                                    bool bSupportedType{ true };
                                    ImGuiDataType imguiDataType{ ImGuiDataType_Float };
                                    std::visit(overloaded{
                                        [&](ShaderReflectionData::Integer const& integerComponent) {
                                            assert(integerComponent.signedness == 0 || integerComponent.signedness == 1);

                                            switch (integerComponent.signedness)
                                            {
                                            case 0: //unsigned
                                                switch (numericMember.componentBitWidth)
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
                                                switch (numericMember.componentBitWidth)
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
                                            switch (numericMember.componentBitWidth)
                                            {
                                            case 64:
                                                imguiDataType = ImGuiDataType_Double ;
                                                break;
                                            case 32:
                                                imguiDataType = ImGuiDataType_Float ;
                                                break;
                                            default:
                                                bSupportedFloat = false;
                                                break;
                                            }
                                        },
                                    }, numericMember.componentType);

                                    if (!bSupportedType)
                                    {
                                        ImGui::Text(fmt::format("Unsupported component bit width {} for member {}", numericMember.componentBitWidth, pushConstantMember.name).c_str());
                                    }

                                    for (uint32_t row{ 0 }; row < rows; row++)
                                    {
                                        size_t const byteOffset{ row * columns * numericMember.componentBitWidth / 8 + pushConstantMember.offsetBytes };
                                        uint8_t* const pDataPointer{ &pushConstantMappedData[byteOffset] };

                                        // Check that ImGui won't modify out of bounds data
                                        assert((byteOffset + columns * numericMember.componentBitWidth / 8) <= pushConstantMappedData.size());

                                        std::string const rowLabel{ fmt::format("{}##{}", pushConstantMember.name, row) };

                                        // void pointer since it could be unsigned OR signed. Could be split up.
                                        ImGui::InputScalarN(rowLabel.c_str(), imguiDataType, reinterpret_cast<void*>(pDataPointer), columns);
                                    }
                                },
                            }, pushConstantMember.typeData);
                        }
                    }
                }

            }
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
    VkImageView const& swapchainImageView = m_swapchainImageViews[swapchainImageIndex];

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
    CheckVkResult(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));

    m_frameNumber++;
}

void Engine::recordDrawBackground(VkCommandBuffer cmd, VkImage image)
{
    ComputeShaderWrapper const& currentComputShader{
        m_computeShaders[m_computeShaderIndex % m_computeShaders.size()]
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, currentComputShader.pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, currentComputShader.pipelineLayout, 0, 1, &m_drawImageDescriptors, 0, nullptr);

    ShaderReflectionData const& reflectionData{ currentComputShader.computeShader.reflectionData() };
    if (reflectionData.defaultEntryPointHasPushConstant())
    {
        std::string const entryPoint{ reflectionData.defaultEntryPoint };
        std::span<uint8_t const> pushConstant{ currentComputShader.computeShader.readRuntimePushConstant(entryPoint) };
        if (!currentComputShader.computeShader.validatePushConstant(pushConstant, entryPoint))
        {
            Error(fmt::format("Invalid push constant data detected by \"{}\"", currentComputShader.computeShader.name()));
        }
        vkCmdPushConstants(cmd, currentComputShader.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstant.size(), pushConstant.data());
    }

    vkCmdDispatch(cmd, std::ceil(m_drawImage.imageExtent.width / 16.0), std::ceil(m_drawImage.imageExtent.height / 16.0), 1);
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
    if (m_initialized)
    {
        Log("Engine cleaning up.");

        CheckVkResult(vkDeviceWaitIdle(m_device));

        m_engineDeletionQueue.flush();

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