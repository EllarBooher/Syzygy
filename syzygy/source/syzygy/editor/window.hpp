#pragma once

#include <glm/vec2.hpp>
#include <optional>

struct GLFWwindow;

namespace syzygy
{
struct PlatformWindow
{
public:
    auto operator=(PlatformWindow&&) -> PlatformWindow& = delete;
    PlatformWindow(PlatformWindow const&) = delete;
    auto operator=(PlatformWindow const&) -> PlatformWindow& = delete;

    PlatformWindow(PlatformWindow&&) noexcept;
    ~PlatformWindow();

private:
    PlatformWindow() = default;
    void destroy();

public:
    static auto create(glm::u16vec2 extent) -> std::optional<PlatformWindow>;

    [[nodiscard]] auto extent() const -> glm::u16vec2;
    [[nodiscard]] auto handle() const -> GLFWwindow*;

private:
    GLFWwindow* m_handle{nullptr};
};
} // namespace syzygy