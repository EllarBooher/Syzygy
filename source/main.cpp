#include "syzygy.hpp"

#include <cstdlib>

auto main() -> int
{
    ApplicationResult const runResult{Application::run()};

    if (runResult != ApplicationResult::SUCCESS)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}