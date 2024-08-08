#include "helpers.hpp"

#include <fmt/core.h>

auto MakeLogPrefix(std::source_location const location) -> std::string
{
    /*
    std::string const relativePath{DebugUtils::getLoadedDebugUtils()
                                       .makeRelativePath(location.file_name())
                                       .string()};
    return fmt::format("[ {}:{} ]", relativePath, location.line());
    */
    return fmt::format("[ {} ]", "HI :)");
}

namespace
{
void PrintLine(std::string const& message, fmt::color const foregroundColor)
{
    fmt::print(fg(foregroundColor), "{}\n", message);
}
} // namespace

void CheckVkResult(VkResult const result, std::source_location const location)
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    std::string const message{fmt::format(
        fmt::fg(fmt::color::red),
        "Detected Vulkan Error : {}",
        string_VkResult(result)
    )};
    throw std::runtime_error(MakeLogPrefix(location) + message);
}

void CheckVkResult_Imgui(VkResult const result)
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    std::string const message{
        fmt::format("Detected Vulkan Error : {}", string_VkResult(result))
    };
    PrintLine(message, fmt::color::red);
}

void LogVkResult(
    VkResult const result,
    std::string const& message,
    std::source_location const location
)
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    SZG_ERROR(fmt::format("{} error: {}", message, string_VkResult(result)));
}

auto DebugUtils::ensureAbsoluteFromWorking(std::filesystem::path const& path)
    -> std::filesystem::path
{
    if (path.is_absolute())
    {
        return path;
    }

    return std::filesystem::current_path() / path;
}
