#include "input.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/editor/window.hpp"
#include "syzygy/platform/vulkanmacros.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtx/string_cast.hpp>
#include <glm/vec2.hpp>
#include <optional>
#include <spdlog/fmt/bundled/core.h>
#include <unordered_map>

namespace
{
struct InputState
{
    struct KeysState
    {
        std::array<bool, static_cast<size_t>(syzygy::KeyCode::MAX)> keysDown;
    };
    struct CursorState
    {
        glm::u16vec2 position;
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
        glm::u16vec2{static_cast<uint16_t>(xpos), static_cast<uint16_t>(ypos)};
    if (state.skipNextCursorDelta)
    {
        state.cursorOld.position = state.cursorNew.position;
        state.skipNextCursorDelta = false;
        return;
    }
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

    if (previousKeyCallback != nullptr)
    {
        SZG_WARNING("Input Handler overwrote key callback.");
    }
    if (previousCursorPosCallback != nullptr)
    {
        SZG_WARNING("Input Handler overwrote previous cursor pos callback.");
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

    if (previousKeyCallback != callbackKey)
    {
        SZG_WARNING("Input Handler deleted unknown key callback.");
    }
    if (previousCursorPosCallback != callbackCursorPos)
    {
        SZG_WARNING("Input Handler deleted unkown cursor pos callback.");
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

namespace
{

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

        keys.keys[index] = KeyStatus{
            .down = isDown,
            .edge = isDown != oldDown,
        };
    }

    CursorSnapshot cursor{};
    cursor.currentPosition = state.cursorNew.position;
    cursor.lastPosition = state.cursorOld.position;

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