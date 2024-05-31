#include "helpers.hpp"

#include <iostream>
#include <algorithm>
#include <regex>
#include <array>

std::string MakeLogPrefix(std::source_location location)
{
    std::string const relativePath = DebugUtils::getLoadedDebugUtils().makeRelativePath(location.file_name()).string();
    return fmt::format(
        "[ {}:{} ]",
        relativePath,
        location.line()
    );
}

static void PrintLine(std::string message, fmt::color foregroundColor)
{
    fmt::print(
        fg(foregroundColor),
        "{}\n",
        message
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

void CheckVkResult_Imgui(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        auto const message = fmt::format(
            "Detected Vulkan Error : {}",
            string_VkResult(result)
        );
        PrintLine(message, fmt::color::red);
    }
}

void LogVkResult(
    VkResult result
    , std::string message
    , std::source_location location
)
{
    if (result == VK_SUCCESS)
    {
        return;
    }

    Error(fmt::format("{} error: {}", message, string_VkResult(result)), location);
}

void LogBase(std::string message, std::source_location location, fmt::color color)
{
    PrintLine(fmt::format("{} {}", MakeLogPrefix(location), message), color);
}

void Log(std::string message, std::source_location location)
{
    LogBase(message, location, fmt::color::gray);
}

void Warning(std::string message, std::source_location location)
{
    LogBase(message, location, fmt::color::yellow);
}

void Error(std::string message, std::source_location location)
{
    LogBase(message, location, fmt::color::red);
}

void DebugUtils::init()
{
    std::filesystem::path const sourcePath{ std::filesystem::weakly_canonical(std::filesystem::path{ SOURCE_DIR }) };

    if (!std::filesystem::exists(sourcePath) || !std::filesystem::is_directory(sourcePath))
    {
        throw std::runtime_error("DebugUtils::Init failure: Source path does not point to valid directory.");
    }

    m_loadedDebugUtils = std::make_unique<DebugUtils>();
    m_loadedDebugUtils->m_sourcePath = sourcePath;

    PrintLine(fmt::format("DebugUtils::Init success: Source path is \"{}\"", m_loadedDebugUtils->m_sourcePath.string()), fmt::color::gray);
}

bool DebugUtils::validateRelativePath(std::filesystem::path path)
{
    if (!path.is_relative())
    {
        return false;
    }

    std::string const firstDir{ (*path.lexically_normal().begin()).string()};
    if (firstDir == "..")
    {
        return false;
    }

    return true;
}

std::filesystem::path DebugUtils::makeAbsolutePath(std::filesystem::path localPath) const
{
    return (m_sourcePath / localPath).lexically_normal();
}

std::unique_ptr<std::filesystem::path> DebugUtils::loadAssetPath(std::filesystem::path localPath) const
{
    if (!validateRelativePath(localPath))
    {
        return nullptr;
    }
    return std::make_unique<std::filesystem::path>(makeAbsolutePath(localPath));
}

std::filesystem::path DebugUtils::makeRelativePath(std::filesystem::path absolutePath) const
{
    assert(absolutePath.is_absolute());
    std::filesystem::path const relativePortion = absolutePath.lexically_relative(m_sourcePath).lexically_normal();

    assert(validateRelativePath(relativePortion));

    return relativePortion.lexically_normal();
}
