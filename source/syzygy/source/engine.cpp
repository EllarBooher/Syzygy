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

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <glm/gtx/string_cast.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "descriptors.hpp"
#include "helpers.hpp"
#include "images.hpp"
#include "initializers.hpp"
#include "pipelines.hpp"

#include "lights.hpp"

#include "ui/engineui.hpp"
#include "ui/pipelineui.hpp"

#define VKRENDERER_COMPILE_WITH_TESTING 0

CameraParameters const Engine::m_defaultCameraParameters{CameraParameters{
    .cameraPosition = glm::vec3(0.0F, -8.0F, -8.0F),
    .eulerAngles = glm::vec3(-0.3F, 0.0F, 0.0F),
    .fov = 70.0F,
    .near = 0.1F,
    .far = 10000.0F,
}};

AtmosphereParameters const Engine::m_defaultAtmosphereParameters{
    AtmosphereParameters{
        .sunEulerAngles = glm::vec3(1.0, 0.0, 0.0),

        .earthRadiusMeters = 6378000,
        .atmosphereRadiusMeters = 6420000,

        .groundColor = glm::vec3{0.9, 0.8, 0.6},

        .scatteringCoefficientRayleigh =
            glm::vec3(0.0000038, 0.0000135, 0.0000331),
        .altitudeDecayRayleigh = 7994.0,

        .scatteringCoefficientMie = glm::vec3(0.000021),
        .altitudeDecayMie = 1200.0,
    }
};

Engine::Engine(PlatformWindow const& window) { init(window); }

auto Engine::loadEngine(PlatformWindow const& window) -> Engine*
{
    if (m_loadedEngine == nullptr)
    {
        Log("Loading Engine.");
        m_loadedEngine = new Engine(window);
    }
    else
    {
        Warning("Called loadEngine when one was already loaded. No new engine "
                "was loaded.");
    }

    return m_loadedEngine;
}

void Engine::init(PlatformWindow const& window)
{
    m_uiPreferences.dpiScale = glm::round(glm::min<float>(
        static_cast<float>(window.extent().y)
            / static_cast<float>(RESOLUTION_DEFAULT.height),
        static_cast<float>(window.extent().x)
            / static_cast<float>(RESOLUTION_DEFAULT.width)
    ));

    initVulkan(window);

    m_initialized = true;

    Log("Engine Initialized.");
}

void Engine::initVulkan(PlatformWindow const& window)
{
    Log("Initializing Vulkan...");

    volkInitialize();

    initInstanceSurfaceDevices(window.handle);

    volkLoadDevice(m_device);

    initAllocator();

    initSwapchain(window.extent());
    initDrawTargets();

    initCommands();
    initSyncStructures();
    initDescriptors();

    updateDescriptors();

    initDefaultMeshData();
    initWorld();
    initDebug();
    initGenericComputePipelines();

    initDeferredShadingPipeline();

    initImgui(window.handle);

    Log("Vulkan Initialized.");
}

