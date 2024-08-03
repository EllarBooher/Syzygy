#include "window.hpp"
#include "syzygy/core/integer.hpp"
#include <GLFW/glfw3.h>
#include <utility>

auto PlatformWindow::extent() const -> glm::u16vec2
{
    int32_t width{0};
    int32_t height{0};
    glfwGetWindowSize(handle(), &width, &height);

    return {
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
    };
}

PlatformWindow::PlatformWindow(PlatformWindow&& other) noexcept
{
    m_handle = std::exchange(other.m_handle, nullptr);
}

PlatformWindow::~PlatformWindow() { destroy(); }

auto PlatformWindow::create(glm::u16vec2 const extent)
    -> std::optional<PlatformWindow>
{
    std::optional<PlatformWindow> windowResult{std::in_place, PlatformWindow{}};
    PlatformWindow& window{windowResult.value()};

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

    window.m_handle = handle;

    return windowResult;
}

void PlatformWindow::destroy()
{
    glfwDestroyWindow(m_handle);
    m_handle = nullptr;
}

auto PlatformWindow::handle() const -> GLFWwindow* { return m_handle; }
