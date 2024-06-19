#pragma once

#include <glm/vec2.hpp>
#include <optional>

struct GLFWwindow;

struct PlatformWindow
{
public:
    PlatformWindow() = default;

    PlatformWindow(PlatformWindow&& other) { *this = std::move(other); }

    PlatformWindow& operator=(PlatformWindow&& other)
    {
        m_handle = std::exchange(other.m_handle, nullptr);

        return *this;
    }

    PlatformWindow(PlatformWindow const& other) = delete;
    PlatformWindow& operator=(PlatformWindow const& other) = delete;

    static auto create(glm::u16vec2 extent) -> std::optional<PlatformWindow>;

    void destroy();

    auto extent() const -> glm::u16vec2;
    auto handle() const -> GLFWwindow*;

private:
    explicit PlatformWindow(GLFWwindow* handle)
        : m_handle(handle)
    {
    }

    GLFWwindow* m_handle{nullptr};
};