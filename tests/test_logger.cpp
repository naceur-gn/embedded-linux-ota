#include <gtest/gtest.h>
#include "logging/logger.h"
#include <fstream>
#include <cstdio>

static const std::string TEST_LOG_DIR = "/tmp/ota_test_log";

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        system(("mkdir -p " + TEST_LOG_DIR).c_str());
    }

    void TearDown() override {
        ota::Logger::instance().shutdown();
        system(("rm -rf " + TEST_LOG_DIR).c_str());
    }
};

TEST_F(LoggerTest, LoggerInitializesSuccessfully) {
    auto& logger = ota::Logger::instance();
    bool result = logger.initialize(TEST_LOG_DIR, ota::LogLevel::INFO);
    EXPECT_TRUE(result);
}

TEST_F(LoggerTest, LoggerWritesMessagesToFile) {
    auto& logger = ota::Logger::instance();
    logger.initialize(TEST_LOG_DIR, ota::LogLevel::DEBUG);

    logger.info("test", "Test message");
    logger.shutdown();

    std::ifstream log_file(TEST_LOG_DIR + "/ota.log");
    ASSERT_TRUE(log_file.is_open());

    std::string content;
    std::getline(log_file, content);
    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("[INFO]"), std::string::npos);
    EXPECT_NE(content.find("test:"), std::string::npos);
    EXPECT_NE(content.find("Test message"), std::string::npos);
}

TEST_F(LoggerTest, LoggerFiltersByLevel) {
    auto& logger = ota::Logger::instance();
    logger.initialize(TEST_LOG_DIR, ota::LogLevel::WARN);

    logger.debug("test", "debug msg");
    logger.info("test", "info msg");
    logger.warn("test", "warn msg");
    logger.error("test", "error msg");
    logger.shutdown();

    std::ifstream log_file(TEST_LOG_DIR + "/ota.log");
    std::string line;
    int line_count = 0;
    while (std::getline(log_file, line)) {
        line_count++;
    }

    EXPECT_EQ(line_count, 2);
}

TEST(LogLevelConversion, LogLevelToStringConversion) {
    EXPECT_EQ(ota::log_level_to_string(ota::LogLevel::DEBUG), "DEBUG");
    EXPECT_EQ(ota::log_level_to_string(ota::LogLevel::INFO), "INFO");
    EXPECT_EQ(ota::log_level_to_string(ota::LogLevel::WARN), "WARN");
    EXPECT_EQ(ota::log_level_to_string(ota::LogLevel::ERROR), "ERROR");
}

TEST(LogLevelConversion, StringToLogLevelConversion) {
    EXPECT_EQ(ota::string_to_log_level("DEBUG"), ota::LogLevel::DEBUG);
    EXPECT_EQ(ota::string_to_log_level("INFO"), ota::LogLevel::INFO);
    EXPECT_EQ(ota::string_to_log_level("WARN"), ota::LogLevel::WARN);
    EXPECT_EQ(ota::string_to_log_level("ERROR"), ota::LogLevel::ERROR);
    EXPECT_EQ(ota::string_to_log_level("invalid"), ota::LogLevel::INFO);
}

TEST_F(LoggerTest, LoggerSetLevelChangesFiltering) {
    auto& logger = ota::Logger::instance();
    logger.initialize(TEST_LOG_DIR, ota::LogLevel::DEBUG);

    logger.set_level(ota::LogLevel::ERROR);
    logger.info("test", "should not appear");
    logger.error("test", "should appear");
    logger.shutdown();

    std::ifstream log_file(TEST_LOG_DIR + "/ota.log");
    std::string line;
    int line_count = 0;
    while (std::getline(log_file, line)) {
        line_count++;
    }

    EXPECT_EQ(line_count, 1);
}
