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

    Log("Created Graphics Context.");

    VulkanContext const& vulkanContext{graphics.vulkanContext()};
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

    return std::make_optional<Editor>(window, graphics, renderer);
}

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
            m_graphics.swapchain().swapchain,
            m_graphics.swapchain().images,
            m_graphics.swapchain().imageViews,
            m_graphics.swapchain().extent,
            currentTimeSeconds,
            deltaTimeSeconds,
            shouldRender
        )};

        if (loopResult == EngineLoopResult::REBUILD_REQUESTED)
        {
            glm::u16vec2 const newExtent{m_window.extent()};

            VkExtent2D const oldExtent{m_graphics.swapchain().extent};

            Log(fmt::format(
                "Resizing swapchain: ({},{}) -> ({},{})",
                oldExtent.width,
                oldExtent.height,
                newExtent.x,
                newExtent.y
            ));

            m_graphics.swapchain().destroy(m_graphics.vulkanContext().device);

            std::optional<Swapchain> newSwapchain{Swapchain::create(
                newExtent,
                m_graphics.vulkanContext().physicalDevice,
                m_graphics.vulkanContext().device,
                m_graphics.vulkanContext().surface
            )};

            if (!newSwapchain.has_value())
            {
                Error(fmt::format(
                    "Failed to create new swapchain for resizing",
                    glm::to_string(newExtent)
                ));
                return EditorResult::ERROR_EDITOR;
            }

            m_graphics.swapchain() = std::move(newSwapchain).value();
        }
    }

    return EditorResult::SUCCESS;
}

Editor::~Editor() noexcept
{
    if (nullptr != m_renderer)
    {
        m_renderer->cleanup(
            m_graphics.vulkanContext().device, m_graphics.allocator()
        );
    }

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