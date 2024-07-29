#include "syzygy/syzygy.hpp"

#include <cstdlib>

auto main() -> int
{
    syzygy::RunResult const runResult{syzygy::runApplication()};

    if (runResult != syzygy::RunResult::SUCCESS)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}