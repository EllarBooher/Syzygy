#pragma once

#include <VkBootstrap.h>
#include <vulkan/vk_enum_string_helper.h>
#include <fmt/color.h>
#include <filesystem>
#include <source_location>

/** Makes a pretty prefix to prepend to log messages. */
std::string MakeLogPrefix(std::source_location location);

/** Checks that a VkResult is a success and throws a runtime error if not. */
void CheckVkResult(VkResult result, std::source_location location = std::source_location::current());

/** Checks that a vkb::Result is a success and throws a runtime error if not. */
template<typename T>
inline T UnwrapVkbResult(vkb::Result<T> result, std::source_location location = std::source_location::current())
{
    if (result.has_value())
    {
        return result.value();
    }

    auto const message = fmt::format(
        fmt::fg(fmt::color::red),
        "Detected Vulkan Bootstrap Error: {}, {}",
        result.error().message(),
        string_VkResult(result.vk_result())
    );
    throw std::runtime_error(MakeLogPrefix(location) + message);
}

/** Logs the message, alongside a prefix that indicates the code location. */
void Log(std::string message, std::source_location location = std::source_location::current());