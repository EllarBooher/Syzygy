#include "editor.hpp"

#include "syzygy/assets.hpp"
#include "syzygy/core/immediate.hpp"
#include "syzygy/core/input.hpp"
#include "syzygy/core/integer.hpp"
#include "syzygy/core/result.hpp"
#include "syzygy/core/scene.hpp"
#include "syzygy/core/scenetexture.hpp"
#include "syzygy/core/timing.hpp"
#include "syzygy/editor/framebuffer.hpp"
#include "syzygy/editor/graphicscontext.hpp"
#include "syzygy/editor/swapchain.hpp"
#include "syzygy/editor/window.hpp"
#include "syzygy/engine.hpp"
#include "syzygy/enginetypes.hpp"
#include "syzygy/helpers.hpp"
#include "syzygy/images/image.hpp"
#include "syzygy/images/imageoperations.hpp"
#include "syzygy/images/imageview.hpp"
#include "syzygy/initializers.hpp"
#include "syzygy/ui/dockinglayout.hpp"
#include "syzygy/ui/hud.hpp"
#include "syzygy/ui/texturedisplay.hpp"
#include "syzygy/ui/uirectangle.hpp"
#include "syzygy/ui/widgets.hpp"
#include "syzygy/vulkanusage.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fmt/core.h>
#include <glm/vec2.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <implot.h>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <vector>

struct UIResults
{
    ui::HUDState hud;
    ui::DockingLayout dockingLayout;
    bool reloadRequested;
};

struct EditorResources
{
    PlatformWindow window;
    GraphicsContext graphics;
    Swapchain swapchain;
    FrameBuffer frameBuffer;
};

