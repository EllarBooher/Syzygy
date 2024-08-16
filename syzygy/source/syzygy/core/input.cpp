#include "input.hpp"

#include "syzygy/platform/vulkanmacros.hpp"
#include <glm/gtx/string_cast.hpp>
#include <glm/vec2.hpp>
#include <optional>
#include <spdlog/fmt/bundled/core.h>

namespace
{
auto toKeyCode_glfw(int32_t key) -> std::optional<syzygy::KeyCode>
{
    switch (key)
    {
    case GLFW_KEY_W:
        return syzygy::KeyCode::W;
    case GLFW_KEY_A:
        return syzygy::KeyCode::A;
    case GLFW_KEY_S:
        return syzygy::KeyCode::S;
    case GLFW_KEY_D:
        return syzygy::KeyCode::D;
    case GLFW_KEY_Q:
        return syzygy::KeyCode::Q;
    case GLFW_KEY_E:
        return syzygy::KeyCode::E;
    case GLFW_KEY_TAB:
        return syzygy::KeyCode::TAB;
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
auto toString(syzygy::KeyStatus const status) -> std::string
{
    if (status.down)
    {
        return status.edge ? "PRESSED" : "HELD";
    }

    return status.edge ? "RELEASED" : "NONE";
}
auto toString(syzygy::KeyCode const key) -> std::string
{
    switch (key)
    {
    case (syzygy::KeyCode::W):
        return "W";
    case (syzygy::KeyCode::A):
        return "A";
    case (syzygy::KeyCode::S):
        return "S";
    case (syzygy::KeyCode::D):
        return "D";
    case (syzygy::KeyCode::Q):
        return "Q";
    case (syzygy::KeyCode::E):
        return "E";
    case (syzygy::KeyCode::TAB):
        return "TAB";
    case (syzygy::KeyCode::MAX):
    default:
        return "UNKOWN_KEY";
    }
}
} // namespace

namespace syzygy
{

void InputHandler::callbackKey_glfw(
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

void InputHandler::callbackMouse_glfw(
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

void InputHandler::handleKey_glfw(
    int32_t key, int32_t /*scancode*/, int32_t action, int32_t /*mods*/
)
{
    std::optional<KeyCode> const keyResult{toKeyCode_glfw(key)};
    if (!keyResult.has_value())
    {
        return;
    }
    KeyCode const keyCode{keyResult.value()};

    bool& isDown{m_keysNew.keysDown[static_cast<size_t>(keyCode)]};
    isDown = isDown_glfw(isDown, action);
}

void InputHandler::handleMouse_glfw(double xpos, double ypos)
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

auto InputHandler::collect() -> InputSnapshot
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

void InputHandler::setSkipNextCursorDelta(bool const skip)
{
    m_skipNextCursorDelta = skip;
}

auto KeySnapshot::getStatus(KeyCode const key) const -> KeyStatus
{
    return keys[static_cast<size_t>(key)];
}

void KeySnapshot::setStatus(KeyCode const key, KeyStatus const status)
{
    if (getStatus(key) == status)
    {
        return;
    }

    keys[static_cast<size_t>(key)] = status;
}

auto KeyStatus::pressed() const -> bool { return down && edge; }

auto KeyStatus::operator==(KeyStatus const& other) const -> bool
{
    return other.down == down && other.edge == edge;
}

auto CursorSnapshot::delta() const -> glm::i32vec2
{
    return glm::i32vec2{currentPosition} - glm::i32vec2{lastPosition};
}

auto InputSnapshot::format() const -> std::string
{
    std::string output;

    for (size_t index{0}; index < keys.keys.size(); index++)
    {
        KeyCode const keyCode{static_cast<KeyCode>(index)};

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
} // namespace syzygy