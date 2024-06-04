#include "application.hpp"

#include <stdlib.h>

int main()
{
    Application application{};

    if (!application.loadedSuccessfully())
    {
        return EXIT_FAILURE;
    }

    application.run();

    return EXIT_SUCCESS;
}