namespace
{
auto initialize() -> std::optional<EditorResources>
{
    Log("Initializing Editor resources.");

    if (glfwInit() == GLFW_FALSE)
    {
        Error("Failed to initialize GLFW.");
        return std::nullopt;
    }

    Log("Creating window...");

    glm::u16vec2 constexpr DEFAULT_WINDOW_EXTENT{1920, 1080};

    std::optional<PlatformWindow> windowResult{
        PlatformWindow::create(DEFAULT_WINDOW_EXTENT)
    };
    if (!windowResult.has_value())
    {
        Error("Failed to create window.");
        return std::nullopt;
    }

    Log("Window created.");

    Log("Creating Graphics Context...");

    std::optional<GraphicsContext> graphicsResult{
        GraphicsContext::create(windowResult.value())
    };
    if (!graphicsResult.has_value())
    {
        Error("Failed to create graphics context.");
        return std::nullopt;
    }
    VulkanContext const& vulkanContext{graphicsResult.value().vulkanContext()};

    Log("Created Graphics Context.");

    Log("Creating Swapchain...");

    std::optional<Swapchain> swapchainResult{Swapchain::create(
        vulkanContext.physicalDevice,
        vulkanContext.device,
        vulkanContext.surface,
        windowResult.value().extent(),
        std::optional<VkSwapchainKHR>{}
    )};
    if (!swapchainResult.has_value())
    {
        Error("Failed to create swapchain.");
        return std::nullopt;
    }

    Log("Created Swapchain.");

    Log("Creating Frame Buffer...");

    std::optional<FrameBuffer> frameBufferResult{FrameBuffer::create(
        vulkanContext.device, vulkanContext.graphicsQueueFamily
    )};
    if (!frameBufferResult.has_value())
    {
        Error("Failed to create FrameBuffer.");
        return std::nullopt;
    }

    Log("Created Frame Buffer.");

    Log("Successfully initialized Editor resources.");

    return EditorResources{
        .window = std::move(windowResult).value(),
        .graphics = std::move(graphicsResult).value(),
        .swapchain = std::move(swapchainResult).value(),
        .frameBuffer = std::move(frameBufferResult).value(),
    };
}
auto defaultRefreshRate() -> float
{
    // Guess that the window is on the primary monitor, as a guess for refresh
    // rate to use
    GLFWvidmode const* const videoModePrimary{
        glfwGetVideoMode(glfwGetPrimaryMonitor())
    };
    return static_cast<float>(videoModePrimary->refreshRate);
}
auto rebuildSwapchain(
    Swapchain& old,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VkSurfaceKHR const surface,
    glm::u16vec2 const newExtent
) -> std::optional<Swapchain>
{
    Log(fmt::format(
        "Resizing swapchain: ({},{}) -> ({},{})",
        old.extent().width,
        old.extent().height,
        newExtent.x,
        newExtent.y
    ));

    std::optional<Swapchain> newSwapchain{Swapchain::create(
        physicalDevice, device, surface, newExtent, old.swapchain()
    )};

    return newSwapchain;
}
auto beginFrame(Frame const& currentFrame, VkDevice const device) -> VkResult
{
    uint64_t constexpr FRAME_WAIT_TIMEOUT_NANOSECONDS = 1'000'000'000;
    if (VkResult const waitResult{vkWaitForFences(
            device,
            1,
            &currentFrame.renderFence,
            VK_TRUE,
            FRAME_WAIT_TIMEOUT_NANOSECONDS
        )};
        waitResult != VK_SUCCESS)
    {
        LogVkResult(waitResult, "Failed to wait on frame in-use fence.");
        return waitResult;
    }

    if (VkResult const resetResult{
            vkResetFences(device, 1, &currentFrame.renderFence)
        };
        resetResult != VK_SUCCESS)
    {
        LogVkResult(resetResult, "Failed to reset frame fences.");
        return resetResult;
    }

    VkCommandBuffer const& cmd = currentFrame.mainCommandBuffer;

    if (VkResult const resetCmdResult{vkResetCommandBuffer(cmd, 0)};
        resetCmdResult != VK_SUCCESS)
    {
        LogVkResult(resetCmdResult, "Failed to reset frame command buffer.");
        return resetCmdResult;
    }

    VkCommandBufferBeginInfo const cmdBeginInfo{vkinit::commandBufferBeginInfo(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    )};
    if (VkResult const beginCmdResult{vkBeginCommandBuffer(cmd, &cmdBeginInfo)};
        beginCmdResult != VK_SUCCESS)
    {
        LogVkResult(beginCmdResult, "Failed to begin frame command buffer.");
        return beginCmdResult;
    }

    return VK_SUCCESS;
}

auto endFrame(
    Frame const& currentFrame,
    Swapchain& swapchain,
    VkDevice const device,
    VkQueue const submissionQueue,
    VkCommandBuffer const cmd,
    szg_image::Image& sourceTexture,
    VkRect2D const sourceSubregion
) -> VkResult
{
    // Copy image to swapchain
    uint64_t constexpr ACQUIRE_TIMEOUT_NANOSECONDS = 1'000'000'000;

    uint32_t swapchainImageIndex{std::numeric_limits<uint32_t>::max()};

    if (VkResult const acquireResult{vkAcquireNextImageKHR(
            device,
            swapchain.swapchain(),
            ACQUIRE_TIMEOUT_NANOSECONDS,
            currentFrame.swapchainSemaphore,
            VK_NULL_HANDLE // No Fence to signal
            ,
            &swapchainImageIndex
        )};
        acquireResult != VK_SUCCESS)
    {
        if (acquireResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            LogVkResult(acquireResult, "Failed to acquire swapchain image.");
        }
        LogVkResult(vkEndCommandBuffer(cmd), "Failed to end command buffer.");
        return acquireResult;
    }

    VkImage const swapchainImage{swapchain.images()[swapchainImageIndex]};

    sourceTexture.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT
    );

