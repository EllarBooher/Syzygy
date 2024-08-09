#include "syzygy/syzygy.hpp"

#include "syzygy/editor/editor.hpp"
#include <spdlog/spdlog.h>

auto syzygy::runApplication() -> syzygy::RunResult
{
    EditorResult const runResult{szg_editor::run()};

    if (runResult != EditorResult::SUCCESS)
    {
        return RunResult::FAILURE;
    }

    return RunResult::SUCCESS;
}
