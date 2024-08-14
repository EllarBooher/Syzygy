#include "syzygy/syzygy.hpp"

#include "syzygy/editor/editor.hpp"
#include "syzygy/helpers.hpp"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

auto syzygy::runApplication() -> syzygy::RunResult
{

    if (glfwInit() != GLFW_TRUE)
    {
        SZG_ERROR("Failed to initialize GLFW.");
        return RunResult::FAILURE;
    }

    EditorResult const runResult{szg_editor::run()};

    glfwTerminate();

    if (runResult != EditorResult::SUCCESS)
    {
        return RunResult::FAILURE;
    }

    return RunResult::SUCCESS;
}
