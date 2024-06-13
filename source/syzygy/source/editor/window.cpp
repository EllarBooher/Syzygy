#include "window.hpp"
#include <GLFW/glfw3.h>

auto PlatformWindow::extent() const -> glm::u16vec2
{
    int32_t width{0};
    int32_t height{0};
    glfwGetWindowSize(handle, &width, &height);

    return {
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
    };
}

// NOLINTBEGIN(readability-make-member-function-const): Propagate const from
// pointer

void PlatformWindow::destroy() { glfwDestroyWindow(handle); }

// NOLINTEND(readability-make-member-function-const)