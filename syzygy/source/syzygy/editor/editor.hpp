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
    ERROR,
};

namespace szg_editor
{
auto run() -> EditorResult;
}