    szg_image::transitionImage(
        cmd,
        swapchainImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    szg_image::recordCopyImageToImage(
        cmd,
        sourceTexture.image(),
        swapchainImage,
        sourceSubregion,
        VkRect2D{.extent{swapchain.extent()}}
    );

    szg_image::transitionImage(
        cmd,
        swapchainImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    TRY_VK_RESULT(
        vkEndCommandBuffer(cmd),
        "Failed to end command buffer after recording copy into swapchain."
    );

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

    TRY_VK_RESULT(
        vkQueueSubmit2(
            submissionQueue, 1, &submitInfo, currentFrame.renderFence
        ),
        "Failed to submit command buffer before frame presentation."
    );

    VkSwapchainKHR const swapchainHandle{swapchain.swapchain()};
    VkPresentInfoKHR const presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame.renderSemaphore,

        .swapchainCount = 1,
        .pSwapchains = &swapchainHandle,

        .pImageIndices = &swapchainImageIndex,
        .pResults = nullptr, // Only one swapchain
    };

    if (VkResult const presentResult{
            vkQueuePresentKHR(submissionQueue, &presentInfo)
        };
        presentResult != VK_SUCCESS)
    {
        if (presentResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            LogVkResult(
                presentResult,
                "Failed swapchain presentation due to error that was not "
                "OUT_OF_DATE."
            );
        }
        return presentResult;
    }

    return VK_SUCCESS;
}
auto uiInit(
    VkInstance const instance,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    uint32_t const graphicsQueueFamily,
    VkQueue const graphicsQueue,
    GLFWwindow* const window
) -> VkDescriptorPool
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

    std::vector<VkFormat> const colorAttachmentFormats{
        VK_FORMAT_R16G16B16A16_SFLOAT
    };
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

    ImGui_ImplVulkan_Init(&initInfo);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    Log("ImGui initialized.");

