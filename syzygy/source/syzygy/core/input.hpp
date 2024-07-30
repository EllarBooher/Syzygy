#pragma once

#include "syzygy/core/integer.hpp"
#include <GLFW/glfw3.h>
#include <array>
#include <glm/vec2.hpp>
#include <string>

namespace szg_input
{
struct KeyStatus
{
    bool down;
    bool edge;

    auto pressed() const -> bool;

    auto operator==(KeyStatus const& other) const -> bool;
};

enum class KeyCode
{
    W,
    A,
    S,
    D,
    Q,
    E,
    TAB,
    MAX
};

struct KeySnapshot
{
    std::array<KeyStatus, static_cast<size_t>(KeyCode::MAX)> keys;

    auto getStatus(KeyCode) const -> KeyStatus;
    void setStatus(KeyCode, KeyStatus);
};
struct CursorSnapshot
{
    glm::u16vec2 lastPosition{};
    glm::u16vec2 currentPosition{};

    auto delta() const -> glm::i32vec2;
};

struct InputSnapshot
{
    KeySnapshot keys;
    CursorSnapshot cursor;

    auto format() const -> std::string;
};

class InputHandler
{
public:
    static void callbackKey_glfw(
        GLFWwindow* window, int key, int scancode, int action, int mods
    );
    void
    handleKey_glfw(int32_t key, int32_t scancode, int32_t action, int32_t mods);

    static void
    callbackMouse_glfw(GLFWwindow* window, double xpos, double ypos);
    void handleMouse_glfw(double xpos, double ypos);

    auto collect() -> InputSnapshot;

private:
    struct KeysState
    {
        std::array<bool, static_cast<size_t>(KeyCode::MAX)> keysDown;
    };
    struct CursorState
    {
        glm::u16vec2 position;
    };

    KeysState m_keysOld;
    CursorState m_cursorOld;

    KeysState m_keysNew;
    CursorState m_cursorNew;
};
} // namespace szg_input