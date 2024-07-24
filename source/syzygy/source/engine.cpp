#include "engine.hpp"

#include "vulkanusage.hpp"

#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <implot.h>

#include "descriptors.hpp"
#include "helpers.hpp"
#include "images.hpp"
#include "initializers.hpp"
#include "lights.hpp"
#include "pipelines.hpp"

#include "ui/pipelineui.hpp"

#define VKRENDERER_COMPILE_WITH_TESTING 0

Engine::Engine(
    PlatformWindow const& window,
    VkInstance const instance,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const generalQueue,
    uint32_t const generalQueueFamilyIndex
)
{
    init(
        window,
        instance,
        physicalDevice,
        device,
        allocator,
        generalQueue,
        generalQueueFamilyIndex
    );
}

auto Engine::loadEngine(
    PlatformWindow const& window,
    VkInstance const instance,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const generalQueue,
    uint32_t const generalQueueFamilyIndex
) -> Engine*
{
    if (m_loadedEngine == nullptr)
    {
        Log("Loading Engine.");
        m_loadedEngine = new Engine(
            window,
            instance,
            physicalDevice,
            device,
            allocator,
            generalQueue,
            generalQueueFamilyIndex
        );
    }
    else
    {
        Warning("Called loadEngine when one was already loaded. No new engine "
                "was loaded.");
    }

    return m_loadedEngine;
}

void Engine::init(
    PlatformWindow const& window,
    VkInstance const instance,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VmaAllocator const allocator,
    VkQueue const generalQueue,
    uint32_t const generalQueueFamilyIndex
)
{
    Log("Initializing Engine...");

    initDrawTargets(device, allocator);

    initDescriptors(device);

    updateDescriptors(device);

    initWorld(device, allocator);
    initDebug(device, allocator);
    initGenericComputePipelines(device);

    initDeferredShadingPipeline(device, allocator);

    initImgui(
        instance,
        physicalDevice,
        device,
        generalQueueFamilyIndex,
        generalQueue,
        window.handle()
    );

    Log("Vulkan Initialized.");

    m_initialized = true;

    Log("Engine Initialized.");
}

void Engine::initDrawTargets(
    VkDevice const device, VmaAllocator const allocator
)
{
    // Initialize the image used for rendering outside of the swapchain.

    VkExtent2D constexpr RESERVED_IMAGE_EXTENT{
        MAX_DRAW_EXTENTS.width, MAX_DRAW_EXTENTS.height
    };
    VkFormat constexpr COLOR_FORMAT{VK_FORMAT_R16G16B16A16_SFLOAT};
    VkImageAspectFlags constexpr COLOR_ASPECTS{VK_IMAGE_ASPECT_COLOR_BIT};

    if (std::optional<AllocatedImage> sceneColorResult{AllocatedImage::allocate(
            allocator,
            device,
            AllocatedImage::AllocationParameters{
                .extent = RESERVED_IMAGE_EXTENT,
                .format = COLOR_FORMAT,
                .usageFlags =
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                    | VK_IMAGE_USAGE_SAMPLED_BIT // used as descriptor for
                                                 // e.g.
                                                 // ImGui
                    | VK_IMAGE_USAGE_STORAGE_BIT // used in compute passes
                    | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT // used in
                    // graphics
                    // passes
                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // copy to from other
                // render passes
                .viewFlags = COLOR_ASPECTS,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            }
        )};
        sceneColorResult.has_value())
    {
        m_sceneColorTexture =
            std::make_unique<AllocatedImage>(std::move(sceneColorResult).value()
            );
    }
    else
    {
        Warning("Failed to allocate scene color texture.");
    }

    if (std::optional<AllocatedImage> drawImageResult{AllocatedImage::allocate(
            allocator,
            device,
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
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            }
        )};
        drawImageResult.has_value())
    {
        m_drawImage =
            std::make_unique<AllocatedImage>(std::move(drawImageResult).value()
            );
    }
    else
    {
        Warning("Failed to allocate total draw image.");
    }

    if (std::optional<AllocatedImage> sceneDepthResult{AllocatedImage::allocate(
            allocator,
            device,
            AllocatedImage::AllocationParameters{
                .extent = RESERVED_IMAGE_EXTENT,
                .format = VK_FORMAT_D32_SFLOAT,
                .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .viewFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            }
        )};
        sceneDepthResult.has_value())
    {
        m_sceneDepthTexture =
            std::make_unique<AllocatedImage>(std::move(sceneDepthResult).value()
            );
    }
    else
    {
        Warning("Failed to allocate scene depth texture.");
    }
}

