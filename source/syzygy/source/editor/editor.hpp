#pragma once

#include "framebuffer.hpp"
#include "graphicscontext.hpp"
#include "swapchain.hpp"
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
    GraphicsContext m_graphics{};
    Swapchain m_swapchain{};
    FrameBuffer m_frameBuffer{};
    Engine* m_renderer{nullptr};

public:
    Editor(Editor const&) = delete;

    Editor& operator=(Editor const&) = delete;

    explicit Editor(
        PlatformWindow const& window,
        GraphicsContext const& graphics,
        Swapchain&& swapchain,
        FrameBuffer const& frameBuffer,
        Engine* const renderer
    )
        : m_window{window}
        , m_graphics{graphics}
        , m_swapchain{std::move(swapchain)}
        , m_frameBuffer{frameBuffer}
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