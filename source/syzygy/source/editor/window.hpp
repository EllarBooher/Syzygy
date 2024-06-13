#pragma once

#include <glm/vec2.hpp>

struct GLFWwindow;

struct PlatformWindow
{
    GLFWwindow* handle{ nullptr };

    auto extent() const->glm::u16vec2;
    void destroy();
};