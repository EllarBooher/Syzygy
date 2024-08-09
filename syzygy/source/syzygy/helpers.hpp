#pragma once

#include "syzygy/vulkanusage.hpp"
#include <cassert>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/core.h>
#include <memory>
#include <source_location>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>

#define VKR_ARRAY(x) static_cast<uint32_t>(x.size()), x.data()
#define VKR_ARRAY_NONE 0, nullptr

namespace szg_utils
{
auto ensureAbsolutePath(
    std::filesystem::path const& path,
    std::filesystem::path const& root = std::filesystem::current_path()
) -> std::filesystem::path;
}

#define SZG_LOG(...) spdlog::info(__VA_ARGS__);
#define SZG_WARNING(...) spdlog::warn(__VA_ARGS__);
#define SZG_ERROR(...) spdlog::error(__VA_ARGS__);

// TODO: Support zero variadic arguments
#define SZG_LOG_VK(result_expr, ...)                                           \
    if (VkResult const& SZG_LOG_result{result_expr};                           \
        SZG_LOG_result != VK_SUCCESS)                                          \
    {                                                                          \
        SZG_ERROR(                                                             \
            "VkError {} detected: {}",                                         \
            string_VkResult(SZG_LOG_result),                                   \
            fmt::format(__VA_ARGS__)                                           \
        )                                                                      \
    }

#define SZG_LOG_VKB(result_expr, ...)                                          \
    if (auto const& SZG_VKB_LOG_result{result_expr};                           \
        !SZG_VKB_LOG_result.has_value())                                       \
    {                                                                          \
        vkb::Error const error{SZG_VKB_LOG_result.error()};                    \
        SZG_ERROR(                                                             \
            "vkb::Error ({},{}) detected: {}",                                 \
            string_VkResult(error.vk_result),                                  \
            error.type.message(),                                              \
            fmt::format(__VA_ARGS__)                                           \
        )                                                                      \
    }

#define SZG_CHECK_VK(result_expr)                                              \
    {                                                                          \
        if (VkResult const& SZG_CHECK_result{result_expr};                     \
            SZG_CHECK_result != VK_SUCCESS)                                    \
        {                                                                      \
            SZG_ERROR(                                                         \
                "VkError {} detected.", string_VkResult(SZG_CHECK_result)      \
            )                                                                  \
            assert(SZG_CHECK_result == VK_SUCCESS);                            \
        }                                                                      \
    }

// Thin error propagation, logs any result that isn't VK_SUCCESS
#define SZG_TRY_VK(result_expr, message, return_expr)                          \
    if (VkResult const SYZYGY_result{result_expr};                             \
        SYZYGY_result != VK_SUCCESS)                                           \
    {                                                                          \
        SZG_LOG_VK(SYZYGY_result, message);                                    \
        return return_expr;                                                    \
    }

// Thin error propagation, logs any result that isn't VK_SUCCESS and propagates
// it
#define SZG_PROPAGATE_VK(result_expr, message)                                 \
    if (VkResult const SYZYGY_result{result_expr};                             \
        SYZYGY_result != VK_SUCCESS)                                           \
    {                                                                          \
        SZG_LOG_VK(SYZYGY_result, message);                                    \
        return SYZYGY_result;                                                  \
    }