void Engine::initInstanceSurfaceDevices(GLFWwindow* const window)
{
    // create VkInstance and VkDebugUtilsMessengerEXT

    vkb::InstanceBuilder instanceBuilder;
    vkb::Result<vkb::Instance> const instanceBuildResult{
        instanceBuilder.set_app_name("Renderer")
            .request_validation_layers()
            .use_default_debug_messenger()
            .require_api_version(1, 3, 0)
            .build()
    };
    vkb::Instance const vkbInstance{UnwrapVkbResult(instanceBuildResult)};

    volkLoadInstance(vkbInstance.instance);

    m_instance = vkbInstance.instance;
    m_debugMessenger = vkbInstance.debug_messenger;

    // create VkSurfaceKHR

    glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface);

    // create VkPhysicalDevice and VkDevice

    VkPhysicalDeviceVulkan13Features const features13{
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features const features12{
        .descriptorIndexing = VK_TRUE,

        .descriptorBindingPartiallyBound = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,

        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceFeatures const features{
        .wideLines = VK_TRUE,
    };

    VkPhysicalDeviceShaderObjectFeaturesEXT const shaderObjectFeature{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
        .pNext = nullptr,

        .shaderObject = VK_TRUE,
    };

    vkb::Result<vkb::PhysicalDevice> const physicalDeviceBuildResult{
        vkb::PhysicalDeviceSelector{vkbInstance}
            .set_minimum_version(1, 3)
            .set_required_features_13(features13)
            .set_required_features_12(features12)
            .set_required_features(features)
            .add_required_extension_features(shaderObjectFeature)
            .add_required_extension(VK_EXT_SHADER_OBJECT_EXTENSION_NAME)
            .set_surface(m_surface)
            .select()
    };
    vkb::PhysicalDevice const vkbPhysicalDevice{
        UnwrapVkbResult(physicalDeviceBuildResult)
    };

    vkb::DeviceBuilder const deviceBuilder{vkbPhysicalDevice};
    vkb::Result<vkb::Device> const deviceBuildResult = deviceBuilder.build();
    vkb::Device const vkbDevice = UnwrapVkbResult(deviceBuildResult);

    volkLoadDevice(vkbDevice.device);

    m_device = vkbDevice.device;
    m_physicalDevice = vkbDevice.physical_device;

    // queues

    vkb::Result<VkQueue> const graphicsQueueResult{
        vkbDevice.get_queue(vkb::QueueType::graphics)
    };
    vkb::Result<uint32_t> const graphicsQueueFamilyResult{
        vkbDevice.get_queue_index(vkb::QueueType::graphics)
    };

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

void Engine::initSwapchain(glm::u16vec2 extent)
{
    m_swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    VkSurfaceFormatKHR const surfaceFormat{
        .format = m_swapchainImageFormat,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };

    uint32_t const width{extent.x};
    uint32_t const height{extent.y};

    vkb::Result<vkb::Swapchain> const swapchainResult{
        vkb::SwapchainBuilder{m_physicalDevice, m_device, m_surface}
            .set_desired_format(surfaceFormat)
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
    };
    vkb::Swapchain vkbSwapchain = UnwrapVkbResult(swapchainResult);

    m_swapchainExtent = {
        .width = vkbSwapchain.extent.width,
        .height = vkbSwapchain.extent.height,
    };
    m_swapchain = vkbSwapchain.swapchain;
    m_swapchainImages = vkbSwapchain.get_images().value();
    m_swapchainImageViews = vkbSwapchain.get_image_views().value();

    m_sceneRect = VkRect2D{
        .extent{m_swapchainExtent},
    };
}

void Engine::initDrawTargets()
{
    // Initialize the image used for rendering outside of the swapchain.

    VkExtent3D constexpr RESERVED_IMAGE_EXTENT{
        MAX_DRAW_EXTENTS.width, MAX_DRAW_EXTENTS.height, 1
    };
    VkFormat constexpr COLOR_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};
    VkImageAspectFlags constexpr COLOR_ASPECTS{VK_IMAGE_ASPECT_COLOR_BIT};

    m_sceneColorTexture =
        AllocatedImage::allocate(
            m_allocator,
            m_device,
            AllocatedImage::AllocationParameters{
                .extent = RESERVED_IMAGE_EXTENT,
                .format = COLOR_FORMAT,
                .usageFlags =
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                    | VK_IMAGE_USAGE_SAMPLED_BIT // used as descriptor for e.g.
                                                 // ImGui
                    | VK_IMAGE_USAGE_STORAGE_BIT // used in compute passes
                    | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT // used in graphics
                                                          // passes
                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT,    // copy to from other
                                                          // render passes
                .viewFlags = COLOR_ASPECTS,
            }
        )
            .value_or(AllocatedImage::makeInvalid());

    m_drawImage =
        AllocatedImage::allocate(
            m_allocator,
            m_device,
            AllocatedImage::AllocationParameters{
                .extent = RESERVED_IMAGE_EXTENT,
                .format = COLOR_FORMAT,
                .usageFlags =
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT // copy to swapchain
                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    | VK_IMAGE_USAGE_STORAGE_BIT
                    | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // during render
                                                           // passes
                .viewFlags = COLOR_ASPECTS,
            }
        )
            .value_or(AllocatedImage::makeInvalid()
            ); // TODO: handle failed case

    m_sceneDepthTexture =
        AllocatedImage::allocate(
            m_allocator,
            m_device,
            AllocatedImage::AllocationParameters{
                .extent = RESERVED_IMAGE_EXTENT,
                .format = VK_FORMAT_D32_SFLOAT,
                .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .viewFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
            }
        )
            .value_or(AllocatedImage::makeInvalid());
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
    m_sceneColorTexture.cleanup(m_device, m_allocator);
    m_drawImage.cleanup(m_device, m_allocator);
    m_sceneDepthTexture.cleanup(m_device, m_allocator);
}

void Engine::initCommands()
{
    VkCommandPoolCreateInfo const commandPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_graphicsQueueFamily,
    };

    for (FrameData& frameData : m_frames)
    {
        CheckVkResult(vkCreateCommandPool(
            m_device, &commandPoolInfo, nullptr, &frameData.commandPool
        ));

        VkCommandBufferAllocateInfo const cmdAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = frameData.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        CheckVkResult(vkAllocateCommandBuffers(
            m_device, &cmdAllocInfo, &frameData.mainCommandBuffer
        ));
    }

    // Immediate command structures

    CheckVkResult(vkCreateCommandPool(
        m_device, &commandPoolInfo, nullptr, &m_immCommandPool
    ));
    VkCommandBufferAllocateInfo const immCmdAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,

        .commandPool = m_immCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CheckVkResult(vkAllocateCommandBuffers(
        m_device, &immCmdAllocInfo, &m_immCommandBuffer
    ));
}

void Engine::initSyncStructures()
{
    // Signaled so first frame can occur
    VkFenceCreateInfo const fenceCreateInfo{
        vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT)
    };
    VkSemaphoreCreateInfo const semaphoreCreateInfo{vkinit::semaphoreCreateInfo(
    )};

    for (FrameData& frameData : m_frames)
    {
        CheckVkResult(vkCreateFence(
            m_device, &fenceCreateInfo, nullptr, &frameData.renderFence
        ));
        CheckVkResult(vkCreateSemaphore(
            m_device,
            &semaphoreCreateInfo,
            nullptr,
            &frameData.swapchainSemaphore
        ));
        CheckVkResult(vkCreateSemaphore(
            m_device, &semaphoreCreateInfo, nullptr, &frameData.renderSemaphore
        ));
    }

    // Immediate sync structures

    CheckVkResult(
        vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_immFence)
    );
}

void Engine::initDescriptors()
{
    std::vector<DescriptorAllocator::PoolSizeRatio> const sizes{
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0.5F},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0.5F}
    };

    m_globalDescriptorAllocator.initPool(
        m_device,
        DESCRIPTOR_SET_CAPACITY_DEFAULT,
        sizes,
        (VkDescriptorPoolCreateFlags)0
    );

    { // Set up the image used by compute shaders.
        m_sceneTextureDescriptorLayout =
            DescriptorLayoutBuilder{}
                .addBinding(
                    DescriptorLayoutBuilder::AddBindingParameters{
                        .binding = 0,
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .stageMask = VK_SHADER_STAGE_COMPUTE_BIT,
                        .bindingFlags = 0,
                    },
                    1
                )
                .build(m_device, 0)
                .value_or(VK_NULL_HANDLE); // TODO: handle

        m_sceneTextureDescriptors = m_globalDescriptorAllocator.allocate(
            m_device, m_sceneTextureDescriptorLayout
        );
    }
}

