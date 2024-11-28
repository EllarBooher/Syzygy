#include "input.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/editor/window.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtx/string_cast.hpp>
#include <glm/vec2.hpp>
#include <optional>
#include <spdlog/fmt/bundled/core.h>
#include <unordered_map>
#include <utility>

namespace
{
struct InputState
{
    struct KeysState
    {
        static size_t constexpr KEY_COUNT{
            static_cast<size_t>(syzygy::KeyCode::MAX)
        };
        std::array<bool, KEY_COUNT> keysDown;
    };
    struct CursorState
    {
        static size_t constexpr MOUSE_BUTTON_COUNT{
            static_cast<size_t>(syzygy::MouseButtonCode::MAX)
        };
        std::array<bool, MOUSE_BUTTON_COUNT> buttonsDown;

        glm::i64vec2 position;
    };

    bool skipNextCursorDelta{};

    KeysState keysOld;
    CursorState cursorOld;

    KeysState keysNew;
    CursorState cursorNew;
};
} // namespace

namespace detail_glfw
{
std::unordered_map<GLFWwindow*, InputState> s_GLFWstates{};

auto keyToKeyCode(int32_t key) -> std::optional<syzygy::KeyCode>
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

auto mouseButtonToCode(int32_t button) -> std::optional<syzygy::MouseButtonCode>
{
    // Check this to make sure the switch statement is well-formed
    static_assert(GLFW_MOUSE_BUTTON_LEFT == GLFW_MOUSE_BUTTON_1);
    static_assert(GLFW_MOUSE_BUTTON_RIGHT == GLFW_MOUSE_BUTTON_2);
    static_assert(GLFW_MOUSE_BUTTON_MIDDLE == GLFW_MOUSE_BUTTON_3);

    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        return syzygy::MouseButtonCode::LEFT;
    case GLFW_MOUSE_BUTTON_RIGHT:
        return syzygy::MouseButtonCode::RIGHT;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        return syzygy::MouseButtonCode::MIDDLE;
    case GLFW_MOUSE_BUTTON_4:
        return syzygy::MouseButtonCode::MISC_4;
    case GLFW_MOUSE_BUTTON_5:
        return syzygy::MouseButtonCode::MISC_5;
    case GLFW_MOUSE_BUTTON_6:
        return syzygy::MouseButtonCode::MISC_6;
    case GLFW_MOUSE_BUTTON_7:
        return syzygy::MouseButtonCode::MISC_7;
    case GLFW_MOUSE_BUTTON_8:
        return syzygy::MouseButtonCode::MISC_8;
    default:
        return std::nullopt;
    }
}

auto isDownFromAction(bool const currentDown, int32_t const action) -> bool
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

void callbackKey(
    GLFWwindow* const window,
    int const key,
    int const /*scancode*/,
    int const action,
    int const /*mods*/
)
{
    if (!s_GLFWstates.contains(window))
    {
        return;
    }

    InputState& state{s_GLFWstates.at(window)};

    std::optional<syzygy::KeyCode> const keyResult{keyToKeyCode(key)};
    if (!keyResult.has_value())
    {
        return;
    }
    syzygy::KeyCode const keyCode{keyResult.value()};

    bool& isDown{state.keysNew.keysDown[static_cast<size_t>(keyCode)]};
    isDown = isDownFromAction(isDown, action);
}

void callbackCursorPos(
    GLFWwindow* const window, double const xpos, double const ypos
)
{
    if (!s_GLFWstates.contains(window))
    {
        return;
    }

    InputState& state{s_GLFWstates.at(window)};

    state.cursorNew.position =
        glm::i64vec2{static_cast<int64_t>(xpos), static_cast<int64_t>(ypos)};
    if (state.skipNextCursorDelta)
    {
        state.cursorOld.position = state.cursorNew.position;
        state.skipNextCursorDelta = false;
        return;
    }
}

void callbackMouseButton(
    GLFWwindow* const window, int const button, int const action, int const mods
)
{
    if (!s_GLFWstates.contains(window))
    {
        return;
    }

    InputState& state{s_GLFWstates.at(window)};
    std::optional<syzygy::MouseButtonCode> const buttonResult{
        mouseButtonToCode(button)
    };
    if (!buttonResult.has_value())
    {
        return;
    }
    syzygy::MouseButtonCode const buttonCode{buttonResult.value()};

    bool& isDown{state.cursorNew.buttonsDown[static_cast<size_t>(buttonCode)]};
    isDown = isDownFromAction(isDown, action);
}

