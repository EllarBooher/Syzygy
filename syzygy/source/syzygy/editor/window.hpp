#pragma once

#include <glm/vec2.hpp>
#include <optional>

struct GLFWwindow;

struct PlatformWindow
{
public:
    PlatformWindow() noexcept = default;

    PlatformWindow(PlatformWindow&& other) noexcept;

    PlatformWindow& operator=(PlatformWindow&& other) noexcept;

    ~PlatformWindow() noexcept { destroy(); }

    PlatformWindow(PlatformWindow const& other) = delete;
    PlatformWindow& operator=(PlatformWindow const& other) = delete;

    static auto create(glm::u16vec2 extent) -> std::optional<PlatformWindow>;

    auto extent() const -> glm::u16vec2;
    auto handle() const -> GLFWwindow*;

private:
    void destroy();

    explicit PlatformWindow(GLFWwindow* handle)
        : m_handle(handle)
    {
    }

    GLFWwindow* m_handle{nullptr};
};