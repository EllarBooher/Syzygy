#include "editor.hpp"

#include "syzygy/assets/assets.hpp"
#include "syzygy/core/immediate.hpp"
#include "syzygy/core/input.hpp"
#include "syzygy/core/log.hpp"
#include "syzygy/core/ringbuffer.hpp"
#include "syzygy/core/timing.hpp"
#include "syzygy/editor/editorconfig.hpp"
#include "syzygy/editor/framebuffer.hpp"
#include "syzygy/editor/graphicscontext.hpp"
#include "syzygy/editor/swapchain.hpp"
#include "syzygy/editor/uilayer.hpp"
#include "syzygy/editor/window.hpp"
#include "syzygy/platform/integer.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include "syzygy/platform/vulkanusage.hpp"
#include "syzygy/renderer/image.hpp"
#include "syzygy/renderer/imageoperations.hpp"
#include "syzygy/renderer/imageview.hpp"
#include "syzygy/renderer/pipelines.hpp"
#include "syzygy/renderer/renderer.hpp"
#include "syzygy/renderer/scene.hpp"
#include "syzygy/renderer/scenetexture.hpp"
#include "syzygy/renderer/shaders.hpp"
#include "syzygy/renderer/vulkanstructs.hpp"
#include "syzygy/ui/dockinglayout.hpp"
#include "syzygy/ui/hud.hpp"
#include "syzygy/ui/statelesswidgets.hpp"
#include "syzygy/ui/texturedisplay.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <functional>
#include <glm/vec2.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace syzygy
{
struct Mesh;
} // namespace syzygy

VkExtent2D constexpr TEXTURE_MAX{4096, 4096};

struct UIResults
{
    syzygy::HUDState hud;
    syzygy::DockingLayout dockingLayout;
    bool reloadRequested;
};

struct EditorResources
{
    syzygy::PlatformWindow window;
    syzygy::GraphicsContext graphics;
    syzygy::Swapchain swapchain;
    syzygy::FrameBuffer frameBuffer;
    syzygy::InputHandler inputHandler;
    syzygy::UILayer uiLayer;
};

