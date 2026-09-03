#pragma once

#include <string>
#include <fstream>
#include <mutex>

namespace ota {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

std::string log_level_to_string(LogLevel level);

LogLevel string_to_log_level(const std::string& str);

class Logger {
public:
    static Logger& instance();

    bool initialize(const std::string& log_dir, LogLevel min_level = LogLevel::INFO);

    void set_level(LogLevel level);

    void debug(const std::string& component, const std::string& message);

    void info(const std::string& component, const std::string& message);

    void warn(const std::string& component, const std::string& message);

    void error(const std::string& component, const std::string& message);

    void shutdown();

private:
    Logger() = default;

    void log(LogLevel level, const std::string& component, const std::string& message);

    std::string format_timestamp();

    std::ofstream log_file_;
    LogLevel min_level_ = LogLevel::INFO;
    std::mutex mutex_;
    bool initialized_ = false;
};

}
