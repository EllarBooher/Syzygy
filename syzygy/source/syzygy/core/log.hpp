#pragma once

#include <memory>

#ifndef SPDLOG_ACTIVE_LEVEL
#ifdef SZG_DEBUG_BUILD
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif
#endif

#include <spdlog/logger.h> // IWYU pragma: keep
#include <spdlog/spdlog.h>

namespace syzygy
{
class Logger
{
public:
    static auto getLogger() -> spdlog::logger&;
    static void initLogging();

private:
    static std::shared_ptr<spdlog::logger> m_logger;
};
} // namespace syzygy

#define SZG_TRACE(...)                                                         \
    SPDLOG_LOGGER_TRACE(&syzygy::Logger::getLogger(), __VA_ARGS__);
#define SZG_DEBUG(...)                                                         \
    SPDLOG_LOGGER_DEBUG(&syzygy::Logger::getLogger(), __VA_ARGS__);
#define SZG_INFO(...)                                                          \
    SPDLOG_LOGGER_INFO(&syzygy::Logger::getLogger(), __VA_ARGS__);
#define SZG_WARNING(...)                                                       \
    SPDLOG_LOGGER_WARN(&syzygy::Logger::getLogger(), __VA_ARGS__);
#define SZG_ERROR(...)                                                         \
    SPDLOG_LOGGER_ERROR(&syzygy::Logger::getLogger(), __VA_ARGS__);
#define SZG_CRITICAL(...)                                                      \
    SPDLOG_LOGGER_CRITICAL(&syzygy::Logger::getLogger(), __VA_ARGS__);