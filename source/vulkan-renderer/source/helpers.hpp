#pragma once

#include <VkBootstrap.h>
#include <vulkan/vk_enum_string_helper.h>
#include <fmt/color.h>
#include <filesystem>
#include <source_location>

#define VKR_ARRAY(x) static_cast<uint32_t>(x.size()), x.data()
#define VKR_ARRAY_NONE 0, nullptr

class DebugUtils
{
public:
    static void init();
    static DebugUtils const& getLoadedDebugUtils()
    {
        if (m_loadedDebugUtils == nullptr)
        {
            init();
        }

        return *m_loadedDebugUtils;
    }

private:

    std::filesystem::path m_sourcePath{};

    inline static std::unique_ptr<DebugUtils> m_loadedDebugUtils{ nullptr };

public:
    
    // Returns whether or not a relative path is valid. 
    // A relative path is valid when:
    // - As an std::filesystem::path, it is relative.
    // - Appending it to an absolute path does not escape the directory 
    // defined by that absolute path.
    static bool validateRelativePath(const std::filesystem::path &path);

    // Returns the absolute path to a file on disk specified by the path 
    // relative to the project's root. 
    // Asserts that the path is valid as defined by validateRelativePath.
    std::filesystem::path makeAbsolutePath(const std::filesystem::path &localPath) const;

    // Returns the absolute path to an (assumed) source relative path.
    // The absolute path to the file when it exists, nullptr otherwise.
    std::unique_ptr<std::filesystem::path> loadAssetPath(const std::filesystem::path &localPath) const;

    // Given an absolute path on disk, returns the portion relative 
    // to the project's root.
    // Asserts that the path is valid as defined by validateRelativePath.
    std::filesystem::path makeRelativePath(const std::filesystem::path &absolutePath) const;
};

// Makes a pretty prefix to prepend to log messages.
std::string MakeLogPrefix(std::source_location location);

// Checks that a VkResult is a success and throws a runtime error if not.
void CheckVkResult(
    VkResult result
    , std::source_location location = std::source_location::current()
);

// A method used by Imgui's CheckVkResult callback that does not throw.
void CheckVkResult_Imgui(VkResult result);

// Logs a VkResult without throwing, only if it is not VK_SUCCESS.
void LogVkResult(
    VkResult result, const std::string &message, std::source_location location = std::source_location::current()
);

// Returns the value inside a vkb::Result if it is a success 
// and throws a runtime error if not.
template<typename T>
inline T UnwrapVkbResult(
    vkb::Result<T> const result
    , std::source_location const location = std::source_location::current()
)
{
    if (result.has_value())
    {
        return result.value();
    }

    auto const message = fmt::format(
        fmt::fg(fmt::color::red)
        , "Detected Vulkan Bootstrap Error: {}, {}"
        , result.error().message()
        , string_VkResult(result.vk_result())
    );
    throw std::runtime_error(MakeLogPrefix(location) + message);
}

// Logs the message in grey, 
// alongside a prefix that indicates the code location.
void Log(const std::string &message, std::source_location location = std::source_location::current());

// Logs the message in grey, 
// alongside a prefix that indicates the code location.
void Warning(const std::string &message, std::source_location location = std::source_location::current());

// Logs the message in red, 
// alongside a prefix that indicates the code location.
void Error(const std::string &message, std::source_location location = std::source_location::current());
