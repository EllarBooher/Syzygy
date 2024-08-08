#include "syzygy/syzygy.hpp"

#include <cstdlib>

int main(int argc, char** argv)
{
    auto const runResult{syzygy::runApplication()};

    if (runResult != syzygy::RunResult::SUCCESS)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}