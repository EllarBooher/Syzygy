#include "application.hpp"

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