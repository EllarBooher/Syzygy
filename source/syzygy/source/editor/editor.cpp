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

    Engine* const renderer{Engine::loadEngine(window)};

    if (renderer == nullptr)
    {
        Error("Failed to load renderer.");
        return std::nullopt;
    }

    Log("Created Editor instance.");

    return std::make_optional<Editor>(window, renderer);
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

        if (deltaTimeSeconds >= 1.0 / targetFPS)
        {
            m_renderer->mainLoop(
                currentTimeSeconds,
                deltaTimeSeconds,
                shouldRender,
                m_window.extent()
            );
        }
    }

    return EditorResult::SUCCESS;
}

Editor::~Editor() noexcept
{
    m_renderer->cleanup();

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