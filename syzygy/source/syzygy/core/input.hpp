#pragma once

#include "syzygy/platform/integer.hpp"
#include <array>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <string>

namespace syzygy
{
struct PlatformWindow;
} // namespace syzygy

namespace syzygy
{
struct ButtonStatus
{
    bool down;
    bool edge;

    // Shorthand for down && edge.
    [[nodiscard]] auto pressed() const -> bool;

    // Shorthand for !down && edge.
    [[nodiscard]] auto released() const -> bool;

    auto operator==(ButtonStatus const& other) const -> bool;
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

enum class MouseButtonCode
{
    LEFT,
    RIGHT,
    MIDDLE,
    MISC_4,
    MISC_5,
    MISC_6,
    MISC_7,
    MISC_8,
    MAX
};

auto toString(KeyCode const) -> std::string;
auto toString(MouseButtonCode const) -> std::string;
auto toString(ButtonStatus const) -> std::string;

struct KeySnapshot
{
    std::array<ButtonStatus, static_cast<size_t>(KeyCode::MAX)> keys;

    [[nodiscard]] auto getStatus(KeyCode) const -> ButtonStatus;
    void setStatus(KeyCode, ButtonStatus);
};
struct CursorSnapshot
{
    glm::i64vec2 lastPosition{};
    glm::i64vec2 currentPosition{};

    std::array<ButtonStatus, static_cast<size_t>(MouseButtonCode::MAX)>
        mouseButtons;

    [[nodiscard]] auto getStatus(MouseButtonCode) const -> ButtonStatus;
    void setStatus(MouseButtonCode, ButtonStatus);

    [[nodiscard]] auto delta() const -> glm::i64vec2;
};

struct InputSnapshot
{
    KeySnapshot keys;
    CursorSnapshot cursor;

    [[nodiscard]] auto format() const -> std::string;
};

struct InputHandler
{
public:
    InputHandler(InputHandler const&) = delete;
    auto operator=(InputHandler const&) -> InputHandler& = delete;

    InputHandler(InputHandler&&) noexcept;
    auto operator=(InputHandler&&) noexcept -> InputHandler&;

    ~InputHandler();

private:
    InputHandler() = default;

public:
    static auto create(PlatformWindow const&) -> std::optional<InputHandler>;

    auto collect() -> InputSnapshot;
    void setCursorCaptured(bool captured);

private:
    struct Impl;

    // This pointer cannot be null when initialized via InputHandler::create.
    std::unique_ptr<Impl> m_impl;
};
} // namespace syzygy