namespace
{
auto initialize() -> std::optional<EditorResources>
{
    SZG_INFO("Initializing Editor resources...");

    SZG_INFO("Creating window...");

    glm::u16vec2 constexpr DEFAULT_WINDOW_EXTENT{1920, 1080};

    std::optional<syzygy::PlatformWindow> windowResult{
        syzygy::PlatformWindow::create(DEFAULT_WINDOW_EXTENT)
    };
    if (!windowResult.has_value())
    {
        SZG_ERROR("Failed to create window.");
        return std::nullopt;
    }

    SZG_INFO("Creating Graphics Context...");

    std::optional<syzygy::GraphicsContext> graphicsResult{
        syzygy::GraphicsContext::create(windowResult.value())
    };
    if (!graphicsResult.has_value())
    {
        SZG_ERROR("Failed to create graphics context.");
        return std::nullopt;
    }
    syzygy::GraphicsContext& graphicsContext{graphicsResult.value()};

    SZG_INFO("Creating Swapchain...");

    std::optional<syzygy::Swapchain> swapchainResult{syzygy::Swapchain::create(
        graphicsContext.physicalDevice(),
        graphicsContext.device(),
        graphicsContext.surface(),
        windowResult.value().extent(),
        std::optional<VkSwapchainKHR>{}
    )};
    if (!swapchainResult.has_value())
    {
        SZG_ERROR("Failed to create swapchain.");
        return std::nullopt;
    }

    SZG_INFO("Creating Frame Buffer...");

    std::optional<syzygy::FrameBuffer> frameBufferResult{
        syzygy::FrameBuffer::create(
            graphicsContext.device(), graphicsContext.universalQueueFamily()
        )
    };
    if (!frameBufferResult.has_value())
    {
        SZG_ERROR("Failed to create FrameBuffer.");
        return std::nullopt;
    }

    SZG_INFO("Creating Input Handler...");

    // Init input handler before UI backend so it chains our callbacks.
    // TODO: break this dependency where we depend on ImGui's GLFW backend to
    // chain our callbacks for us

    std::optional<syzygy::InputHandler> inputHandlerResult{
        syzygy::InputHandler::create(windowResult.value())
    };
    if (!inputHandlerResult.has_value())
    {
        SZG_ERROR("Failed to create input handler.");
        return std::nullopt;
    }
    syzygy::InputHandler const& inputHandler{inputHandlerResult.value()};

    SZG_INFO("Creating UI Layer...");

    std::optional<syzygy::UILayer> uiLayerResult{syzygy::UILayer::create(
        graphicsContext.instance(),
        graphicsContext.physicalDevice(),
        graphicsContext.device(),
        graphicsContext.allocator(),
        TEXTURE_MAX,
        graphicsContext.universalQueueFamily(),
        graphicsContext.universalQueue(),
        windowResult.value(),
        syzygy::UIPreferences{}
    )};
    if (!uiLayerResult.has_value())
    {
        SZG_ERROR("Failed to create UI Layer.");
        return std::nullopt;
    }

    SZG_INFO("Successfully initialized Editor resources.");

    return EditorResources{
        .window = std::move(windowResult).value(),
        .graphics = std::move(graphicsResult).value(),
        .swapchain = std::move(swapchainResult).value(),
        .frameBuffer = std::move(frameBufferResult).value(),
        .inputHandler = std::move(inputHandlerResult).value(),
        .uiLayer = std::move(uiLayerResult).value(),
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
    syzygy::Swapchain& old,
    VkPhysicalDevice const physicalDevice,
    VkDevice const device,
    VkSurfaceKHR const surface
) -> std::optional<syzygy::Swapchain>
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice, surface, &surfaceCapabilities
        )
        != VK_SUCCESS)
    {
        SZG_ERROR("Failed to get surface capabilities for swapchain creation.");
        return std::nullopt;
    }

    glm::u16vec2 const newExtent{
        surfaceCapabilities.currentExtent.width,
        surfaceCapabilities.currentExtent.height
    };

    SZG_INFO(
        "Resizing swapchain: ({},{}) -> ({},{})",
        old.extent().width,
        old.extent().height,
        newExtent.x,
        newExtent.y
    );

    std::optional<syzygy::Swapchain> newSwapchain{syzygy::Swapchain::create(
        physicalDevice, device, surface, newExtent, old.swapchain()
    )};

    return newSwapchain;
}
auto beginFrame(syzygy::Frame const& currentFrame, VkDevice const device)
    -> VkResult
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
        SZG_LOG_VK(waitResult, "Failed to wait on frame in-use fence.");
        return waitResult;
    }

    if (VkResult const resetResult{
            vkResetFences(device, 1, &currentFrame.renderFence)
        };
        resetResult != VK_SUCCESS)
    {
        SZG_LOG_VK(resetResult, "Failed to reset frame fences.");
        return resetResult;
    }

    VkCommandBuffer const& cmd = currentFrame.mainCommandBuffer;

    if (VkResult const resetCmdResult{vkResetCommandBuffer(cmd, 0)};
        resetCmdResult != VK_SUCCESS)
    {
        SZG_LOG_VK(resetCmdResult, "Failed to reset frame command buffer.");
        return resetCmdResult;
    }

    VkCommandBufferBeginInfo const cmdBeginInfo{syzygy::commandBufferBeginInfo(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    )};
    if (VkResult const beginCmdResult{vkBeginCommandBuffer(cmd, &cmdBeginInfo)};
        beginCmdResult != VK_SUCCESS)
    {
        SZG_LOG_VK(beginCmdResult, "Failed to begin frame command buffer.");
        return beginCmdResult;
    }

    return VK_SUCCESS;
}

