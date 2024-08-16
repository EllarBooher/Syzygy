#pragma once

namespace syzygy
{
enum class EditorResult
{
    SUCCESS,
    ERROR,
};
auto run() -> EditorResult;
} // namespace syzygy