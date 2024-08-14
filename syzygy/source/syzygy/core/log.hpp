#pragma once

#include <spdlog/spdlog.h>

namespace szg_log
{
class Logger
{
public:
    static auto getLogger() -> spdlog::logger&;
    static void initLogging();

private:
    static std::shared_ptr<spdlog::logger> m_logger;
};
} // namespace szg_log

#define SZG_TRACE(...) szg_log::Logger::getLogger().trace(__VA_ARGS__);
#define SZG_LOG(...) szg_log::Logger::getLogger().info(__VA_ARGS__);
#define SZG_WARNING(...) szg_log::Logger::getLogger().warn(__VA_ARGS__);
#define SZG_ERROR(...) szg_log::Logger::getLogger().error(__VA_ARGS__);
#define SZG_CRITICAL(...) szg_log::Logger::getLogger().critical(__VA_ARGS__);