auto endFrame(
    syzygy::Frame const& currentFrame,
    syzygy::Swapchain& swapchain,
    VkDevice const device,
    VkQueue const submissionQueue,
    VkCommandBuffer const cmd,
    syzygy::SceneTexture& sourceTexture,
    VkRect2D const sourceSubregion,
    syzygy::GammaTransferFunction const gammaFunction
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
            SZG_LOG_VK(acquireResult, "Failed to acquire swapchain image.");
        }
        SZG_LOG_VK(vkEndCommandBuffer(cmd), "Failed to end command buffer.");
        return acquireResult;
    }

    sourceTexture.color().recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_GENERAL
    );

    { // Perform conversion linear -> nonlinear before presentation
        syzygy::ShaderObjectReflected const& shader{
            swapchain.eotfPipeline(gammaFunction)
        };

        VkShaderStageFlagBits const stage{VK_SHADER_STAGE_COMPUTE_BIT};
        VkShaderEXT const shaderObject{shader.shaderObject()};
        VkPipelineLayout const layout{swapchain.eotfPipelineLayout()};
        VkDescriptorSet const descriptor{sourceTexture.singletonDescriptor()};

        vkCmdBindShadersEXT(cmd, 1, &stage, &shaderObject);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            layout,
            0,
            1,
            &descriptor,
            0,
            nullptr
        );

        uint32_t constexpr WORKGROUP_SIZE{16};

        VkExtent2D const swapchainExtent{swapchain.extent()};

        vkCmdDispatch(
            cmd,
            syzygy::computeDispatchCount(swapchainExtent.width, WORKGROUP_SIZE),
            syzygy::computeDispatchCount(
                swapchainExtent.height, WORKGROUP_SIZE
            ),
            1
        );

        vkCmdBindShadersEXT(cmd, 1, &stage, nullptr);
    }

    sourceTexture.color().recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
    );

    VkImage const swapchainImage{swapchain.images()[swapchainImageIndex]};
    syzygy::transitionImage(
        cmd,
        swapchainImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    syzygy::recordCopyImageToImage(
        cmd,
        sourceTexture.color().image().image(),
        swapchainImage,
        sourceSubregion,
        VkRect2D{.extent{swapchain.extent()}}
    );

    syzygy::transitionImage(
        cmd,
        swapchainImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    SZG_PROPAGATE_VK(
        vkEndCommandBuffer(cmd),
        "Failed to end command buffer after recording copy into swapchain."
    );

    // Submit commands

    VkCommandBufferSubmitInfo const cmdSubmitInfo{
        syzygy::commandBufferSubmitInfo(cmd)
    };
    VkSemaphoreSubmitInfo const waitInfo{syzygy::semaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, currentFrame.swapchainSemaphore
    )};
    VkSemaphoreSubmitInfo const signalInfo{syzygy::semaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, currentFrame.renderSemaphore
    )};

    std::vector<VkCommandBufferSubmitInfo> const cmdSubmitInfos{cmdSubmitInfo};
    std::vector<VkSemaphoreSubmitInfo> const waitInfos{waitInfo};
    std::vector<VkSemaphoreSubmitInfo> const signalInfos{signalInfo};

    // TODO: transferring to the swapchain should have its own command buffer
    VkSubmitInfo2 const submitInfo =
        syzygy::submitInfo(cmdSubmitInfos, waitInfos, signalInfos);

    SZG_PROPAGATE_VK(
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
            SZG_LOG_VK(
                presentResult,
                "Failed swapchain presentation due to error that was not "
                "OUT_OF_DATE."
            );
        }
        return presentResult;
    }

    return VK_SUCCESS;
}
} // namespace