    return imguiDescriptorPool;
}
auto uiBegin(
    ui::UIPreferences& preferences, ui::UIPreferences const& defaultPreferences
) -> UIResults
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ui::HUDState const hud{renderHUD(preferences)};

    bool const reloadUI{
        hud.applyPreferencesRequested || hud.resetPreferencesRequested
    };
    if (hud.resetPreferencesRequested)
    {
        preferences = defaultPreferences;
    }
    ui::DockingLayout dockingLayout{};

    if (hud.rebuildLayoutRequested && hud.dockspaceID != 0)
    {
        dockingLayout =
            buildDefaultMultiWindowLayout(hud.workArea, hud.dockspaceID);
    }

    return {
        .hud = hud,
        .dockingLayout = dockingLayout,
        .reloadRequested = reloadUI,
    };
}
auto uiRecordDraw(
    VkCommandBuffer const cmd,
    scene::SceneTexture& sceneTexture,
    szg_image::ImageView& windowTexture
) -> VkRect2D
{
    sceneTexture.texture().recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    windowTexture.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    );

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
        vkinit::renderingAttachmentInfo(
            windowTexture.view(), VK_IMAGE_LAYOUT_GENERAL
        )
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
void uiEnd() { ImGui::Render(); }
void uiCleanup()
{
    ImPlot::DestroyContext();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
// Resets the ImGui style and reloads other resources like fonts, then builds a
// new style from the passed preferences.
void uiReload(VkDevice const device, ui::UIPreferences const preferences)
{
    float constexpr FONT_BASE_SIZE{13.0F};

    std::filesystem::path const fontPath{
        DebugUtils::getLoadedDebugUtils().makeAbsolutePath(
            std::filesystem::path{"assets/proggyfonts/ProggyClean.ttf"}
        )
    };
    ImGui::GetIO().Fonts->Clear();
    ImGui::GetIO().Fonts->AddFontFromFileTTF(
        fontPath.string().c_str(), FONT_BASE_SIZE * preferences.dpiScale
    );

    // Wait for idle since we are modifying backend resources
    vkDeviceWaitIdle(device);
    // We destroy this to later force a rebuild when the fonts are needed.
    ImGui_ImplVulkan_DestroyFontsTexture();

    // TODO: is rebuilding the font with a specific scale good?
    // ImGui recommends building fonts at various sizes then just
    // selecting them

    // Reset style so further scaling works off the base "1.0x" scaling
    ImGui::GetStyle() = ImGuiStyle{};
    ImGui::StyleColorsDark();

    ImGui::GetStyle().ScaleAllSizes(preferences.dpiScale);
}
} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto szg_editor::run() -> EditorResult
{
    using namespace std::chrono_literals;

    std::optional<EditorResources> resourcesResult{initialize()};
    if (!resourcesResult.has_value())
    {
        Error("Failed to initialize editor.");
        return EditorResult::ERROR;
    }
    PlatformWindow const& mainWindow{resourcesResult.value().window};
    GraphicsContext& graphicsContext{resourcesResult.value().graphics};
    Swapchain& swapchain{resourcesResult.value().swapchain};
    FrameBuffer& frameBuffer{resourcesResult.value().frameBuffer};

    // Init input before imgui so it properly chains our callbacks
    szg_input::InputHandler inputHandler{};
    glfwSetWindowUserPointer(
        mainWindow.handle(), reinterpret_cast<void*>(&inputHandler)
    );
    glfwSetKeyCallback(
        mainWindow.handle(), szg_input::InputHandler::callbackKey_glfw
    );
    glfwSetCursorPosCallback(
        mainWindow.handle(), szg_input::InputHandler::callbackMouse_glfw
    );
    VkDescriptorPool const imguiPool{uiInit(
        graphicsContext.vulkanContext().instance,
        graphicsContext.vulkanContext().physicalDevice,
        graphicsContext.vulkanContext().device,
        graphicsContext.vulkanContext().graphicsQueueFamily,
        graphicsContext.vulkanContext().graphicsQueue,
        mainWindow.handle()
    )};

    // We oversize textures and use resizable subregion
    VkExtent2D constexpr TEXTURE_MAX{4096, 4096};
    std::optional<scene::SceneTexture> sceneTextureResult{
        scene::SceneTexture::create(
            graphicsContext.vulkanContext().device,
            graphicsContext.allocator(),
            graphicsContext.descriptorAllocator(),
            TEXTURE_MAX,
            VK_FORMAT_R16G16B16A16_SFLOAT
        )
    };
    if (!sceneTextureResult.has_value())
    {
        Error("Failed to allocate scene texture");
        return EditorResult::ERROR;
    }
    std::optional<std::unique_ptr<szg_image::ImageView>> windowTextureResult{
        szg_image::ImageView::allocate(
            graphicsContext.vulkanContext().device,
            graphicsContext.allocator(),
            szg_image::ImageAllocationParameters{
                .extent = TEXTURE_MAX,
                .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                .usageFlags =
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT        // copy to swapchain
                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT      // copy from scene
                    | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // imgui

            },
            szg_image::ImageViewAllocationParameters{
                .subresourceRange =
                    vkinit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)
            }
        )
    };
    if (!windowTextureResult.has_value())
    {
        Warning("Failed to allocate window texture.");
        return EditorResult::ERROR;
    }

    ImmediateSubmissionQueue submissionQueue{};
    if (auto result{ImmediateSubmissionQueue::create(
            graphicsContext.vulkanContext().device,
            graphicsContext.vulkanContext().graphicsQueueFamily
        )};
        result.has_value())
    {
        submissionQueue = std::move(result).value();
    }
    else
    {
        Error("Failed to create immediate submission queue.");
        return EditorResult::ERROR;
    }

    MeshAssetLibrary meshAssets{};
    if (auto result{loadGltfMeshes(
            graphicsContext.vulkanContext().device,
            graphicsContext.allocator(),
            graphicsContext.vulkanContext().graphicsQueue,
            submissionQueue,
            "assets/vkguide/basicmesh.glb"
        )};
        result.has_value())
    {
        meshAssets.loadedMeshes.insert(
            std::end(meshAssets.loadedMeshes),
            std::begin(result.value()),
            std::end(result.value())
        );
    }
    else
    {
        Error("Failed to load any meshes.");
        return EditorResult::ERROR;
    }

    // Load textures as a test of image loading functionality
    std::vector<std::unique_ptr<szg_image::Image>> testImages{};
    if (auto textureLoadResult{szg_assets::loadTextureFromFile(
            graphicsContext.vulkanContext().device,
            graphicsContext.allocator(),
            graphicsContext.vulkanContext().graphicsQueue,
            submissionQueue,
            "assets/khronos-gltf-sample-assets/ABeautifulGame/glTF/"
            "bishop_black_base_color.jpg",
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        )};
        textureLoadResult.has_value())
    {
        testImages.push_back(std::move(textureLoadResult).value());
    }

    // A test widget that can display a texture in a UI window
    std::unique_ptr<ui::TextureDisplay> testImageWidget{};
    if (std::optional<ui::TextureDisplay> textureDisplayResult{
            ui::TextureDisplay::create(
                graphicsContext.vulkanContext().device,
                graphicsContext.allocator(),
                TEXTURE_MAX,
                VK_FORMAT_R16G16B16A16_SFLOAT
            )
        };
        textureDisplayResult.has_value())
    {
        testImageWidget = std::make_unique<ui::TextureDisplay>(
            std::move(textureDisplayResult).value()
        );
    }
    else
    {
        Error("Failed to create widget to display images.");
    }

    bool inputCapturedByScene{false};
    scene::Scene scene{scene::Scene::defaultScene(
        graphicsContext.vulkanContext().device,
        graphicsContext.allocator(),
        meshAssets
    )};

    scene::SceneTexture& sceneTexture = sceneTextureResult.value();
    szg_image::ImageView& windowTexture = *windowTextureResult.value();

    Engine* const renderer = Engine::loadEngine(
        mainWindow,
        graphicsContext.vulkanContext().instance,
        graphicsContext.vulkanContext().physicalDevice,
        graphicsContext.vulkanContext().device,
        graphicsContext.allocator(),
        graphicsContext.descriptorAllocator(),
        sceneTexture,
        graphicsContext.vulkanContext().graphicsQueue,
        graphicsContext.vulkanContext().graphicsQueueFamily
    );
    if (renderer == nullptr)
    {
        return EditorResult::ERROR;
    }

    ui::UIPreferences uiPreferences{};
    bool uiReloadNecessary{false};
    uiReload(graphicsContext.vulkanContext().device, uiPreferences);

    double timeSecondsPrevious{0.0};
    RingBuffer fpsHistory{};
    float fpsTarget{defaultRefreshRate()};

    while (glfwWindowShouldClose(mainWindow.handle()) == GLFW_FALSE)
    {
        glfwPollEvents();

        if (glfwGetWindowAttrib(mainWindow.handle(), GLFW_ICONIFIED)
            == GLFW_TRUE)
        {
            // TODO: should time pause while minified?
            std::this_thread::sleep_for(1ms);
            continue;
        }

        double const timeSecondsCurrent{glfwGetTime()};
        double const deltaTimeSeconds{timeSecondsCurrent - timeSecondsPrevious};

        if (deltaTimeSeconds < 1.0 / fpsTarget)
        {
            continue;
        }

        szg_input::InputSnapshot const inputSnapshot{inputHandler.collect()};

        TickTiming const lastFrameTiming{
            .timeElapsedSeconds = timeSecondsCurrent,
            .deltaTimeSeconds = deltaTimeSeconds,
        };

        timeSecondsPrevious = timeSecondsCurrent;

        fpsHistory.write(1.0 / deltaTimeSeconds);

        if (inputCapturedByScene)
        {
            scene.handleInput(lastFrameTiming, inputSnapshot);
        }
        scene.tick(lastFrameTiming);

        frameBuffer.increment();
        Frame const& currentFrame{frameBuffer.currentFrame()};
        VulkanContext const& vulkanContext{graphicsContext.vulkanContext()};

        if (VkResult const beginFrameResult{
                beginFrame(currentFrame, vulkanContext.device)
            };
            beginFrameResult != VK_SUCCESS)
        {
            LogVkResult(beginFrameResult, "Failed to begin frame.");
            return EditorResult::ERROR;
        }

        if (uiReloadNecessary)
        {
            uiReload(vulkanContext.device, uiPreferences);
        }

        UIResults const uiResults{uiBegin(uiPreferences, ui::UIPreferences{})};
        uiReloadNecessary = uiResults.reloadRequested;
        renderer->uiEngineControls(uiResults.dockingLayout);
        ui::performanceWindow(
            "Engine Performance",
            uiResults.dockingLayout.centerBottom,
            fpsHistory,
            fpsTarget
        );
        ui::WindowResult<std::optional<scene::SceneViewport>> const
            sceneViewport{ui::sceneViewportWindow(
                "Scene Viewport",
                uiResults.dockingLayout.centerTop,
                uiResults.hud.maximizeSceneViewport
                    ? uiResults.hud.workArea
                    : std::optional<ui::UIRectangle>{},
                sceneTexture
            )};
        if (inputCapturedByScene != sceneViewport.focused)
        {
            if (sceneViewport.focused)
            {
                glfwSetInputMode(
                    mainWindow.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED
                );
            }
            else
            {
                glfwSetInputMode(
                    mainWindow.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL
                );
            }

            inputCapturedByScene = sceneViewport.focused;
        }
        if (inputCapturedByScene
            && inputSnapshot.keys.getStatus(szg_input::KeyCode::TAB).pressed())
        {
            inputCapturedByScene = false;
            glfwSetInputMode(
                mainWindow.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL
            );
            ImGui::SetWindowFocus(nullptr);
        }

        if (testImageWidget != nullptr && !testImages.empty())
        {
            testImageWidget->uiRender(
                "TEST IMAGE",
                uiResults.dockingLayout.right,
                currentFrame.mainCommandBuffer,
                *testImages[0]
            );
        }

        ui::sceneControlsWindow(
            "Default Scene", uiResults.dockingLayout.left, scene, meshAssets
        );
        uiEnd();

        renderer->recordDraw(
            currentFrame.mainCommandBuffer,
            scene,
            sceneTexture,
            sceneViewport.payload
        );

        VkRect2D const windowTextureDrawArea{uiRecordDraw(
            currentFrame.mainCommandBuffer, sceneTexture, windowTexture
        )};

        if (VkResult const endFrameResult{endFrame(
                currentFrame,
                swapchain,
                vulkanContext.device,
                vulkanContext.graphicsQueue,
                currentFrame.mainCommandBuffer,
                windowTexture.image(),
                windowTextureDrawArea
            )};
            endFrameResult != VK_SUCCESS)
        {
            if (endFrameResult != VK_ERROR_OUT_OF_DATE_KHR)
            {
                LogVkResult(
                    endFrameResult,
                    "Failed to end frame, due to non-out-of-date error."
                );
                return EditorResult::ERROR;
            }

            std::optional<Swapchain> newSwapchain{rebuildSwapchain(
                swapchain,
                vulkanContext.physicalDevice,
                vulkanContext.device,
                vulkanContext.surface,
                mainWindow.extent()
            )};

            if (!newSwapchain.has_value())
            {
                Error("Failed to create new swapchain for resizing");
                return EditorResult::ERROR;
            }
            swapchain = std::move(newSwapchain).value();
        }
    }

    vkDeviceWaitIdle(graphicsContext.vulkanContext().device);
    if (nullptr != renderer)
    {
        renderer->cleanup(
            graphicsContext.vulkanContext().device, graphicsContext.allocator()
        );
    }
    uiCleanup();
    vkDestroyDescriptorPool(
        graphicsContext.vulkanContext().device, imguiPool, nullptr
    );

    return EditorResult::SUCCESS;
}