void Engine::updateDescriptors()
{
    VkDescriptorImageInfo const sceneTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_sceneColorTexture.imageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet const sceneTextureWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,

        .dstSet = m_sceneTextureDescriptors,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,

        .pImageInfo = &sceneTextureInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    std::vector<VkWriteDescriptorSet> const writes{sceneTextureWrite};

    vkUpdateDescriptorSets(m_device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
}

void Engine::initDefaultMeshData()
{
    m_testMeshes =
        loadGltfMeshes( // NOLINT(bugprone-unchecked-optional-access):
                        // Necessary for program execution
            this,
            "assets/vkguide/basicmesh.glb"
        )
            .value();
}

auto randomQuat() -> glm::quat
{
    // https://stackoverflow.com/a/56794499

    glm::vec2 const xy{glm::diskRand(1.0F)};
    glm::vec2 const uv{glm::diskRand(1.0F)};

    float const s{glm::sqrt((1 - glm::length2(xy)) / glm::length2(uv))};

    return {s * uv.y, xy.x, xy.y, s * uv.x};
}

void Engine::initWorld()
{
    int32_t const coordinateMin{-40};
    int32_t const coordinateMax{40};

    if (m_meshInstances.models != nullptr
        || m_meshInstances.modelInverseTransposes != nullptr)
    {
        Warning("initWorld called when World already initialized");
        return;
    }

    { // Mesh Instances
        // Floor
        for (int32_t x{coordinateMin}; x <= coordinateMax; x++)
        {
            for (int32_t z{coordinateMin}; z <= coordinateMax; z++)
            {
                glm::vec3 const position{
                    static_cast<float>(x) * 20.0F,
                    1.0F,
                    static_cast<float>(z) * 20.0F
                };
                glm::vec3 const scale{10.0F, 2.0F, 10.0F};

                m_meshInstances.originals.push_back(
                    glm::translate(position) * glm::scale(scale)
                );
            }
        }

        m_meshInstances.dynamicIndex = m_meshInstances.originals.size();

        for (int32_t x{coordinateMin}; x <= coordinateMax; x++)
        {
            for (int32_t z{coordinateMin}; z <= coordinateMax; z++)
            {
                glm::vec3 const position{
                    static_cast<float>(x), -4.0, static_cast<float>(z)
                };
                glm::quat const orientation{randomQuat()};
                glm::vec3 const scale{0.2F};

                m_meshInstances.originals.push_back(
                    glm::translate(position) * glm::toMat4(orientation)
                    * glm::scale(scale)
                );
            }
        }

        VkDeviceSize const maxInstanceCount{m_meshInstances.originals.size()};
        m_meshInstances.models = std::make_unique<TStagedBuffer<glm::mat4x4>>(
            TStagedBuffer<glm::mat4x4>::allocate(
                m_device,
                m_allocator,
                maxInstanceCount,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            )
        );
        m_meshInstances.modelInverseTransposes =
            std::make_unique<TStagedBuffer<glm::mat4x4>>(
                TStagedBuffer<glm::mat4x4>::allocate(
                    m_device,
                    m_allocator,
                    maxInstanceCount,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                )
            );

        std::vector<glm::mat4x4> modelInverseTransposes{};
        modelInverseTransposes.reserve(m_meshInstances.originals.size());
        for (glm::mat4x4 const& model : m_meshInstances.originals)
        {
            modelInverseTransposes.push_back(glm::inverseTranspose(model));
        }

        m_meshInstances.models->stage(m_meshInstances.originals);
        m_meshInstances.modelInverseTransposes->stage(modelInverseTransposes);

        immediateSubmit(
            [&](VkCommandBuffer cmd)
            {
                m_meshInstances.models->recordCopyToDevice(cmd, m_allocator);
                m_meshInstances.modelInverseTransposes->recordCopyToDevice(
                    cmd, m_allocator
                );
            }
        );
    }

    { // Camera

        m_camerasBuffer = std::make_unique<TStagedBuffer<gputypes::Camera>>(
            TStagedBuffer<gputypes::Camera>::allocate(
                m_device,
                m_allocator,
                CAMERA_CAPACITY,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            )
        );
        m_camerasBuffer->push(gputypes::Camera{});

        m_cameraIndexMain = 0;
    }

    { // Atmosphere
        m_atmospheresBuffer =
            std::make_unique<TStagedBuffer<gputypes::Atmosphere>>(
                TStagedBuffer<gputypes::Atmosphere>::allocate(
                    m_device,
                    m_allocator,
                    ATMOSPHERE_CAPACITY,
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                )
            );
        std::vector<gputypes::Atmosphere> const atmospheres{
            m_atmosphereParameters.toDeviceEquivalent()
        };
        m_atmospheresBuffer->stage(atmospheres);

        immediateSubmit(
            [&](VkCommandBuffer cmd)
            { m_atmospheresBuffer->recordCopyToDevice(cmd, m_allocator); }
        );
    }
}

void Engine::initDebug()
{
    m_debugLines.pipeline = std::make_unique<DebugLineGraphicsPipeline>(
        m_device,
        DebugLineGraphicsPipeline::ImageFormats{
            .color = m_sceneColorTexture.imageFormat,
            .depth = m_sceneDepthTexture.imageFormat,
        }
    );
    m_debugLines.indices = std::make_unique<TStagedBuffer<uint32_t>>(
        TStagedBuffer<uint32_t>::allocate(
            m_device,
            m_allocator,
            DEBUGLINES_CAPACITY,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        )
    );
    m_debugLines.vertices =
        std::make_unique<TStagedBuffer<Vertex>>(TStagedBuffer<Vertex>::allocate(
            m_device,
            m_allocator,
            DEBUGLINES_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ));
}

void Engine::initDeferredShadingPipeline()
{
    m_deferredShadingPipeline = std::make_unique<DeferredShadingPipeline>(
        m_device, m_allocator, m_globalDescriptorAllocator, MAX_DRAW_EXTENTS
    );

    m_deferredShadingPipeline->updateRenderTargetDescriptors(
        m_device, m_sceneDepthTexture
    );
}

void Engine::initGenericComputePipelines()
{
    std::vector<std::string> const shaderPaths{
        "shaders/booleanpush.comp.spv",
        "shaders/gradient_color.comp.spv",
        "shaders/sparse_push_constant.comp.spv",
        "shaders/matrix_color.comp.spv"
    };
    m_genericComputePipeline = std::make_unique<ComputeCollectionPipeline>(
        m_device, m_sceneTextureDescriptorLayout, shaderPaths
    );
}

void Engine::initImgui(GLFWwindow* const window)
{
    Log("Initializing ImGui...");

    std::vector<VkDescriptorPoolSize> const poolSizes{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo const poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,

        .maxSets = 1000,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VkDescriptorPool imguiDescriptorPool{VK_NULL_HANDLE};
    CheckVkResult(vkCreateDescriptorPool(
        m_device, &poolInfo, nullptr, &imguiDescriptorPool
    ));

    ImGui::CreateContext();
    ImPlot::CreateContext();

    std::vector<VkFormat> const colorAttachmentFormats{m_drawImage.imageFormat};
    VkPipelineRenderingCreateInfo const dynamicRenderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,

        .viewMask = 0, // Not sure on this value
        .colorAttachmentCount =
            static_cast<uint32_t>(colorAttachmentFormats.size()),
        .pColorAttachmentFormats = colorAttachmentFormats.data(),

        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    // Load functions since we are using volk,
    // and not the built-in vulkan loader
    ImGui_ImplVulkan_LoadFunctions(
        [](char const* functionName, void* vkInstance)
        {
            return vkGetInstanceProcAddr(
                *(reinterpret_cast<VkInstance*>(vkInstance)), functionName
            );
        },
        &m_instance
    );

    // This amount is recommended by ImGui to satisfy validation layers, even if
    // a little wasteful
    VkDeviceSize constexpr IMGUI_MIN_ALLOCATION_SIZE{1024ULL * 1024ULL};

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = m_instance,
        .PhysicalDevice = m_physicalDevice,
        .Device = m_device,

        .QueueFamily = m_graphicsQueueFamily,
        .Queue = m_graphicsQueue,

        .DescriptorPool = imguiDescriptorPool,

        .MinImageCount = 3,
        .ImageCount = 3,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT, // No MSAA

        // Dynamic rendering
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = dynamicRenderingInfo,

        // Allocation/Debug
        .Allocator = nullptr,
        .CheckVkResultFn = CheckVkResult_Imgui,
        .MinAllocationSize = IMGUI_MIN_ALLOCATION_SIZE,
    };
    m_imguiDescriptorPool = imguiDescriptorPool;

    ImGui_ImplVulkan_Init(&initInfo);

    { // Initialize the descriptor set that imgui uses to read our color output
        VkSamplerCreateInfo const samplerInfo{vkinit::samplerCreateInfo(
            0,
            VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            VK_FILTER_NEAREST,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
        )};

        assert(m_imguiSceneTextureSampler == VK_NULL_HANDLE);
        vkCreateSampler(
            m_device, &samplerInfo, nullptr, &m_imguiSceneTextureSampler
        );

        m_imguiSceneTextureDescriptor = ImGui_ImplVulkan_AddTexture(
            m_imguiSceneTextureSampler,
            m_sceneColorTexture.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    m_uiReloadRequested = true;

    m_imguiStyleDefault = ImGui::GetStyle();

    Log("ImGui initialized.");
}

void Engine::resizeSwapchain(glm::u16vec2 extent)
{
    vkDeviceWaitIdle(m_device);
    cleanupSwapchain();
    initSwapchain(extent);

    m_resizeRequested = false;
}

void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function
)
{
    CheckVkResult(vkResetFences(m_device, 1, &m_immFence));
    CheckVkResult(vkResetCommandBuffer(m_immCommandBuffer, 0));

    VkCommandBuffer const& cmd = m_immCommandBuffer;

    VkCommandBufferBeginInfo const cmdBeginInfo{vkinit::commandBufferBeginInfo(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    )};

    CheckVkResult(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    CheckVkResult(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo const cmdSubmitInfo{
        vkinit::commandBufferSubmitInfo(cmd)
    };
    std::vector<VkCommandBufferSubmitInfo> const cmdSubmitInfos{cmdSubmitInfo};
    VkSubmitInfo2 const submitInfo{vkinit::submitInfo(cmdSubmitInfos, {}, {})};

    CheckVkResult(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, m_immFence));

    // 100 second timeout
    uint64_t constexpr SUBMIT_TIMEOUT_NANOSECONDS{100'000'000'000};
    VkBool32 constexpr WAIT_ALL{VK_TRUE};
    CheckVkResult(vkWaitForFences(
        m_device, 1, &m_immFence, WAIT_ALL, SUBMIT_TIMEOUT_NANOSECONDS
    ));
}

auto Engine::uploadMeshToGPU(
    std::span<uint32_t const> const indices,
    std::span<Vertex const> const vertices
) -> std::unique_ptr<GPUMeshBuffers>
{
    // Allocate buffer

    size_t const indexBufferSize{indices.size_bytes()};
    size_t const vertexBufferSize{vertices.size_bytes()};

    AllocatedBuffer indexBuffer{AllocatedBuffer::allocate(
        m_device,
        m_allocator,
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    )};

    AllocatedBuffer vertexBuffer{AllocatedBuffer::allocate(
        m_device,
        m_allocator,
        vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    )};

    // Copy data into buffer

    AllocatedBuffer stagingBuffer{AllocatedBuffer::allocate(
        m_device,
        m_allocator,
        vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VMA_ALLOCATION_CREATE_MAPPED_BIT
    )};

    uint8_t* const data{
        reinterpret_cast<uint8_t*>(stagingBuffer.allocation->GetMappedData())
    };

    if (data == nullptr)
    {
        Warning("Mesh upload failed: Pointer to staging buffer was nullptr.");
        return nullptr;
    }

    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy(data + vertexBufferSize, indices.data(), indexBufferSize);

    immediateSubmit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy const vertexCopy{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertexBufferSize,
            };
            vkCmdCopyBuffer(
                cmd, stagingBuffer.buffer, vertexBuffer.buffer, 1, &vertexCopy
            );

            VkBufferCopy const indexCopy{
                .srcOffset = vertexBufferSize,
                .dstOffset = 0,
                .size = indexBufferSize,
            };
            vkCmdCopyBuffer(
                cmd, stagingBuffer.buffer, indexBuffer.buffer, 1, &indexCopy
            );
        }
    );

    return std::make_unique<GPUMeshBuffers>(
        std::move(indexBuffer), std::move(vertexBuffer)
    );
}

// TODO: Once scenes are made, extract this to a testing scene
#if VKRENDERER_COMPILE_WITH_TESTING
void testDebugLines(float currentTimeSeconds, DebugLines& debugLines)
{
    glm::quat const boxOrientation{glm::toQuat(glm::orientate3(glm::vec3(
        currentTimeSeconds, currentTimeSeconds * glm::euler<float>(), 0.0
    )))};

    debugLines.pushBox(
        glm::vec3(
            3.0 * glm::cos(2.0 * currentTimeSeconds),
            -2.0,
            3.0 * glm::sin(2.0 * currentTimeSeconds)
        ),
        boxOrientation,
        glm::vec3{1.0, 1.0, 1.0}
    );

    debugLines.pushRectangle(
        glm::vec3{2.0, -2.0, 0.0},
        glm::quatLookAt(
            glm::vec3(-1.0, -1.0, 1.0), glm::vec3(-1.0, -1.0, -1.0)
        ),
        glm::vec2{3.0, 1.0}
    );
}
#endif

void Engine::mainLoop(
    double const elapsedTimeSeconds,
    double const deltaTimeSeconds,
    bool shouldRender,
    glm::u16vec2 windowExtent
)
{
    m_debugLines.clear();

    TickTiming const tickTiming{
        .timeElapsed = elapsedTimeSeconds,
        .deltaTimeSeconds = deltaTimeSeconds,
    };

    tickWorld(tickTiming);
#if VKRENDERER_COMPILE_WITH_TESTING
    testDebugLines(currentTimeSeconds, m_debugLines);
#endif
    double const instantFPS{1.0F / tickTiming.deltaTimeSeconds};

    m_fpsValues.write(instantFPS);

    if (!shouldRender)
    {
        return;
    }

    if (m_resizeRequested)
    {
        Log("Resizing swapchain.");
        resizeSwapchain(windowExtent);
    }

    if (renderUI(m_device))
    {
        // TODO: fix
        // For some reason, ImGui gives visual artifacts for two frames
        // when resetting certain docking states. We force updating to
        // flush these through.
        renderUI(m_device);
        renderUI(m_device);
    }
    draw();
}

// TODO: Break this method up to get rid of the NOLINT. It should be as pure as
// possible, with most of the UI implementation living outside of this source
// file. Considerations must be made for updating the scene drawing extents and
// all of engine's members.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Engine::renderUI(VkDevice const device) -> bool
{
    if (m_uiReloadRequested)
    {
        // It is necessary to defer reloading to before each frame, since
        // beginning an ImGui frame locks some resources we wish to modify.

        float constexpr FONT_BASE_SIZE{13.0F};

        UIPreferences const currentPreferences{m_uiPreferences};

        std::filesystem::path const fontPath{
            DebugUtils::getLoadedDebugUtils().makeAbsolutePath(
                std::filesystem::path{"assets/proggyfonts/ProggyClean.ttf"}
            )
        };
        ImGui::GetIO().Fonts->Clear();
        ImGui::GetIO().Fonts->AddFontFromFileTTF(
            fontPath.string().c_str(),
            FONT_BASE_SIZE * currentPreferences.dpiScale
        );

        // Wait for idle since we are modifying backend resources
        vkDeviceWaitIdle(device);
        // We destroy this to later force a rebuild when the fonts are needed.
        ImGui_ImplVulkan_DestroyFontsTexture();

        // TODO: is rebuilding the font with a specific scale good?
        // ImGui recommends building fonts at various sizes then just
        // selecting them

        // Reset style so further scaling works off the base "1.0x" scaling
        ImGui::GetStyle() = m_imguiStyleDefault;
        ImGui::GetStyle().ScaleAllSizes(currentPreferences.dpiScale);

        m_uiReloadRequested = false;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    HUDState const hud{renderHUD(m_uiPreferences)};

    m_uiReloadRequested = hud.applyPreferencesRequested;
    if (hud.resetPreferencesRequested)
    {
        m_uiPreferences = m_uiPreferencesDefault;
        m_uiReloadRequested = true;
    }

    bool skipFrame{false};
    std::optional<DockingLayout> dockingLayout{};

    if (hud.maximizeSceneViewport)
    {
        ImGui::SetNextWindowPos(hud.workArea.pos());
        ImGui::SetNextWindowSize(hud.workArea.size());

        ImGuiWindowFlags constexpr FULLSCREEN_WINDOW_FLAGS{
            ImGuiWindowFlags_None | ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_NoBringToFrontOnFocus
        };

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
        if (ImGui::Begin(
                "SceneViewportMaximized", nullptr, FULLSCREEN_WINDOW_FLAGS
            ))
        {
            VkExtent2D const sceneContentExtent{
                .width =
                    static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                .height =
                    static_cast<uint32_t>(ImGui::GetContentRegionAvail().y),
            };

            m_sceneRect = VkRect2D{
                .offset{VkOffset2D{
                    .x = 0,
                    .y = 0,
                }},
                .extent{VkExtent2D{sceneContentExtent}},
            };

            ImVec2 const uvMax{
                static_cast<float>(m_sceneRect.extent.width)
                    / static_cast<float>(m_sceneColorTexture.imageExtent.width),
                static_cast<float>(m_sceneRect.extent.height)
                    / static_cast<float>(m_sceneColorTexture.imageExtent.height)
            };

            ImGui::Image(
                (ImTextureID)m_imguiSceneTextureDescriptor,
                ImGui::GetContentRegionAvail(),
                ImVec2{0.0, 0.0},
                uvMax,
                ImVec4{1.0F, 1.0F, 1.0F, 1.0F},
                ImVec4{0.0F, 0.0F, 0.0F, 0.0F}
            );
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }
    else
    {
        if (hud.resetLayoutRequested && hud.dockspaceID != 0)
        {
            dockingLayout =
                buildDefaultMultiWindowLayout(hud.workArea, hud.dockspaceID);
            skipFrame = true;
        }

        { // Scene Controls
            if (dockingLayout.has_value())
            {
                ImGui::SetNextWindowDockID(dockingLayout.value().left);
            }

            if (ImGui::Begin("Scene Controls"))
            {
                imguiMeshInstanceControls(
                    m_renderMeshInstances, m_testMeshes, m_testMeshUsed
                );

                ImGui::Separator();
                imguiStructureControls(m_sceneBounds, DEFAULT_SCENE_BOUNDS);

                ImGui::Separator();
                ImGui::Checkbox("Show Spotlights", &m_showSpotlights);

                ImGui::Separator();
                ImGui::Checkbox(
                    "Use Orthographic Camera", &m_useOrthographicProjection
                );

                ImGui::Separator();
                imguiStructureControls(
                    m_cameraParameters, m_defaultCameraParameters
                );

                ImGui::Separator();
                imguiStructureControls(
                    m_atmosphereParameters, m_defaultAtmosphereParameters
                );
            }
            ImGui::End();
        }

        { // Engine Controls
            if (dockingLayout.has_value())
            {
                ImGui::SetNextWindowDockID(dockingLayout.value().right);
            }

            if (ImGui::Begin("Engine Controls"))
            {
                imguiRenderingSelection(m_activeRenderingPipeline);

                ImGui::Separator();
                switch (m_activeRenderingPipeline)
                {
                case RenderingPipelines::DEFERRED:
                    imguiPipelineControls(*m_deferredShadingPipeline);
                    break;
                case RenderingPipelines::COMPUTE_COLLECTION:
                    imguiPipelineControls(*m_genericComputePipeline);
                    break;
                default:
                    ImGui::Text("Invalid rendering pipeline selected.");
                    break;
                }

                ImGui::Separator();
                imguiStructureControls(m_debugLines);
            }
            ImGui::End();
        }

        { // Performance window
            if (dockingLayout.has_value())
            {
                ImGui::SetNextWindowDockID(dockingLayout.value().centerBottom);
            }

            imguiPerformanceWindow(
                PerformanceValues{
                    .samplesFPS = m_fpsValues.values(),
                    .averageFPS = m_fpsValues.average(),
                    .currentFrame = m_fpsValues.current(),
                },
                m_targetFPS
            );
        }

        { // Scene viewport
            if (dockingLayout.has_value())
            {
                ImGui::SetNextWindowDockID(dockingLayout.value().centerTop);
            }

            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F)
            );
            if (ImGui::Begin("SceneViewport"))
            {
                VkExtent2D const sceneContentExtent{
                    .width =
                        static_cast<uint32_t>(ImGui::GetContentRegionAvail().x),
                    .height =
                        static_cast<uint32_t>(ImGui::GetContentRegionAvail().y),
                };

                m_sceneRect = VkRect2D{
                    .offset{VkOffset2D{
                        .x = 0,
                        .y = 0,
                    }},
                    .extent{sceneContentExtent},
                };

                ImVec2 const uvMax{
                    static_cast<float>(m_sceneRect.extent.width)
                        / static_cast<float>(
                            m_sceneColorTexture.imageExtent.width
                        ),
                    static_cast<float>(m_sceneRect.extent.height)
                        / static_cast<float>(
                            m_sceneColorTexture.imageExtent.height
                        )
                };

                ImGui::Image(
                    (ImTextureID)m_imguiSceneTextureDescriptor,
                    ImGui::GetContentRegionAvail(),
                    ImVec2{0.0, 0.0},
                    uvMax,
                    ImVec4{1.0F, 1.0F, 1.0F, 1.0F},
                    ImVec4{0.0F, 0.0F, 0.0F, 0.0F}
                );
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }
    }

    ImGui::Render();
    return skipFrame;
}

void Engine::tickWorld(TickTiming timing)
{
    std::span<glm::mat4x4> const models{m_meshInstances.models->mapValidStaged()
    };
    std::span<glm::mat4x4> const modelInverseTransposes{
        m_meshInstances.modelInverseTransposes->mapValidStaged()
    };

    if (models.size() != modelInverseTransposes.size())
    {
        Warning("models and modelInverseTransposes out of sync");
        return;
    }

    size_t index{0};
    for (glm::mat4x4 const& modelOriginal : m_meshInstances.originals)
    {
        if (index >= m_meshInstances.dynamicIndex)
        {
            glm::vec4 const position{
                modelOriginal * glm::vec4(0.0, 0.0, 0.0, 1.0)
            };

            double const timeOffset{
                (position.x - (-10) + position.z - (-10)) / 3.1415
            };

            double const y{std::sin(timing.timeElapsed + timeOffset)};

            glm::mat4x4 const translation{glm::translate(glm::vec3(0.0, y, 0.0))
            };

            models[index] = translation * modelOriginal;

            // In general, the model inverse transposes only need to be updated
            // once per tick, before rendering and after the last update of the
            // model matrices. For now, we only update once per tick, so we
            // just compute it here.
            modelInverseTransposes[index] =
                glm::inverseTranspose(models[index]);
        }
        index += 1;
    }

    // Atmosphere
    {
        AtmosphereParameters::AnimationParameters const atmosphereAnimation{
            m_atmosphereParameters.animation
        };
        if (atmosphereAnimation.animateSun)
        {
            float const time{
                // position of sun as proxy for time
                glm::dot(geometry::up, m_atmosphereParameters.directionToSun())
            };

            bool const isNight{time < -0.11F};
            float const sunriseAngle{glm::asin(0.1F)};

            if (isNight && atmosphereAnimation.skipNight)
            {
                if (atmosphereAnimation.animationSpeed > 0.0)
                {
                    m_atmosphereParameters.sunEulerAngles.x =
                        glm::pi<float>() - sunriseAngle;
                }
                else
                {
                    m_atmosphereParameters.sunEulerAngles.x = sunriseAngle;
                }
            }
            else
            {
                m_atmosphereParameters.sunEulerAngles.x +=
                    static_cast<float>(timing.deltaTimeSeconds)
                    * atmosphereAnimation.animationSpeed;
            }

            m_atmosphereParameters.sunEulerAngles = glm::mod(
                m_atmosphereParameters.sunEulerAngles,
                glm::vec3(glm::two_pi<float>())
            );
        }
    }
}

void Engine::draw()
{
    FrameData& currentFrame = getCurrentFrame();

    uint64_t constexpr FRAME_WAIT_TIMEOUT_NANOSECONDS = 1'000'000'000;
    CheckVkResult(vkWaitForFences(
        m_device,
        1,
        &currentFrame.renderFence,
        VK_TRUE,
        FRAME_WAIT_TIMEOUT_NANOSECONDS
    ));

    currentFrame.deletionQueue.flush();

    CheckVkResult(vkResetFences(m_device, 1, &currentFrame.renderFence));

    VkCommandBuffer const& cmd = currentFrame.mainCommandBuffer;
    CheckVkResult(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo const cmdBeginInfo{vkinit::commandBufferBeginInfo(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    )};
    CheckVkResult(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // Begin scene drawing

    { // Copy cameras to gpu
        float const aspectRatio{
            static_cast<float>(vkutil::aspectRatio(m_sceneRect.extent))
        };

        float const orthoDistanceFromCamera{5.0F};

        std::span<gputypes::Camera> const cameras{
            m_camerasBuffer->mapValidStaged()
        };
        cameras[m_cameraIndexMain] = {
            m_useOrthographicProjection
                ? m_cameraParameters.toDeviceEquivalentOrthographic(
                    aspectRatio, orthoDistanceFromCamera
                )
                : m_cameraParameters.toDeviceEquivalent(aspectRatio)
        };

        m_camerasBuffer->recordCopyToDevice(cmd, m_allocator);
    }

    { // Copy atmospheres to gpu
        std::span<gputypes::Atmosphere> const stagedAtmospheres{
            m_atmospheresBuffer->mapValidStaged()
        };
        if (stagedAtmospheres.size() <= m_atmosphereIndex)
        {
            Warning("AtmosphereIndex does not point to valid atmosphere, "
                    "resetting to 0.");
            m_atmosphereIndex = 0;
        }
        if (!stagedAtmospheres.empty()
            && m_atmosphereIndex < stagedAtmospheres.size())
        {
            stagedAtmospheres[m_atmosphereIndex] =
                m_atmosphereParameters.toDeviceEquivalent();
        }

        m_atmospheresBuffer->recordCopyToDevice(cmd, m_allocator);
    }

    { // Copy models to gpu
        m_meshInstances.models->recordCopyToDevice(cmd, m_allocator);
        m_meshInstances.modelInverseTransposes->recordCopyToDevice(
            cmd, m_allocator
        );
    }

    {
        vkutil::transitionImage(
            cmd,
            m_sceneColorTexture.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        switch (m_activeRenderingPipeline)
        {
        case RenderingPipelines::DEFERRED:
        {
            std::vector<gputypes::LightDirectional> directionalLights{};
            std::span<gputypes::Atmosphere const> const atmospheres{
                m_atmospheresBuffer->readValidStaged()
            };
            if (m_atmosphereIndex < atmospheres.size())
            {
                gputypes::Atmosphere const atmosphere{
                    atmospheres[m_atmosphereIndex]
                };

                float const sunCosine{
                    // position of sun as proxy for time
                    glm::dot(geometry::up, atmosphere.directionToSun)
                };
                if (sunCosine > 0.0)
                { // Sunlight
                    float constexpr SUNLIGHT_STRENGTH{0.5F};

                    directionalLights.push_back(lights::makeDirectional(
                        glm::vec4(atmosphere.sunlightColor, 1.0),
                        SUNLIGHT_STRENGTH,
                        m_atmosphereParameters.sunEulerAngles,
                        m_sceneBounds.center,
                        m_sceneBounds.extent
                    ));
                }

                float constexpr SUNSET_COSINE{0.06};

                if (sunCosine < SUNSET_COSINE)
                { // Moonlight
                    float constexpr MOONRISE_LENGTH{0.08};

                    float const moonlightStrength{
                        0.1F
                        * glm::clamp(
                            0.0F,
                            1.0F,
                            glm::abs(sunCosine - SUNSET_COSINE)
                                / MOONRISE_LENGTH
                        )
                    };

                    glm::vec4 constexpr MOONLIGHT_COLOR_RGBA{
                        0.3, 0.4, 0.6, 1.0
                    };
                    glm::vec3 constexpr STRAIGHT_DOWN_EULER_ANGLES{
                        -glm::half_pi<float>(), 0.0F, 0.0F
                    };

                    directionalLights.push_back(lights::makeDirectional(
                        MOONLIGHT_COLOR_RGBA,
                        moonlightStrength,
                        STRAIGHT_DOWN_EULER_ANGLES,
                        m_sceneBounds.center,
                        m_sceneBounds.extent
                    ));
                }
            }
            else
            {
                glm::vec4 constexpr WHITE_RGBA{1.0F};
                float constexpr STRENGTH{1.0F};
                glm::vec3 constexpr STRAIGHT_DOWN_EULER_ANGLES{
                    -glm::half_pi<float>(), 0.0F, 0.0F
                };

                directionalLights.push_back(lights::makeDirectional(
                    WHITE_RGBA,
                    STRENGTH,
                    STRAIGHT_DOWN_EULER_ANGLES,
                    m_sceneBounds.center,
                    m_sceneBounds.extent
                ));
            }

            std::vector<gputypes::LightSpot> const spotLights{
                lights::makeSpot(
                    glm::vec4(0.0, 1.0, 0.0, 1.0),
                    30.0,
                    1.0,
                    1.0,
                    60,
                    1.0,
                    glm::vec3(-1.0, 0.0, 1.0),
                    glm::vec3(-8.0, -10.0, -2.0),
                    0.1,
                    1000.0
                ),
                lights::makeSpot(
                    glm::vec4(1.0, 0.0, 0.0, 1.0),
                    30.0,
                    1.0,
                    1.0,
                    60,
                    1.0,
                    glm::vec3(-1.0, 0.0, -1.0),
                    glm::vec3(8.0, -10.0, 2.0),
                    0.1,
                    1000.0
                ),
            };

            m_deferredShadingPipeline->recordDrawCommands(
                cmd,
                m_sceneRect,
                VK_IMAGE_LAYOUT_GENERAL,
                m_sceneColorTexture,
                m_sceneDepthTexture,
                directionalLights,
                m_showSpotlights ? spotLights
                                 : std::vector<gputypes::LightSpot>{},
                m_cameraIndexMain,
                *m_camerasBuffer,
                m_atmosphereIndex,
                *m_atmospheresBuffer,
                m_renderMeshInstances,
                *m_testMeshes[m_testMeshUsed],
                m_meshInstances
            );

            m_debugLines.pushBox(
                m_sceneBounds.center,
                glm::quat_identity<float, glm::qualifier::defaultp>(),
                m_sceneBounds.extent
            );
            recordDrawDebugLines(cmd, m_cameraIndexMain, *m_camerasBuffer);

            break;
        }
        case RenderingPipelines::COMPUTE_COLLECTION:
        {
            m_genericComputePipeline->recordDrawCommands(
                cmd, m_sceneTextureDescriptors, m_sceneRect.extent
            );

            break;
        }
        }
    }

    // End scene drawing

    // ImGui Drawing

    vkutil::transitionImage(
        cmd,
        m_sceneColorTexture.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    vkutil::transitionImage(
        cmd,
        m_drawImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    recordDrawImgui(cmd, m_drawImage.imageView);

    vkutil::transitionImage(
        cmd,
        m_drawImage.image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    // End ImGui Drawing

    // Copy image to swapchain

    uint32_t swapchainImageIndex;
    VkResult const acquireResult{vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        FRAME_WAIT_TIMEOUT_NANOSECONDS,
        currentFrame.swapchainSemaphore,
        VK_NULL_HANDLE // No Fence to signal
        ,
        &swapchainImageIndex
    )};
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_resizeRequested = true;
        CheckVkResult(vkEndCommandBuffer(cmd));
        return;
    }
    CheckVkResult(acquireResult);

    VkImage const& swapchainImage{m_swapchainImages[swapchainImageIndex]};
    VkImageView const& swapchainImageView{
        m_swapchainImageViews[swapchainImageIndex]
    };

    vkutil::transitionImage(
        cmd,
        m_drawImage.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    vkutil::transitionImage(
        cmd,
        swapchainImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    vkutil::recordCopyImageToImage(
        cmd,
        m_drawImage.image,
        swapchainImage,
        m_drawRect,
        VkRect2D{.extent{m_swapchainExtent}}
    );

    vkutil::transitionImage(
        cmd,
        swapchainImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    CheckVkResult(vkEndCommandBuffer(cmd));

    // Submit commands

    VkCommandBufferSubmitInfo const cmdSubmitInfo{
        vkinit::commandBufferSubmitInfo(cmd)
    };
    VkSemaphoreSubmitInfo const waitInfo{vkinit::semaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        currentFrame.swapchainSemaphore
    )};
    VkSemaphoreSubmitInfo const signalInfo{vkinit::semaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currentFrame.renderSemaphore
    )};

    std::vector<VkCommandBufferSubmitInfo> const cmdSubmitInfos{cmdSubmitInfo};
    std::vector<VkSemaphoreSubmitInfo> const waitInfos{waitInfo};
    std::vector<VkSemaphoreSubmitInfo> const signalInfos{signalInfo};
    VkSubmitInfo2 const submitInfo =
        vkinit::submitInfo(cmdSubmitInfos, waitInfos, signalInfos);

    CheckVkResult(vkQueueSubmit2(
        m_graphicsQueue, 1, &submitInfo, currentFrame.renderFence
    ));

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

    VkResult const presentResult{
        vkQueuePresentKHR(m_graphicsQueue, &presentInfo)
    };
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

void Engine::recordDrawImgui(VkCommandBuffer const cmd, VkImageView const view)
{
    VkRenderingAttachmentInfo const colorAttachmentInfo{
        vkinit::renderingAttachmentInfo(view, VK_IMAGE_LAYOUT_GENERAL)
    };

    std::vector<VkRenderingAttachmentInfo> const colorAttachments{
        colorAttachmentInfo
    };
    VkRenderingInfo const renderingInfo{vkinit::renderingInfo(
        VkRect2D{.extent{m_swapchainExtent}}, colorAttachments, nullptr
    )};

    vkCmdBeginRendering(cmd, &renderingInfo);

    ImDrawData* const drawData{ImGui::GetDrawData()};

    m_drawRect = VkRect2D{
        .offset{VkOffset2D{
            .x = static_cast<int32_t>(drawData->DisplayPos.x),
            .y = static_cast<int32_t>(drawData->DisplayPos.y),
        }},
        .extent{VkExtent2D{
            .width = static_cast<uint32_t>(drawData->DisplaySize.x),
            .height = static_cast<uint32_t>(drawData->DisplaySize.y),
        }},
    };

    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

    vkCmdEndRendering(cmd);
}

void Engine::recordDrawDebugLines(
    VkCommandBuffer const cmd,
    uint32_t const cameraIndex,
    TStagedBuffer<gputypes::Camera> const& camerasBuffer
)
{
    m_debugLines.lastFrameDrawResults = {};

    if (m_debugLines.enabled && m_debugLines.indices->stagedSize() > 0)
    {
        m_debugLines.recordCopy(cmd, m_allocator);

        DrawResultsGraphics const drawResults{
            m_debugLines.pipeline->recordDrawCommands(
                cmd,
                false,
                m_debugLines.lineWidth,
                m_sceneRect,
                m_sceneColorTexture,
                m_sceneDepthTexture,
                cameraIndex,
                camerasBuffer,
                *m_debugLines.vertices,
                *m_debugLines.indices
            )
        };

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
    vkDestroySampler(m_device, m_imguiSceneTextureSampler, nullptr);

    m_genericComputePipeline->cleanup(m_device);
    m_deferredShadingPipeline->cleanup(m_device, m_allocator);

    m_meshInstances.models.reset();
    m_meshInstances.modelInverseTransposes.reset();

    m_atmospheresBuffer.reset();
    m_camerasBuffer.reset();

    m_testMeshes.clear();
    m_debugLines.cleanup(m_device, m_allocator);

    m_globalDescriptorAllocator.destroyPool(m_device);

    vkDestroyDescriptorSetLayout(
        m_device, m_sceneTextureDescriptorLayout, nullptr
    );

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

    m_initialized = false;
}
