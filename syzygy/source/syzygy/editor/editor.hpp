#pragma once

enum class EditorResult
{
    SUCCESS,
    ERROR,
};

namespace syzygy
{
auto run() -> EditorResult;
}