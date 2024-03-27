#include "helpers.h"

// stringify magic
#define STR(str) #str
#define STRING(str) STR(str)

// The path of the source dir, so we can log just relative paths.
#ifdef SOURCE_DIR
#define LOG_ROOT STRING(SOURCE_DIR)
#else
#define LOG_ROOT ""
#endif

std::string MakeLogPrefix(std::source_location location)
{
    std::string relativePath = std::filesystem::relative(location.file_name(), LOG_ROOT).string();
    return fmt::format(
        "[ {}:{} ]",
        relativePath,
        location.line()
    );
}

void CheckVkResult(VkResult result, std::source_location location)
{
    if (result != VK_SUCCESS)
    {
        auto const message = fmt::format(
            fmt::fg(fmt::color::red),
            "Detected Vulkan Error : {}",
            string_VkResult(result)
        );
        throw std::runtime_error(MakeLogPrefix(location) + message);
    }
}

void Log(std::string message, std::source_location location)
{
    std::string const relativePath = std::filesystem::relative(location.file_name(), LOG_ROOT).string();

    fmt::print(
        fg(fmt::color::gray),
        "{} {}\n",
        MakeLogPrefix(location),
        message
    );
}