// Returns true on success
auto registerWindow(GLFWwindow* const handle) -> bool
{
    if (handle == nullptr)
    {
        SZG_ERROR("Input Handler tried to register null GLFWwindow.");
        return false;
    }

    if (detail_glfw::s_GLFWstates.contains(handle))
    {
        SZG_ERROR(
            "Input Handler tried to register already-registered GLFWwindow."
        );
        return false;
    }

    detail_glfw::s_GLFWstates.emplace(handle, InputState{});

    auto* const previousKeyCallback{glfwSetKeyCallback(handle, callbackKey)};
    auto* const previousCursorPosCallback{
        glfwSetCursorPosCallback(handle, callbackCursorPos)
    };
    auto* const previousMouseButtonCallback{
        glfwSetMouseButtonCallback(handle, callbackMouseButton)
    };

    if (previousKeyCallback != nullptr)
    {
        SZG_WARNING("Input Handler overwrote key callback.");
    }
    if (previousCursorPosCallback != nullptr)
    {
        SZG_WARNING("Input Handler overwrote previous cursor pos callback.");
    }
    if (previousMouseButtonCallback != nullptr)
    {
        SZG_WARNING("Input Handler overwrote previous mouse button callback.");
    }

    return true;
}

void unregisterWindow(GLFWwindow* const handle)
{
    if (handle == nullptr)
    {
        SZG_ERROR("Input Handler tried to unregister null GLFWwindow.");
        return;
    }

    if (!detail_glfw::s_GLFWstates.contains(handle))
    {
        SZG_ERROR("Input Handler tried to unregister not-registered GLFWwindow."
        );
        return;
    }

    detail_glfw::s_GLFWstates.erase(handle);

    auto* const previousKeyCallback{glfwSetKeyCallback(handle, nullptr)};
    auto* const previousCursorPosCallback{
        glfwSetCursorPosCallback(handle, nullptr)
    };
    auto* const previousMouseButtonCallback{
        glfwSetMouseButtonCallback(handle, nullptr)
    };

    if (previousKeyCallback != callbackKey)
    {
        SZG_WARNING("Input Handler deleted unknown key callback.");
    }
    if (previousCursorPosCallback != callbackCursorPos)
    {
        SZG_WARNING("Input Handler deleted unkown cursor pos callback.");
    }
    if (previousMouseButtonCallback != callbackMouseButton)
    {
        SZG_WARNING("Input Handler deleted unknown mouse button callback.");
    }
}

} // namespace detail_glfw

namespace syzygy
{
struct InputHandler::Impl
{

public:
    Impl(Impl const&) = delete;
    auto operator=(Impl const&) -> Impl& = delete;

    Impl(Impl&& other) noexcept { *this = std::move(other); };
    auto operator=(Impl&& other) noexcept -> Impl&
    {
        m_window = std::exchange(other.m_window, nullptr);
        return *this;
    };

    ~Impl() { detail_glfw::unregisterWindow(m_window); }

    static auto create(GLFWwindow* const handle)
        -> std::optional<std::unique_ptr<Impl>>
    {
        if (!detail_glfw::registerWindow(handle))
        {
            return std::nullopt;
        }

        std::optional<std::unique_ptr<Impl>> result{new Impl{}};

        result.value()->m_window = handle;

        return result;
    }

    void setCursorEnabled(bool const enabled)
    {
        glfwSetInputMode(
            m_window,
            GLFW_CURSOR,
            enabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED
        );
    }

    auto getState() -> InputState&
    {
        // Throw on out-of-bounds access is OK since InputState should never be
        // missing
        return detail_glfw::s_GLFWstates.at(m_window);
    }

private:
    Impl() = default;

    GLFWwindow* m_window{};
};
} // namespace syzygy

