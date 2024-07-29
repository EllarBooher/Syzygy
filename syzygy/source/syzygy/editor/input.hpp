#pragma once

#include <glm/vec2.hpp>
#include <GLFW/glfw3.h>
#include <string>
#include <array>

namespace szg_input
{
enum class KeyStatus
{
    NONE,
    PRESSED,
    HELD,
    RELEASED,
};
enum class KeyCode
{
    W,
    A,
    S,
    D,
    MAX
};

struct InputSnapshot
{
    bool dirty{false};
    std::array<KeyStatus, static_cast<size_t>(KeyCode::MAX)> keys;

    auto getStatus(KeyCode) const -> KeyStatus;
    void setStatus(KeyCode, KeyStatus);
};

class InputHandler
{
public:
    static void callback_glfw(GLFWwindow* window, int key, int scancode, int action, int mods);
    void handle_glfw(int32_t key, int32_t scancode, int32_t action, int32_t mods);
    void increment();
    auto formatStatus() -> std::string;
    auto collect() const -> InputSnapshot;

private:
    InputSnapshot m_snapshot{};
};
} // namespace szg_input