void Engine::initDescriptors(VkDevice const device)
{
    std::vector<DescriptorAllocator::PoolSizeRatio> const sizes{
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0.5F},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0.5F}
    };

    m_globalDescriptorAllocator.initPool(
        device,
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
                .build(device, 0)
                .value_or(VK_NULL_HANDLE); // TODO: handle

        m_sceneTextureDescriptors = m_globalDescriptorAllocator.allocate(
            device, m_sceneTextureDescriptorLayout
        );
    }
}

void Engine::updateDescriptors(VkDevice const device)
{
    VkDescriptorImageInfo const sceneTextureInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = m_sceneColorTexture->view(),
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

    vkUpdateDescriptorSets(device, VKR_ARRAY(writes), VKR_ARRAY_NONE);
}

void Engine::initWorld(VkDevice const device, VmaAllocator const allocator)
{
    m_camerasBuffer = std::make_unique<TStagedBuffer<gputypes::Camera>>(
        TStagedBuffer<gputypes::Camera>::allocate(
            device,
            allocator,
            CAMERA_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        )
    );
    m_atmospheresBuffer = std::make_unique<TStagedBuffer<gputypes::Atmosphere>>(
        TStagedBuffer<gputypes::Atmosphere>::allocate(
            device,
            allocator,
            ATMOSPHERE_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        )
    );
}

void Engine::initDebug(VkDevice const device, VmaAllocator const allocator)
{
    m_debugLines.pipeline = std::make_unique<DebugLineGraphicsPipeline>(
        device,
        DebugLineGraphicsPipeline::ImageFormats{
            .color = m_sceneColorTexture->format(),
            .depth = m_sceneDepthTexture->format(),
        }
    );
    m_debugLines.indices = std::make_unique<TStagedBuffer<uint32_t>>(
        TStagedBuffer<uint32_t>::allocate(
            device,
            allocator,
            DEBUGLINES_CAPACITY,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        )
    );
    m_debugLines.vertices =
        std::make_unique<TStagedBuffer<Vertex>>(TStagedBuffer<Vertex>::allocate(
            device,
            allocator,
            DEBUGLINES_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ));
}

void Engine::initDeferredShadingPipeline(
    VkDevice const device, VmaAllocator const allocator
)
{
    m_deferredShadingPipeline = std::make_unique<DeferredShadingPipeline>(
        device, allocator, m_globalDescriptorAllocator, MAX_DRAW_EXTENTS
    );

    m_deferredShadingPipeline->updateRenderTargetDescriptors(
        device, *m_sceneDepthTexture
    );
}

void Engine::initGenericComputePipelines(VkDevice const device)
{
    std::vector<std::string> const shaderPaths{
        "shaders/booleanpush.comp.spv",
        "shaders/gradient_color.comp.spv",
        "shaders/sparse_push_constant.comp.spv",
        "shaders/matrix_color.comp.spv"
    };
    m_genericComputePipeline = std::make_unique<ComputeCollectionPipeline>(
        device, m_sceneTextureDescriptorLayout, shaderPaths
    );
}

