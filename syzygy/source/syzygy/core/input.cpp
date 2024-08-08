#include "input.hpp"

#include "syzygy/helpers.hpp"
#include <fmt/core.h>
#include <glm/gtx/string_cast.hpp>
#include <glm/vec2.hpp>
#include <optional>

namespace
{
auto toKeyCode_glfw(int32_t key) -> std::optional<szg_input::KeyCode>
{
    switch (key)
    {
    case GLFW_KEY_W:
        return szg_input::KeyCode::W;
    case GLFW_KEY_A:
        return szg_input::KeyCode::A;
    case GLFW_KEY_S:
        return szg_input::KeyCode::S;
    case GLFW_KEY_D:
        return szg_input::KeyCode::D;
    case GLFW_KEY_Q:
        return szg_input::KeyCode::Q;
    case GLFW_KEY_E:
        return szg_input::KeyCode::E;
    case GLFW_KEY_TAB:
        return szg_input::KeyCode::TAB;
    default:
        return std::nullopt;
    }
}
auto isDown_glfw(bool const currentDown, int32_t const action) -> bool
{
    switch (action)
    {
    case (GLFW_REPEAT):
    case (GLFW_PRESS):
        return true;
    case (GLFW_RELEASE):
        return false;
    default:
        return currentDown;
    }
}
auto toString(szg_input::KeyStatus const status) -> std::string
{
    if (status.down)
    {
        return status.edge ? "PRESSED" : "HELD";
    }

    return status.edge ? "RELEASED" : "NONE";
}
auto toString(szg_input::KeyCode const key) -> std::string
{
    switch (key)
    {
    case (szg_input::KeyCode::W):
        return "W";
    case (szg_input::KeyCode::A):
        return "A";
    case (szg_input::KeyCode::S):
        return "S";
    case (szg_input::KeyCode::D):
        return "D";
    case (szg_input::KeyCode::Q):
        return "Q";
    case (szg_input::KeyCode::E):
        return "E";
    case (szg_input::KeyCode::TAB):
        return "TAB";
    case (szg_input::KeyCode::MAX):
    default:
        return "UNKOWN_KEY";
    }
}
} // namespace

void szg_input::InputHandler::callbackKey_glfw(
    GLFWwindow* window, int key, int scancode, int action, int mods
)
{
    InputHandler* activeHandler{
        reinterpret_cast<InputHandler*>(glfwGetWindowUserPointer(window))
    };

    if (activeHandler == nullptr)
    {
        return;
    }

    activeHandler->handleKey_glfw(key, scancode, action, mods);
}

void szg_input::InputHandler::callbackMouse_glfw(
    GLFWwindow* window, double xpos, double ypos
)
{
    InputHandler* activeHandler{
        reinterpret_cast<InputHandler*>(glfwGetWindowUserPointer(window))
    };

    if (activeHandler == nullptr)
    {
        return;
    }

    activeHandler->handleMouse_glfw(xpos, ypos);
}

void szg_input::InputHandler::handleKey_glfw(
    int32_t key, int32_t /*scancode*/, int32_t action, int32_t /*mods*/
)
{
    std::optional<szg_input::KeyCode> const keyResult{toKeyCode_glfw(key)};
    if (!keyResult.has_value())
    {
        return;
    }
    szg_input::KeyCode const keyCode{keyResult.value()};

    bool& isDown{m_keysNew.keysDown[static_cast<size_t>(keyCode)]};
    isDown = isDown_glfw(isDown, action);
}

void szg_input::InputHandler::handleMouse_glfw(double xpos, double ypos)
{
    m_cursorNew.position =
        glm::u16vec2{static_cast<uint16_t>(xpos), static_cast<uint16_t>(ypos)};
    if (m_skipNextCursorDelta)
    {
        m_cursorOld.position = m_cursorNew.position;
        m_skipNextCursorDelta = false;
        return;
    }
}

auto szg_input::InputHandler::collect() -> InputSnapshot
{
    KeySnapshot keys{};
    for (size_t index{0}; index < m_keysNew.keysDown.size(); index++)
    {
        bool const oldDown{m_keysOld.keysDown[index]};
        bool const isDown{m_keysNew.keysDown[index]};

        keys.keys[index] = KeyStatus{
            .down = isDown,
            .edge = isDown != oldDown,
        };
    }

    CursorSnapshot cursor{};
    cursor.currentPosition = m_cursorNew.position;
    cursor.lastPosition = m_cursorOld.position;

    m_cursorOld = m_cursorNew;
    m_keysOld = m_keysNew;

    return {
        .keys = keys,
        .cursor = cursor,
    };
}

void szg_input::InputHandler::setSkipNextCursorDelta(bool const skip)
{
    m_skipNextCursorDelta = skip;
}

auto szg_input::KeySnapshot::getStatus(KeyCode const key) const -> KeyStatus
{
    return keys[static_cast<size_t>(key)];
}

void szg_input::KeySnapshot::setStatus(
    KeyCode const key, KeyStatus const status
)
{
    if (getStatus(key) == status)
    {
        return;
    }

    keys[static_cast<size_t>(key)] = status;
}

auto szg_input::KeyStatus::pressed() const -> bool { return down && edge; }

auto szg_input::KeyStatus::operator==(KeyStatus const& other) const -> bool
{
    return other.down == down && other.edge == edge;
}

auto szg_input::CursorSnapshot::delta() const -> glm::i32vec2
{
    return glm::i32vec2{currentPosition} - glm::i32vec2{lastPosition};
}

auto szg_input::InputSnapshot::format() const -> std::string
{
    std::string output;

    for (size_t index{0}; index < keys.keys.size(); index++)
    {
        szg_input::KeyCode const keyCode{static_cast<szg_input::KeyCode>(index)
        };

        output += fmt::format(
            "{}: {:9}", toString(keyCode), toString(keys.getStatus(keyCode))
        );
    }

    output += fmt::format(
        "Cursor: Current: {} Last: {}",
        glm::to_string(cursor.currentPosition),
        glm::to_string(cursor.lastPosition)
    );

    return output;
}
