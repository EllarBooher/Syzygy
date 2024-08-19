#pragma once

#include "syzygy/platform/integer.hpp"
#include <array>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <string>

namespace syzygy
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

struct PlatformWindow;

class InputHandler
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