#pragma once

#include "syzygy/editor/framebuffer.hpp"
#include "syzygy/editor/graphicscontext.hpp"
#include "syzygy/editor/swapchain.hpp"
#include "syzygy/editor/window.hpp"
#include <optional>
#include <utility>

enum class EditorResult
{
    SUCCESS,
    ERROR_NO_RENDERER,
    ERROR_RENDERER,
    ERROR_EDITOR,
};

class Editor
{
public:
    Editor(Editor&& other) { *this = std::move(other); }

    Editor& operator=(Editor&& other)
    {
        destroy();

        m_window = std::move(other.m_window);
        m_graphics = std::move(other.m_graphics);
        m_swapchain = std::move(other.m_swapchain);
        m_frameBuffer = std::move(other.m_frameBuffer);
        m_initialized = std::exchange(other.m_initialized, false);

        return *this;
    }

    Editor(Editor const&) = delete;
    Editor& operator=(Editor const&) = delete;

    static auto create() -> std::optional<Editor>;

    ~Editor() noexcept { destroy(); }

    auto run() -> EditorResult;

private:
    Editor() = default;

    void destroy();

    explicit Editor(
        PlatformWindow&& window,
        GraphicsContext&& graphics,
        Swapchain&& swapchain,
        FrameBuffer&& frameBuffer
    )
        : m_window{std::move(window)}
        , m_graphics{std::move(graphics)}
        , m_swapchain{std::move(swapchain)}
        , m_frameBuffer{std::move(frameBuffer)}
        , m_initialized{true}
    {
    }

    bool m_initialized{false};

    PlatformWindow m_window{};
    GraphicsContext m_graphics{};
    Swapchain m_swapchain{};
    FrameBuffer m_frameBuffer{};
};