#include "helpers.hpp"

#include <fmt/core.h>

auto MakeLogPrefix(std::source_location const location) -> std::string
{
    std::string const relativePath{DebugUtils::getLoadedDebugUtils()
                                       .makeRelativePath(location.file_name())
                                       .string()};
    return fmt::format("[ {}:{} ]", relativePath, location.line());
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

    Error(
        fmt::format("{} error: {}", message, string_VkResult(result)), location
    );
}

void LogBase(
    std::string const& message,
    std::source_location const location,
    fmt::color const color
)
{
    PrintLine(fmt::format("{} {}", MakeLogPrefix(location), message), color);
}

void Log(std::string const& message, std::source_location const location)
{
    LogBase(message, location, fmt::color::gray);
}

void Warning(std::string const& message, std::source_location const location)
{
    LogBase(message, location, fmt::color::yellow);
}

void Error(std::string const& message, std::source_location const location)
{
    LogBase(message, location, fmt::color::red);
}

void DebugUtils::init()
{
    std::filesystem::path const sourcePath{
        std::filesystem::weakly_canonical(std::filesystem::path{SOURCE_DIR})
    };

    if (!std::filesystem::exists(sourcePath)
        || !std::filesystem::is_directory(sourcePath))
    {
        throw std::runtime_error(
            "DebugUtils::Init failure: "
            "Source path does not point to valid directory."
        );
    }

    m_loadedDebugUtils = std::make_unique<DebugUtils>();
    m_loadedDebugUtils->m_sourcePath = sourcePath;

    PrintLine(
        fmt::format(
            "DebugUtils::Init success: Source path is \"{}\"",
            m_loadedDebugUtils->m_sourcePath.string()
        ),
        fmt::color::gray
    );
}

auto DebugUtils::validateRelativePath(std::filesystem::path const& path) -> bool
{
    if (!path.is_relative())
    {
        return false;
    }

    std::string const firstDir{(*path.lexically_normal().begin()).string()};
    return firstDir != "..";
}

auto DebugUtils::makeAbsolutePath(std::filesystem::path const& localPath) const
    -> std::filesystem::path
{
    return (m_sourcePath / localPath).lexically_normal();
}

auto DebugUtils::loadAssetPath(std::filesystem::path const& localPath) const
    -> std::unique_ptr<std::filesystem::path>
{
    if (!validateRelativePath(localPath))
    {
        return nullptr;
    }
    return std::make_unique<std::filesystem::path>(makeAbsolutePath(localPath));
}

auto DebugUtils::makeRelativePath(std::filesystem::path const& absolutePath
) const -> std::filesystem::path
{
    assert(absolutePath.is_absolute());

    std::filesystem::path const relativePortion{
        absolutePath.lexically_relative(m_sourcePath).lexically_normal()
    };

    assert(validateRelativePath(relativePortion));

    return relativePortion.lexically_normal();
}
