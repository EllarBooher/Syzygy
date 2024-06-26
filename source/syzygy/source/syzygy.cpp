#include "syzygy.hpp"

#include "editor/editor.hpp"

auto Application::run() -> ApplicationResult
{
    std::optional<Editor> editor{Editor::create()};

    if (!editor.has_value())
    {
        return ApplicationResult::FAILURE;
    }

    EditorResult const runResult{editor.value().run()};

    if (runResult != EditorResult::SUCCESS)
    {
        return ApplicationResult::FAILURE;
    }

    return ApplicationResult::SUCCESS;
}
