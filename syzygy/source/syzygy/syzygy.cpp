#include "syzygy/syzygy.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/editor/editor.hpp"
#include <GLFW/glfw3.h>

auto syzygy::runApplication() -> syzygy::RunResult
{
    if (glfwInit() != GLFW_TRUE)
    {
        SZG_ERROR("Failed to initialize GLFW.");
        return RunResult::FAILURE;
    }

    szg_log::Logger::initLogging();

    EditorResult const runResult{szg_editor::run()};

    glfwTerminate();

    if (runResult != EditorResult::SUCCESS)
    {
        return RunResult::FAILURE;
    }

    return RunResult::SUCCESS;
}
