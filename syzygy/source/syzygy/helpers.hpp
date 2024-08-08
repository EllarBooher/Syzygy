#pragma once

#include "syzygy/vulkanusage.hpp"
#include <VkBootstrap.h>
#include <cassert>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/core.h>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vulkan/vk_enum_string_helper.h>

#define VKR_ARRAY(x) static_cast<uint32_t>(x.size()), x.data()
#define VKR_ARRAY_NONE 0, nullptr

class DebugUtils
{
public:
    static auto ensureAbsoluteFromWorking(std::filesystem::path const& path)
        -> std::filesystem::path;
};

consteval auto sourceLocationRelative(
    std::source_location const location = std::source_location::current()
) -> std::string_view
{
    std::string_view filename{location.file_name()};
    size_t position{filename.find(SZG_LOGGING_SOURCE_DIR)};
    bool const found{position != std::string_view::npos};
    if (found)
    {
        filename.remove_prefix(position);
    }

    return filename;
}

#define SZG_LOG(msg)                                                           \
    {                                                                          \
        fmt::print(                                                            \
            fg(fmt::color::gray),                                              \
            "[ {} ] {} \n",                                                    \
            sourceLocationRelative(),                                          \
            msg                                                                \
        );                                                                     \
    }

#define SZG_WARNING(msg)                                                       \
    {                                                                          \
        fmt::print(                                                            \
            fg(fmt::color::yellow),                                            \
            "[ {} ] {} \n",                                                    \
            sourceLocationRelative(),                                          \
            msg                                                                \
        );                                                                     \
    }

#define SZG_ERROR(msg)                                                         \
    {                                                                          \
        fmt::print(                                                            \
            fg(fmt::color::red), "[ {} ] {} \n", sourceLocationRelative(), msg \
        );                                                                     \
    }

// Makes a pretty prefix to prepend to log messages.
std::string MakeLogPrefix(std::source_location location);

// Checks that a VkResult is a success and throws a runtime error if not.
void CheckVkResult(
    VkResult result,
    std::source_location location = std::source_location::current()
);

// A method used by Imgui's CheckVkResult callback that does not throw.
void CheckVkResult_Imgui(VkResult result);

// Logs a VkResult without throwing, only if it is not VK_SUCCESS.
void LogVkResult(
    VkResult result,
    std::string const& message,
    std::source_location location = std::source_location::current()
);

// Thin error propagation, logs any result that isn't VK_SUCCESS
#define TRY_VK(result_expr, message, return_expr)                              \
    if (VkResult const SYZYGY_result{result_expr};                             \
        SYZYGY_result != VK_SUCCESS)                                           \
    {                                                                          \
        LogVkResult(SYZYGY_result, message);                                   \
        return return_expr;                                                    \
    }

// Thin error propagation, logs any result that isn't VK_SUCCESS and propagates
// it
#define TRY_VK_RESULT(result_expr, message)                                    \
    if (VkResult const SYZYGY_result{result_expr};                             \
        SYZYGY_result != VK_SUCCESS)                                           \
    {                                                                          \
        LogVkResult(SYZYGY_result, message);                                   \
        return SYZYGY_result;                                                  \
    }

template <typename T>
inline void
LogVkbError(vkb::Result<T> const& result, std::string const& message)
{
    SZG_ERROR(fmt::format(
        "{}. Error: {}. VkResult: {}.",
        message,
        result.error().message(),
        string_VkResult(result.vk_result())
    ));
}