void Engine::initImgui(
    VkInstance const instance,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    uint32_t graphicsQueueFamily,
    VkQueue graphicsQueue,
    GLFWwindow* const window
)
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
    CheckVkResult(
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiDescriptorPool)
    );

    ImGui::CreateContext();
    ImPlot::CreateContext();

    std::vector<VkFormat> const colorAttachmentFormats{m_drawImage->format()};
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
        const_cast<VkInstance*>(&instance)
    );

    // This amount is recommended by ImGui to satisfy validation layers, even if
    // a little wasteful
    VkDeviceSize constexpr IMGUI_MIN_ALLOCATION_SIZE{1024ULL * 1024ULL};

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,

        .QueueFamily = graphicsQueueFamily,
        .Queue = graphicsQueue,

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
            device, &samplerInfo, nullptr, &m_imguiSceneTextureSampler
        );

        m_imguiSceneTextureDescriptor = ImGui_ImplVulkan_AddTexture(
            m_imguiSceneTextureSampler,
            m_sceneColorTexture->view(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    Log("ImGui initialized.");
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

auto Engine::uiBegin(
    UIPreferences& currentPreferences, UIPreferences const& defaultPreferences
) -> UIResults
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    HUDState const hud{renderHUD(currentPreferences)};

    bool const reloadUI{
        hud.applyPreferencesRequested || hud.resetPreferencesRequested
    };
    if (hud.resetPreferencesRequested)
    {
        currentPreferences = defaultPreferences;
    }
    DockingLayout dockingLayout{};

    if (hud.rebuildLayoutRequested && hud.dockspaceID != 0)
    {
        dockingLayout =
            buildDefaultMultiWindowLayout(hud.workArea, hud.dockspaceID);
    }

    return UIResults{
        .hud = hud,
        .dockingLayout = dockingLayout,
        .reloadRequested = reloadUI,
    };
}

void Engine::uiRenderOldWindows(
    HUDState const& hud, DockingLayout const& dockingLayout
)
{
    if (std::optional<ui::RenderTarget> sceneViewport{ui::sceneViewport(
            m_imguiSceneTextureDescriptor,
            m_sceneColorTexture->extent2D(),
            hud.maximizeSceneViewport ? hud.workArea
                                      : std::optional<UIRectangle>{},
            dockingLayout.centerTop
        )};
        sceneViewport.has_value())
    {
        glm::vec2 const pixelExtent{sceneViewport.value().extent};

        VkExtent2D const sceneContentExtent{
            .width = static_cast<uint32_t>(pixelExtent.x),
            .height = static_cast<uint32_t>(pixelExtent.y),
        };

        m_sceneRect = VkRect2D{
            .offset{VkOffset2D{
                .x = 0,
                .y = 0,
            }},
            .extent{sceneContentExtent},
        };
    }

    if (UIWindow const sceneControls{UIWindow::beginDockable(
            "Scene Controls (LEGACY)", dockingLayout.left
        )};
        sceneControls.open)
    {
        ImGui::Separator();
        imguiStructureControls(m_sceneBounds, DEFAULT_SCENE_BOUNDS);
    }

    if (UIWindow const engineControls{
            UIWindow::beginDockable("Engine Controls", dockingLayout.right)
        };
        engineControls.open)
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
}

void Engine::uiEnd() { ImGui::Render(); }

void Engine::tickWorld(TickTiming const /*timing*/) { m_debugLines.clear(); }

auto Engine::recordDraw(VkCommandBuffer const cmd, scene::Scene const& scene)
    -> DrawResults
{
    // Begin scene drawing

    { // Copy cameras to gpu
        float const aspectRatio{
            static_cast<float>(vkutil::aspectRatio(m_sceneRect.extent))
        };

        gputypes::Camera const mainCamera{
            scene.camera.toDeviceEquivalent(aspectRatio)
        };

        m_camerasBuffer->clearStaged();
        m_camerasBuffer->push(mainCamera);
        m_camerasBuffer->recordCopyToDevice(cmd);
    }

    std::vector<gputypes::LightDirectional> directionalLights{};
    { // Copy atmospheres to gpu
        scene::AtmosphereBaked const bakedAtmosphere{
            scene.atmosphere.baked(m_sceneBounds)
        };
        if (bakedAtmosphere.moonlight.has_value())
        {
            directionalLights.push_back(bakedAtmosphere.moonlight.value());
        }
        if (bakedAtmosphere.sunlight.has_value())
        {
            directionalLights.push_back(bakedAtmosphere.sunlight.value());
        }

        m_atmospheresBuffer->clearStaged();
        m_atmospheresBuffer->push(bakedAtmosphere.atmosphere);
        m_atmospheresBuffer->recordCopyToDevice(cmd);
    }

    { // Copy models to gpu
        scene.geometry.models->recordCopyToDevice(cmd);
        scene.geometry.modelInverseTransposes->recordCopyToDevice(cmd);
    }

    {
        m_sceneColorTexture->recordTransitionBarriered(
            cmd, VK_IMAGE_LAYOUT_GENERAL
        );

        switch (m_activeRenderingPipeline)
        {
        case RenderingPipelines::DEFERRED:
        {
            // TODO: create a struct that contains a ref to a struct in a
            // buffer
            uint32_t const cameraIndex{0};
            uint32_t const atmosphereIndex{0};

            m_deferredShadingPipeline->recordDrawCommands(
                cmd,
                m_sceneRect,
                *m_sceneColorTexture,
                *m_sceneDepthTexture,
                directionalLights,
                scene.spotlightsRender ? scene.spotlights
                                       : std::vector<gputypes::LightSpot>{},
                cameraIndex,
                *m_camerasBuffer,
                atmosphereIndex,
                *m_atmospheresBuffer,
                scene.geometry
            );

            m_sceneColorTexture->recordTransitionBarriered(
                cmd, VK_IMAGE_LAYOUT_GENERAL
            );

            m_debugLines.pushBox(
                m_sceneBounds.center,
                glm::quat_identity<float, glm::qualifier::defaultp>(),
                m_sceneBounds.extent
            );
            recordDrawDebugLines(cmd, cameraIndex, *m_camerasBuffer);

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

    m_sceneColorTexture->recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    m_drawImage->recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

    VkRect2D const renderedArea{recordDrawImgui(cmd, m_drawImage->view())};

    // TODO: is this line necessary?
    m_drawImage->recordTransitionBarriered(cmd, VK_IMAGE_LAYOUT_GENERAL);

    // End ImGui Drawing

    return DrawResults{
        .renderTarget = *m_drawImage,
        .renderArea = renderedArea,
    };
}

auto Engine::recordDrawImgui(VkCommandBuffer const cmd, VkImageView const view)
    -> VkRect2D
{
    ImDrawData* const drawData{ImGui::GetDrawData()};

    VkRect2D renderedArea{
        .offset{VkOffset2D{
            .x = static_cast<int32_t>(drawData->DisplayPos.x),
            .y = static_cast<int32_t>(drawData->DisplayPos.y),
        }},
        .extent{VkExtent2D{
            .width = static_cast<uint32_t>(drawData->DisplaySize.x),
            .height = static_cast<uint32_t>(drawData->DisplaySize.y),
        }},
    };

    VkRenderingAttachmentInfo const colorAttachmentInfo{
        vkinit::renderingAttachmentInfo(view, VK_IMAGE_LAYOUT_GENERAL)
    };
    std::vector<VkRenderingAttachmentInfo> const colorAttachments{
        colorAttachmentInfo
    };
    VkRenderingInfo const renderingInfo{
        vkinit::renderingInfo(renderedArea, colorAttachments, nullptr)
    };
    vkCmdBeginRendering(cmd, &renderingInfo);

    ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

    vkCmdEndRendering(cmd);

    return renderedArea;
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
        m_debugLines.recordCopy(cmd);

        DrawResultsGraphics const drawResults{
            m_debugLines.pipeline->recordDrawCommands(
                cmd,
                false,
                m_debugLines.lineWidth,
                m_sceneRect,
                *m_sceneColorTexture,
                *m_sceneDepthTexture,
                cameraIndex,
                camerasBuffer,
                *m_debugLines.vertices,
                *m_debugLines.indices
            )
        };

        m_debugLines.lastFrameDrawResults = drawResults;
    }
}

void Engine::cleanup(VkDevice const device, VmaAllocator const allocator)
{
    if (!m_initialized)
    {
        return;
    }

    Log("Engine cleaning up.");

    CheckVkResult(vkDeviceWaitIdle(device));

    ImPlot::DestroyContext();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device, m_imguiDescriptorPool, nullptr);
    vkDestroySampler(device, m_imguiSceneTextureSampler, nullptr);

    m_genericComputePipeline->cleanup(device);
    m_deferredShadingPipeline->cleanup(device, allocator);

    m_atmospheresBuffer.reset();
    m_camerasBuffer.reset();

    m_debugLines.cleanup(device, allocator);

    m_globalDescriptorAllocator.destroyPool(device);

    vkDestroyDescriptorSetLayout(
        device, m_sceneTextureDescriptorLayout, nullptr
    );

    m_sceneDepthTexture.reset();
    m_sceneColorTexture.reset();
    m_drawImage.reset();

    m_initialized = false;

    Log("Engine cleaned up.");
}
