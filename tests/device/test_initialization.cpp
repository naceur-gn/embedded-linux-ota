#include <gtest/gtest.h>
#include "device/device_config.h"
#include "device/device_state.h"
#include "logging/logger.h"
#include <fstream>
#include <cstdio>

static const std::string TEST_INIT_DIR = "/tmp/ota_init_test";

class InitializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        system(("rm -rf " + TEST_INIT_DIR).c_str());
        system(("mkdir -p " + TEST_INIT_DIR).c_str());
        system(("mkdir -p " + TEST_INIT_DIR + "/logs").c_str());
    }

    void TearDown() override {
        ota::Logger::instance().shutdown();
        system(("rm -rf " + TEST_INIT_DIR).c_str());
    }

    void write_valid_config() {
        std::ofstream file(TEST_INIT_DIR + "/device.conf");
        file << "device_id=device-001\n"
             << "hardware_version=revA\n"
             << "software_version=1.0.0\n"
             << "active_slot=A\n";
    }

    void write_invalid_config() {
        std::ofstream file(TEST_INIT_DIR + "/device.conf");
        file << "device_id=\n"
             << "hardware_version=revA\n"
             << "software_version=1.0.0\n"
             << "active_slot=A\n";
    }
};

TEST_F(InitializationTest, ValidConfigInitializationSucceeds) {
    write_valid_config();

    auto& logger = ota::Logger::instance();
    EXPECT_TRUE(logger.initialize(TEST_INIT_DIR + "/logs", ota::LogLevel::INFO));

    auto config = ota::load_config(TEST_INIT_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(ota::validate_config(*config));

    std::string state_dir = TEST_INIT_DIR + "/state";
    EXPECT_TRUE(ota::initialize_state(state_dir, config->software_version, config->active_slot));

    auto state = ota::load_state(state_dir);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->current_version, "1.0.0");
    EXPECT_EQ(state->active_slot, "A");
    EXPECT_EQ(state->update_state, ota::UpdateState::IDLE);
}

TEST_F(InitializationTest, InvalidConfigInitializationFails) {
    write_invalid_config();

    auto& logger = ota::Logger::instance();
    EXPECT_TRUE(logger.initialize(TEST_INIT_DIR + "/logs", ota::LogLevel::INFO));

    auto config = ota::load_config(TEST_INIT_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));

    std::string error = ota::config_error_message(*config);
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("device_id"), std::string::npos);
}

TEST_F(InitializationTest, MissingConfigFileFails) {
    auto config = ota::load_config("/nonexistent/path/device.conf");
    EXPECT_FALSE(config.has_value());
}

TEST_F(InitializationTest, StateInitializationCreatesDirectory) {
    std::string state_dir = TEST_INIT_DIR + "/nested/state";

    bool result = ota::initialize_state(state_dir, "1.0.0", "A");
    EXPECT_TRUE(result);

    auto state = ota::load_state(state_dir);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->current_version, "1.0.0");
}

TEST_F(InitializationTest, LoggerInitializationFailureHandled) {
    auto& logger = ota::Logger::instance();
    bool result = logger.initialize("/nonexistent/path/that/cannot/be/created", ota::LogLevel::INFO);
    EXPECT_FALSE(result);
}

TEST_F(InitializationTest, DeviceInfoReflectsConfig) {
    write_valid_config();

    auto config = ota::load_config(TEST_INIT_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());

    std::string state_dir = TEST_INIT_DIR + "/state";
    ota::initialize_state(state_dir, config->software_version, config->active_slot);

    auto state = ota::load_state(state_dir);
    ASSERT_TRUE(state.has_value());

    EXPECT_EQ(config->device_id, "device-001");
    EXPECT_EQ(config->hardware_version, "revA");
    EXPECT_EQ(state->current_version, config->software_version);
    EXPECT_EQ(state->active_slot, config->active_slot);
}
