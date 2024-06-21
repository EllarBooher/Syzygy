#include "editor.hpp"

#include "../engine.hpp"
#include "../helpers.hpp"
#include "../initializers.hpp"
#include <GLFW/glfw3.h>

auto Editor::create() -> std::optional<Editor>
{
    Log("Creating Editor instance.");

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
        windowResult.value().extent(),
        vulkanContext.physicalDevice,
        vulkanContext.device,
        vulkanContext.surface,
        std::optional<VkSwapchainKHR>{}
    )};
    if (!swapchainResult.has_value())
    {
        Error("Failed to create swapchain.");
        return std::nullopt;
    }

    Log("Created Swapchain.");

    Log("Creating Frame Buffer...");

    VulkanResult<FrameBuffer> frameBufferResult{FrameBuffer::create(
        vulkanContext.device, vulkanContext.graphicsQueueFamily
    )};
    if (!frameBufferResult.has_value())
    {
        LogVkResult(
            frameBufferResult.vk_result(), "Failed to create frame buffer."
        );
    }

    Log("Created Frame Buffer.");

    Engine* const renderer{Engine::loadEngine(
        windowResult.value(),
        vulkanContext.instance,
        vulkanContext.physicalDevice,
        vulkanContext.device,
        graphicsResult.value().allocator(),
        vulkanContext.graphicsQueue,
        vulkanContext.graphicsQueueFamily
    )};
    if (renderer == nullptr)
    {
        Error("Failed to load renderer.");
        return std::nullopt;
    }

    Log("Created Editor instance.");

    return std::make_optional<Editor>(Editor{
        std::move(windowResult).value(),
        std::move(graphicsResult).value(),
        std::move(swapchainResult).value(),
        std::move(frameBufferResult).value(),
        renderer
    });
}

namespace
{
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
        newExtent, physicalDevice, device, surface, old.swapchain()
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
    AllocatedImage& drawImage,
    VkRect2D const drawRect
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
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // TODO: remove this check, move ending command buffer outside of
            // this function
            CheckVkResult(vkEndCommandBuffer(cmd));
            return acquireResult;
        }
    }
    assert(swapchainImageIndex != std::numeric_limits<uint32_t>::max());

    VkImage const swapchainImage{swapchain.images()[swapchainImageIndex]};

    drawImage.recordTransitionBarriered(
        cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
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
        drawImage.image(),
        swapchainImage,
        drawRect,
        VkRect2D{.extent{swapchain.extent()}}
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

    if (VkResult const submissionResult{vkQueueSubmit2(
            submissionQueue, 1, &submitInfo, currentFrame.renderFence
        )};
        submissionResult != VK_SUCCESS)
    {
        LogVkResult(
            submissionResult,
            "Failed to submit command buffer before frame presentation."
        );
        return submissionResult;
    }

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

    {
        VkResult const presentResult{
            vkQueuePresentKHR(submissionQueue, &presentInfo)
        };
        if (presentResult != VK_SUCCESS
            && presentResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            LogVkResult(presentResult, "Failed frame presentation.");
        }
        return presentResult;
    }
}
} // namespace

auto Editor::run() -> EditorResult
{
    if (m_renderer == nullptr)
    {
        return EditorResult::ERROR_NO_RENDERER;
    }

    while (glfwWindowShouldClose(m_window.handle()) == GLFW_FALSE)
    {
        glfwPollEvents();

        bool const iconified{
            glfwGetWindowAttrib(m_window.handle(), GLFW_ICONIFIED) == GLFW_TRUE
        };

        if (iconified)
        {
            continue;
        }

        static double previousTimeSeconds{0};

        double const currentTimeSeconds{glfwGetTime()};
        double const deltaTimeSeconds{currentTimeSeconds - previousTimeSeconds};

        double const targetFPS{m_renderer->targetFPS()};

        if (deltaTimeSeconds < 1.0 / targetFPS)
        {
            continue;
        }

        previousTimeSeconds = currentTimeSeconds;

        m_frameBuffer.increment();

        Frame const& currentFrame{m_frameBuffer.currentFrame()};
        VulkanContext const& vulkanContext{m_graphics.vulkanContext()};

        if (VkResult const beginFrameResult{
                beginFrame(currentFrame, vulkanContext.device)
            };
            beginFrameResult != VK_SUCCESS)
        {
            LogVkResult(beginFrameResult, "Failed to begin frame.");
            return EditorResult::ERROR_EDITOR;
        }

        m_renderer->tickWorld(TickTiming{
            .timeElapsedSeconds = currentTimeSeconds,
            .deltaTimeSeconds = deltaTimeSeconds,
        });

        VkRect2D const drawRect{m_renderer->mainLoop(
            vulkanContext.device,
            currentFrame.mainCommandBuffer,
            deltaTimeSeconds
        )};

        if (VkResult const endFrameResult{endFrame(
                currentFrame,
                m_swapchain,
                vulkanContext.device,
                vulkanContext.graphicsQueue,
                currentFrame.mainCommandBuffer,
                m_renderer->drawImage(),
                drawRect
            )};
            endFrameResult != VK_SUCCESS)
        {
            if (endFrameResult != VK_ERROR_OUT_OF_DATE_KHR)
            {
                LogVkResult(
                    endFrameResult,
                    "Failed to end frame, due to non-out-of-date error."
                );
                return EditorResult::ERROR_EDITOR;
            }

            if (std::optional<Swapchain> newSwapchain{rebuildSwapchain(
                    m_swapchain,
                    vulkanContext.physicalDevice,
                    vulkanContext.device,
                    vulkanContext.surface,
                    m_window.extent()
                )};
                newSwapchain.has_value())
            {
                m_swapchain = std::move(newSwapchain).value();
            }
            else
            {
                Error("Failed to create new swapchain for resizing");
                return EditorResult::ERROR_EDITOR;
            }
        }
    }

    return EditorResult::SUCCESS;
}

void Editor::destroy()
{
    if (!m_initialized)
    {
        return;
    }

    VkDevice const device{m_graphics.vulkanContext().device};
    if (VK_NULL_HANDLE == device)
    {
        Warning("At destruction time, Vulkan device was null.");
        return;
    }

    if (nullptr != m_renderer)
    {
        m_renderer->cleanup(device, m_graphics.allocator());
    }

    // Ensure proper destruction order
    m_frameBuffer = {};
    m_swapchain = {};
    m_graphics = {};

    m_window = {};

    glfwTerminate();
}
