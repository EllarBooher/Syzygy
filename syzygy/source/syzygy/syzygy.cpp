#include "syzygy/syzygy.hpp"

#include "syzygy/editor/editor.hpp"
#include <optional>

auto syzygy::runApplication() -> syzygy::RunResult
{
    std::optional<Editor> editor{Editor::create()};

    if (!editor.has_value())
    {
        return RunResult::FAILURE;
    }

    EditorResult const runResult{editor.value().run()};

    if (runResult != EditorResult::SUCCESS)
    {
        return RunResult::FAILURE;
    }

    return RunResult::SUCCESS;
}
