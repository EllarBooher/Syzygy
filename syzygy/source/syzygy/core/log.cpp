#include "log.hpp"

#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace szg_log
{
std::shared_ptr<spdlog::logger> Logger::m_logger{};

auto Logger::getLogger() -> spdlog::logger& { return *m_logger; }

void Logger::initLogging()
{
    spdlog::set_pattern("[%T] [%^%=7l%$] %v");

    auto consoleSink{std::make_shared<spdlog::sinks::stdout_color_sink_st>()};
    auto fileSink{
        std::make_shared<spdlog::sinks::basic_file_sink_st>("Syzygy.log", true)
    };

    consoleSink->set_pattern("[%T] %^%=8l%$: %v");
    fileSink->set_pattern("[%T] [%l] %v");

    std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
    m_logger =
        std::make_shared<spdlog::logger>("SYZYGY", sinks.begin(), sinks.end());

    spdlog::register_logger(m_logger);
    m_logger->set_level(spdlog::level::trace);
    m_logger->flush_on(spdlog::level::trace);
}
} // namespace szg_log