namespace syzygy
{
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto run() -> EditorResult
{
    using namespace std::chrono_literals;

    std::optional<EditorResources> resourcesResult{initialize()};
    if (!resourcesResult.has_value())
    {
        SZG_ERROR("Failed to initialize editor.");
        return EditorResult::ERROR;
    }
    PlatformWindow const& mainWindow{resourcesResult.value().window};
    GraphicsContext& graphicsContext{resourcesResult.value().graphics};
    Swapchain& swapchain{resourcesResult.value().swapchain};
    FrameBuffer& frameBuffer{resourcesResult.value().frameBuffer};
    InputHandler& inputHandler{resourcesResult.value().inputHandler};
    UILayer& uiLayer{resourcesResult.value().uiLayer};

    EditorConfiguration configuration{};

    ImmediateSubmissionQueue submissionQueue{};
    if (auto result{ImmediateSubmissionQueue::create(
            graphicsContext.device(), graphicsContext.universalQueueFamily()
        )};
        result.has_value())
    {
        submissionQueue = std::move(result).value();
    }
    else
    {
        SZG_ERROR("Failed to create immediate submission queue.");
        return EditorResult::ERROR;
    }

    std::optional<AssetLibrary> assetLibraryResult{
        AssetLibrary::loadDefaultAssets(graphicsContext, submissionQueue)
    };
    if (!assetLibraryResult.has_value())
    {
        SZG_ERROR("Unable to initialize asset library.");
        return EditorResult::ERROR;
    }
    AssetLibrary& assetLibrary{assetLibraryResult.value()};

    // A test widget that can display a texture in a UI window
    std::unique_ptr<TextureDisplay> testImageWidget{};
    if (std::optional<TextureDisplay> textureDisplayResult{
            TextureDisplay::create(
                graphicsContext.device(),
                graphicsContext.allocator(),
                graphicsContext.universalQueue(),
                submissionQueue,
                TEXTURE_MAX,
                VK_FORMAT_R16G16B16A16_SFLOAT
            )
        };
        textureDisplayResult.has_value())
    {
        testImageWidget = std::make_unique<TextureDisplay>(
            std::move(textureDisplayResult).value()
        );
    }
    else
    {
        SZG_ERROR("Failed to create widget to display images.");
    }

    bool inputCapturedByScene{false};
    Scene scene{Scene::defaultScene(assetLibrary)};
    std::optional<Renderer> rendererResult{Renderer::create(
        graphicsContext.device(),
        graphicsContext.allocator(),
        uiLayer.sceneTexture(),
        graphicsContext.descriptorAllocator(),
        uiLayer.sceneTextureLayout().value_or(VK_NULL_HANDLE)
    )};
    if (!rendererResult.has_value())
    {
        SZG_ERROR("Unable to create Renderer.");
        return EditorResult::ERROR;
    }
    Renderer& renderer{rendererResult.value()};

    double timeSecondsPrevious{0.0};
    RingBuffer fpsHistory{};
    float fpsTarget{defaultRefreshRate()};

    glfwShowWindow(mainWindow.handle());

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

        InputSnapshot const inputSnapshot{inputHandler.collect()};

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

        if (VkResult const beginFrameResult{
                beginFrame(currentFrame, graphicsContext.device())
            };
            beginFrameResult != VK_SUCCESS)
        {
            SZG_LOG_VK(beginFrameResult, "Failed to begin frame.");
            return EditorResult::ERROR;
        }

        assetLibrary.processTasks(graphicsContext, submissionQueue);

        DockingLayout const& dockingLayout{uiLayer.begin()};
        if (uiLayer.HUDMenuItem("Tools", "Load Mesh (.glTF / .glb / .bin)"))
        {
            assetLibrary.loadMeshesDialog(
                mainWindow, graphicsContext, submissionQueue
            );
        }

        editorConfigurationWindow(
            "Editor Configuration",
            dockingLayout.right,
            configuration,
            EditorConfiguration{}
        );

        renderer.uiEngineControls(dockingLayout);
        performanceWindow(
            "Engine Performance",
            dockingLayout.centerBottom,
            fpsHistory,
            fpsTarget
        );

        std::optional<SceneViewport> sceneViewport{
            uiLayer.sceneViewport(inputCapturedByScene)
        };

        if (inputCapturedByScene
            && (!sceneViewport.has_value()
                || inputSnapshot.keys.getStatus(KeyCode::TAB).pressed()))
        {
            uiLayer.setCursorEnabled(true);
            inputHandler.setCursorCaptured(false);
            inputCapturedByScene = false;
        }
        if (!inputCapturedByScene && sceneViewport.has_value()
            && sceneViewport.value().focused)
        {
            uiLayer.setCursorEnabled(false);
            inputHandler.setCursorCaptured(true);
            inputCapturedByScene = true;
        }

        if (TextureDisplay::UIResult const textureDisplayResult{
                testImageWidget->uiRender(
                    "Texture Viewer",
                    dockingLayout.right,
                    currentFrame.mainCommandBuffer,
                    assetLibrary.fetchAssetRefs<ImageView>()
                )
            };
            textureDisplayResult.loadTexturesRequested)
        {
            assetLibrary.loadTexturesDialog(mainWindow, uiLayer);
        }

        uiLayer.renderWidgets();

        sceneControlsWindows(
            "Default Scene",
            dockingLayout.left,
            scene,
            assetLibrary.fetchAssets<Mesh>(),
            assetLibrary.fetchAssets<ImageView>()
        );

        uiLayer.end();

        scene.calculateShadowBounds();

        if (sceneViewport.has_value())
        {
            renderer.recordDraw(
                currentFrame.mainCommandBuffer,
                scene,
                graphicsContext.descriptorAllocator(),
                sceneViewport.value().texture,
                sceneViewport.value().renderedSubregion
            );
        }

        std::optional<UIOutputImage> uiOutput{
            uiLayer.recordDraw(currentFrame.mainCommandBuffer)
        };
        if (!uiOutput.has_value())
        {
            // TODO: make this not a fatal error, but that requires better
            // handling on frame resources like the open command buffer
            SZG_ERROR("UI Layer did not have output image.");
            return EditorResult::ERROR;
        }

        if (VkResult const endFrameResult{endFrame(
                currentFrame,
                swapchain,
                graphicsContext.device(),
                graphicsContext.universalQueue(),
                currentFrame.mainCommandBuffer,
                uiOutput.value().texture,
                uiOutput.value().renderedSubregion,
                configuration.transferFunction
            )};
            endFrameResult != VK_SUCCESS)
        {
            if (endFrameResult != VK_ERROR_OUT_OF_DATE_KHR)
            {
                SZG_LOG_VK(
                    endFrameResult,
                    "Failed to end frame, due to non-out-of-date error."
                );
                return EditorResult::ERROR;
            }

            std::optional<Swapchain> newSwapchain{rebuildSwapchain(
                swapchain,
                graphicsContext.physicalDevice(),
                graphicsContext.device(),
                graphicsContext.surface()
            )};

            if (!newSwapchain.has_value())
            {
                SZG_ERROR("Failed to create new swapchain for resizing");
                return EditorResult::ERROR;
            }
            swapchain = std::move(newSwapchain).value();
        }
    }

    vkDeviceWaitIdle(graphicsContext.device());

    return EditorResult::SUCCESS;
}
} // namespace syzygy