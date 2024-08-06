#pragma once

enum class EditorResult
{
    SUCCESS,
    ERROR,
};

namespace szg_editor
{
auto run() -> EditorResult;
}