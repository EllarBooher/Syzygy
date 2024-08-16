#pragma once

#include <glm/vec2.hpp>
#include <optional>

struct GLFWwindow;

namespace syzygy
{
struct PlatformWindow
{
public:
    PlatformWindow& operator=(PlatformWindow&&) = delete;
    PlatformWindow(PlatformWindow const&) = delete;
    PlatformWindow& operator=(PlatformWindow const&) = delete;

    PlatformWindow(PlatformWindow&&) noexcept;
    ~PlatformWindow();

private:
    PlatformWindow() = default;
    void destroy();

public:
    static auto create(glm::u16vec2 extent) -> std::optional<PlatformWindow>;

    auto extent() const -> glm::u16vec2;
    auto handle() const -> GLFWwindow*;

private:
    GLFWwindow* m_handle{nullptr};
};
} // namespace syzygy