#include "syzygy/syzygy.hpp"

#include "syzygy/editor/editor.hpp"

auto syzygy::runApplication() -> syzygy::RunResult
{
    EditorResult const runResult{szg_editor::run()};

    if (runResult != EditorResult::SUCCESS)
    {
        return RunResult::FAILURE;
    }

    return RunResult::SUCCESS;
}
