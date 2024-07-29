#include "input.hpp"

#include <fmt/core.h>

namespace
{
auto incrementKeyStatus(szg_input::KeyStatus const status)
    -> szg_input::KeyStatus
{
    switch (status)
    {
    case (szg_input::KeyStatus::HELD):
    case (szg_input::KeyStatus::PRESSED):
        return szg_input::KeyStatus::HELD;
    case (szg_input::KeyStatus::RELEASED):
    case (szg_input::KeyStatus::NONE):
        return szg_input::KeyStatus::NONE;
    }
}
auto toKeyCode_glfw(int32_t key) -> szg_input::KeyCode
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
    default:
        return szg_input::KeyCode::D;
    }
}
auto transitionStatus_glfw(
    szg_input::KeyStatus const status, int32_t const action
) -> szg_input::KeyStatus
{
    switch (action)
    {
    case (GLFW_REPEAT):
        if (status == szg_input::KeyStatus::HELD)
        {
            return szg_input::KeyStatus::HELD;
        }
        return szg_input::KeyStatus::PRESSED;
    case (GLFW_PRESS):
        return szg_input::KeyStatus::PRESSED;
    case (GLFW_RELEASE):
        return szg_input::KeyStatus::RELEASED;
    default:
        return szg_input::KeyStatus::NONE;
    }
}
auto toString(szg_input::KeyStatus const status) -> std::string
{
    switch (status)
    {
    case (szg_input::KeyStatus::HELD):
        return "HELD";
    case (szg_input::KeyStatus::PRESSED):
        return "PRESSED";
    case (szg_input::KeyStatus::RELEASED):
        return "RELEASED";
    case (szg_input::KeyStatus::NONE):
        return "NONE";
    }
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
    }
}
} // namespace

void szg_input::InputHandler::callback_glfw(
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

    activeHandler->handle_glfw(key, scancode, action, mods);
}

void szg_input::InputHandler::handle_glfw(
    int32_t key, int32_t scancode, int32_t action, int32_t mods
)
{
    szg_input::KeyCode const keyCode{toKeyCode_glfw(key)};

    szg_input::KeyStatus const oldStatus{m_snapshot.getStatus(keyCode)};
    szg_input::KeyStatus const newStatus{transitionStatus_glfw(oldStatus, action)};

    m_snapshot.setStatus(keyCode, newStatus);
}

void szg_input::InputHandler::increment()
{
    InputSnapshot const oldSnapshot{m_snapshot};

    std::array<
        szg_input::KeyStatus,
        static_cast<size_t>(szg_input::KeyCode::MAX)>
        keys{};

    bool dirty{false};
    for (size_t index{0}; index < keys.size(); index++)
    {
        szg_input::KeyStatus const oldStatus{oldSnapshot.keys[index]};
        szg_input::KeyStatus const newStatus{incrementKeyStatus(oldStatus)};
        keys[index] = newStatus;

        dirty |= oldStatus != newStatus;
    };

    m_snapshot = InputSnapshot{
        .dirty = dirty,
        .keys = keys,
    };
}

auto szg_input::InputHandler::formatStatus() -> std::string
{
    InputSnapshot& snapshot{m_snapshot};

    std::string output;

    for (size_t index{0}; index < snapshot.keys.size(); index++)
    {
        szg_input::KeyCode const key{static_cast<szg_input::KeyCode>(index)};

        output += fmt::format(
            "{}: {:9}", toString(key), toString(snapshot.getStatus(key))
        );
    }

    return output;
}

auto szg_input::InputHandler::collect() const -> InputSnapshot
{
    return m_snapshot;
}

auto szg_input::InputSnapshot::getStatus(KeyCode const key) const -> KeyStatus
{
    return keys[static_cast<size_t>(key)];
}

void szg_input::InputSnapshot::setStatus(
    KeyCode const key, KeyStatus const status
)
{
    if (getStatus(key) == status)
    {
        return;
    }

    dirty = true;
    keys[static_cast<size_t>(key)] = status;
}
