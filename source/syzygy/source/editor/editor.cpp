#include "editor.hpp"

#include "../engine.hpp"
#include "../helpers.hpp"
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

    std::optional<PlatformWindow> const windowResult{
        createWindow(DEFAULT_WINDOW_EXTENT)
    };
    if (!windowResult.has_value())
    {
        Error("Failed to create window.");
        return std::nullopt;
    }

    PlatformWindow const window{windowResult.value()};

    Log("Window created.");

    Log("Creating Graphics Context...");

    std::optional<GraphicsContext> const graphicsResult{
        GraphicsContext::create(window)
    };
    if (!graphicsResult.has_value())
    {
        Error("Failed to create graphics context.");
        return std::nullopt;
    }
    GraphicsContext graphics{graphicsResult.value()};
    VulkanContext const& vulkanContext{graphics.vulkanContext()};

    Log("Created Graphics Context.");

    Log("Creating Swapchain...");

    std::optional<Swapchain> swapchainResult{Swapchain::create(
        window.extent(),
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
    Swapchain swapchain{swapchainResult.value()};

    Log("Created Swapchain.");

    Engine* const renderer{Engine::loadEngine(
        window,
        vulkanContext.instance,
        vulkanContext.physicalDevice,
        vulkanContext.device,
        graphics.allocator(),
        vulkanContext.graphicsQueue,
        vulkanContext.graphicsQueueFamily
    )};
    if (renderer == nullptr)
    {
        Error("Failed to load renderer.");
        return std::nullopt;
    }

    Log("Created Editor instance.");

    return std::make_optional<Editor>(window, graphics, swapchain, renderer);
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
        old.extent.width,
        old.extent.height,
        newExtent.x,
        newExtent.y
    ));

    std::optional<Swapchain> newSwapchain{Swapchain::create(
        newExtent, physicalDevice, device, surface, old.swapchain
    )};

    old.destroy(device);

    return newSwapchain;
}
} // namespace

auto Editor::run() -> EditorResult
{
    if (m_renderer == nullptr)
    {
        return EditorResult::ERROR_NO_RENDERER;
    }

    while (glfwWindowShouldClose(m_window.handle) == GLFW_FALSE)
    {
        glfwPollEvents();

        bool const iconified{
            glfwGetWindowAttrib(m_window.handle, GLFW_ICONIFIED) == GLFW_TRUE
        };

        bool const shouldRender{!iconified};

        static double previousTimeSeconds{0};

        double const currentTimeSeconds{glfwGetTime()};
        double const deltaTimeSeconds{currentTimeSeconds - previousTimeSeconds};

        double const targetFPS{m_renderer->targetFPS()};

        if (deltaTimeSeconds < 1.0 / targetFPS)
        {
            continue;
        }

        previousTimeSeconds = currentTimeSeconds;

        EngineLoopResult const loopResult{m_renderer->mainLoop(
            m_graphics.vulkanContext().device,
            m_graphics.allocator(),
            m_graphics.vulkanContext().graphicsQueue,
            m_swapchain.swapchain,
            m_swapchain.images,
            m_swapchain.imageViews,
            m_swapchain.extent,
            currentTimeSeconds,
            deltaTimeSeconds,
            shouldRender
        )};

        if (loopResult == EngineLoopResult::REBUILD_REQUESTED)
        {
            std::optional<Swapchain> newSwapchain{};
            if (!newSwapchain.has_value())
            {
                Error("Failed to create new swapchain for resizing");
                return EditorResult::ERROR_EDITOR;
            }
            m_swapchain = std::move(newSwapchain).value();
        }
    }

    return EditorResult::SUCCESS;
}

Editor::~Editor() noexcept
{
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

    m_swapchain.destroy(device);
    m_graphics.destroy();

    glfwTerminate();
    m_window.destroy();
}

auto Editor::createWindow(glm::u16vec2 const extent)
    -> std::optional<PlatformWindow>
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    char const* const WINDOW_TITLE = "Syzygy";

    GLFWwindow* const handle{glfwCreateWindow(
        static_cast<int32_t>(extent.x),
        static_cast<int32_t>(extent.y),
        WINDOW_TITLE,
        nullptr,
        nullptr
    )};

    if (handle == nullptr)
    {
        // TODO: figure out where GLFW reports errors
        return std::nullopt;
    }

    return PlatformWindow{handle};
}