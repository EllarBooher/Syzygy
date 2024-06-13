#pragma once

#include "window.hpp"
#include <glm/vec2.hpp>
#include <optional>

class Engine;

enum class EditorResult
{
    SUCCESS,
    ERROR_NO_RENDERER,
    ERROR_RENDERER,
    ERROR_EDITOR,
};

class Editor
{
private:
    PlatformWindow m_window{};
    Engine* m_renderer{nullptr};

public:
    Editor(Editor const&) = delete;

    Editor& operator=(Editor const&) = delete;

    Editor(Editor&& other)
        : m_window(other.m_window)
        , m_renderer(other.m_renderer)
    {
        other.m_window = {};
        other.m_renderer = nullptr;
    };

    explicit Editor(PlatformWindow window, Engine* renderer)
        : m_window{window}
        , m_renderer{renderer}
    {
    }

    ~Editor() noexcept;

private:
    Editor() = default;

    static auto createWindow(glm::u16vec2 const extent)
        -> std::optional<PlatformWindow>;

public:
    static auto create() -> std::optional<Editor>;

    auto run() -> EditorResult;
};