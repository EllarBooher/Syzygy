#pragma once

namespace syzygy
{
enum class RunResult
{
    SUCCESS,
    FAILURE,
};

auto runApplication() -> RunResult;
} // namespace syzygy