#include "logging/logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace ota {

std::string log_level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

LogLevel string_to_log_level(const std::string& str) {
    if (str == "DEBUG") return LogLevel::DEBUG;
    if (str == "INFO")  return LogLevel::INFO;
    if (str == "WARN")  return LogLevel::WARN;
    if (str == "ERROR") return LogLevel::ERROR;
    return LogLevel::INFO;
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

bool Logger::initialize(const std::string& log_dir, LogLevel min_level) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    if (ec) {
        return false;
    }

    std::string log_file = log_dir + "/ota.log";
    log_file_.open(log_file, std::ios::app);
    if (!log_file_.is_open()) {
        return false;
    }

    min_level_ = min_level;
    initialized_ = true;

    return true;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::debug(const std::string& component, const std::string& message) {
    log(LogLevel::DEBUG, component, message);
}

void Logger::info(const std::string& component, const std::string& message) {
    log(LogLevel::INFO, component, message);
}

void Logger::warn(const std::string& component, const std::string& message) {
    log(LogLevel::WARN, component, message);
}

void Logger::error(const std::string& component, const std::string& message) {
    log(LogLevel::ERROR, component, message);
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_.is_open()) {
        log_file_.close();
    }
    initialized_ = false;
}

void Logger::log(LogLevel level, const std::string& component, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_ || level < min_level_) {
        return;
    }

    std::string timestamp = format_timestamp();
    std::string level_str = log_level_to_string(level);

    std::ostringstream oss;
    oss << timestamp << " [" << level_str << "] " << component << ": " << message;

    std::string formatted = oss.str();

    log_file_ << formatted << "\n";
    log_file_.flush();
}

std::string Logger::format_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

}
