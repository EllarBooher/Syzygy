#pragma once

#include <memory>
#include <spdlog/logger.h>

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

#define SZG_TRACE(...) syzygy::Logger::getLogger().trace(__VA_ARGS__);
#define SZG_INFO(...) syzygy::Logger::getLogger().info(__VA_ARGS__);
#define SZG_WARNING(...) syzygy::Logger::getLogger().warn(__VA_ARGS__);
#define SZG_ERROR(...) syzygy::Logger::getLogger().error(__VA_ARGS__);
#define SZG_CRITICAL(...) syzygy::Logger::getLogger().critical(__VA_ARGS__);