namespace syzygy
{

auto toString(ButtonStatus const status) -> std::string
{
    if (status.down)
    {
        return status.edge ? "PRESSED" : "HELD";
    }

    return status.edge ? "RELEASED" : "NONE";
}
auto toString(KeyCode const key) -> std::string
{
    switch (key)
    {
    case (KeyCode::W):
        return "W";
    case (KeyCode::A):
        return "A";
    case (KeyCode::S):
        return "S";
    case (KeyCode::D):
        return "D";
    case (KeyCode::Q):
        return "Q";
    case (KeyCode::E):
        return "E";
    case (KeyCode::TAB):
        return "Tab";
    case (KeyCode::MAX):
    default:
        return "Unknown Keyboard Key";
    }
}

auto toString(MouseButtonCode const mouseButton) -> std::string
{
    switch (mouseButton)
    {
    case (MouseButtonCode::LEFT):
        return "Left Mouse Button";
    case (MouseButtonCode::RIGHT):
        return "Right Mouse Button";
    case (MouseButtonCode::MIDDLE):
        return "Middle Mouse Button";
    case (MouseButtonCode::MISC_4):
        return "Mouse Button 4";
    case (MouseButtonCode::MISC_5):
        return "Mouse Button 5";
    case (MouseButtonCode::MISC_6):
        return "Mouse Button 6";
    case (MouseButtonCode::MISC_7):
        return "Mouse Button 7";
    case (MouseButtonCode::MISC_8):
        return "Mouse Button 8";
    case (MouseButtonCode::MAX):
    default:
        return "Unknown Mouse Button";
    }
}

InputHandler::~InputHandler() { m_impl.reset(); }
auto InputHandler::create(PlatformWindow const& window)
    -> std::optional<InputHandler>
{
    GLFWwindow* const windowHandle{window.handle()};

    std::optional<InputHandler> handlerResult{InputHandler{}};
    InputHandler& handler{handlerResult.value()};

    auto implResult{Impl::create(windowHandle)};
    if (!implResult.has_value() || implResult.value() == nullptr)
    {
        return std::nullopt;
    }

    handler.m_impl = std::move(implResult).value();

    return handlerResult;
}

auto InputHandler::collect() -> InputSnapshot
{
    InputState& state{m_impl->getState()};

    KeySnapshot keys{};
    for (size_t index{0}; index < state.keysNew.keysDown.size(); index++)
    {
        bool const oldDown{state.keysOld.keysDown[index]};
        bool const isDown{state.keysNew.keysDown[index]};

        keys.keys[index] = ButtonStatus{
            .down = isDown,
            .edge = isDown != oldDown,
        };
    }

    CursorSnapshot cursor{};
    cursor.currentPosition = state.cursorNew.position;
    cursor.lastPosition = state.cursorOld.position;
    for (size_t index{0}; index < state.cursorNew.buttonsDown.size(); index++)
    {
        bool const oldDown{state.cursorOld.buttonsDown[index]};
        bool const isDown{state.cursorNew.buttonsDown[index]};

        cursor.mouseButtons[index] = ButtonStatus{
            .down = isDown,
            .edge = isDown != oldDown,
        };
    }

    state.cursorOld = state.cursorNew;
    state.keysOld = state.keysNew;

    return {
        .keys = keys,
        .cursor = cursor,
    };
}

void InputHandler::setCursorCaptured(bool captured)
{
    InputState& state{m_impl->getState()};

    m_impl->setCursorEnabled(!captured);

    state.skipNextCursorDelta = true;
}

InputHandler::InputHandler(InputHandler&& other) noexcept
{
    *this = std::move(other);
}

auto InputHandler::operator=(InputHandler&& other) noexcept -> InputHandler&
{
    m_impl = std::move(other.m_impl);
    return *this;
}

auto KeySnapshot::getStatus(KeyCode const key) const -> ButtonStatus
{
    return keys[static_cast<size_t>(key)];
}

void KeySnapshot::setStatus(KeyCode const key, ButtonStatus const status)
{
    if (getStatus(key) == status)
    {
        return;
    }

    keys[static_cast<size_t>(key)] = status;
}

auto ButtonStatus::pressed() const -> bool { return down && edge; }

auto ButtonStatus::released() const -> bool { return !down && edge; }

auto ButtonStatus::operator==(ButtonStatus const& other) const -> bool
{
    return other.down == down && other.edge == edge;
}

auto CursorSnapshot::getStatus(MouseButtonCode const mouseButton) const
    -> ButtonStatus
{
    return mouseButtons[static_cast<size_t>(mouseButton)];
}

void CursorSnapshot::setStatus(
    MouseButtonCode const mouseButton, ButtonStatus const status
)
{
    mouseButtons[static_cast<size_t>(mouseButton)] = status;
}

auto CursorSnapshot::delta() const -> glm::i64vec2
{
    return glm::i64vec2{currentPosition} - glm::i64vec2{lastPosition};
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