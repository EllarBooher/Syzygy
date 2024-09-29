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
struct KeyStatus
{
    bool down;
    bool edge;

    [[nodiscard]] auto pressed() const -> bool;

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

    [[nodiscard]] auto getStatus(KeyCode) const -> KeyStatus;
    void setStatus(KeyCode, KeyStatus);
};
struct CursorSnapshot
{
    glm::i64vec2 lastPosition{};
    glm::i64vec2 currentPosition{};

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