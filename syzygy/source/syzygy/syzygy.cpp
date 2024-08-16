#include "syzygy/syzygy.hpp"

#include "syzygy/core/log.hpp"
#include "syzygy/editor/editor.hpp"
#include <GLFW/glfw3.h>

namespace syzygy
{
auto runApplication() -> RunResult
{
    if (glfwInit() != GLFW_TRUE)
    {
        SZG_ERROR("Failed to initialize GLFW.");
        return RunResult::FAILURE;
    }

    Logger::initLogging();

    EditorResult const runResult{run()};

    glfwTerminate();

    if (runResult != EditorResult::SUCCESS)
    {
        return RunResult::FAILURE;
    }

    return RunResult::SUCCESS;
}
} // namespace syzygy