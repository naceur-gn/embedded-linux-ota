#include <gtest/gtest.h>
#include "device/device_config.h"
#include "device/device_state.h"
#include "logging/logger.h"
#include <fstream>
#include <cstdio>

static const std::string TEST_INTEGRATION_DIR = "/tmp/ota_integration_test";

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        system(("rm -rf " + TEST_INTEGRATION_DIR).c_str());
        system(("mkdir -p " + TEST_INTEGRATION_DIR).c_str());
        system(("mkdir -p " + TEST_INTEGRATION_DIR + "/logs").c_str());
        system(("mkdir -p " + TEST_INTEGRATION_DIR + "/state").c_str());
    }

    void TearDown() override {
        ota::Logger::instance().shutdown();
        system(("rm -rf " + TEST_INTEGRATION_DIR).c_str());
    }
};

TEST_F(IntegrationTest, CompleteFoundationLifecycle) {
    std::string config_path = TEST_INTEGRATION_DIR + "/device.conf";
    std::string state_dir = TEST_INTEGRATION_DIR + "/state";
    std::string log_dir = TEST_INTEGRATION_DIR + "/logs";

    {
        std::ofstream file(config_path);
        file << "device_id=device-001\n"
             << "hardware_version=revA\n"
             << "software_version=1.0.0\n"
             << "active_slot=A\n";
    }

    auto& logger = ota::Logger::instance();
    ASSERT_TRUE(logger.initialize(log_dir, ota::LogLevel::DEBUG));

    logger.info("integration", "Starting integration test");

    auto config = ota::load_config(config_path);
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(ota::validate_config(*config));

    logger.info("integration", "Config loaded: " + config->device_id);

    bool state_initialized = ota::initialize_state(state_dir,
                                                    config->software_version,
                                                    config->active_slot);
    ASSERT_TRUE(state_initialized);

    auto state = ota::load_state(state_dir);
    ASSERT_TRUE(state.has_value());

    EXPECT_EQ(config->device_id, "device-001");
    EXPECT_EQ(config->hardware_version, "revA");
    EXPECT_EQ(state->current_version, "1.0.0");
    EXPECT_EQ(state->active_slot, "A");
    EXPECT_EQ(state->update_state, ota::UpdateState::IDLE);

    logger.info("integration", "State verified");

    state->update_state = ota::UpdateState::CHECKING;
    state->pending_slot = "B";
    state->pending_version = "1.1.0";
    ASSERT_TRUE(ota::save_state(*state, state_dir));

    auto reloaded = ota::load_state(state_dir);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->update_state, ota::UpdateState::CHECKING);
    EXPECT_EQ(reloaded->pending_slot, "B");
    EXPECT_EQ(reloaded->pending_version, "1.1.0");

    logger.info("integration", "State persistence verified");

    state->update_state = ota::UpdateState::CONFIRMED;
    state->current_version = "1.1.0";
    state->active_slot = "B";
    state->pending_slot = "";
    state->pending_version = "";
    ASSERT_TRUE(ota::save_state(*state, state_dir));

    auto final_state = ota::load_state(state_dir);
    ASSERT_TRUE(final_state.has_value());
    EXPECT_EQ(final_state->current_version, "1.1.0");
    EXPECT_EQ(final_state->active_slot, "B");
    EXPECT_EQ(final_state->update_state, ota::UpdateState::CONFIRMED);

    logger.info("integration", "Integration test completed successfully");

    std::ifstream log_file(log_dir + "/ota.log");
    ASSERT_TRUE(log_file.is_open());

    std::string line;
    int log_lines = 0;
    while (std::getline(log_file, line)) {
        log_lines++;
        EXPECT_NE(line.find("[INFO]"), std::string::npos);
        EXPECT_NE(line.find("integration:"), std::string::npos);
    }
    EXPECT_GE(log_lines, 4);
}

TEST_F(IntegrationTest, ConfigurationChangeAffectsDeviceIdentity) {
    std::string config_path = TEST_INTEGRATION_DIR + "/device.conf";
    std::string state_dir = TEST_INTEGRATION_DIR + "/state";

    {
        std::ofstream file(config_path);
        file << "device_id=device-001\n"
             << "hardware_version=revA\n"
             << "software_version=1.0.0\n"
             << "active_slot=A\n";
    }

    auto config1 = ota::load_config(config_path);
    ASSERT_TRUE(config1.has_value());
    EXPECT_EQ(config1->device_id, "device-001");
    EXPECT_EQ(config1->software_version, "1.0.0");

    {
        std::ofstream file(config_path);
        file << "device_id=device-002\n"
             << "hardware_version=revB\n"
             << "software_version=2.0.0\n"
             << "active_slot=B\n";
    }

    auto config2 = ota::load_config(config_path);
    ASSERT_TRUE(config2.has_value());
    EXPECT_EQ(config2->device_id, "device-002");
    EXPECT_EQ(config2->hardware_version, "revB");
    EXPECT_EQ(config2->software_version, "2.0.0");
    EXPECT_EQ(config2->active_slot, "B");
}

TEST_F(IntegrationTest, StateSurvivesSimulatedRestart) {
    std::string state_dir = TEST_INTEGRATION_DIR + "/state";

    ota::initialize_state(state_dir, "1.0.0", "A");

    auto state1 = ota::load_state(state_dir);
    ASSERT_TRUE(state1.has_value());
    state1->update_state = ota::UpdateState::DOWNLOADING;
    state1->boot_attempts = 2;
    state1->rollback_reason = "test_rollback";
    ota::save_state(*state1, state_dir);

    ota::Logger::instance().shutdown();

    auto state2 = ota::load_state(state_dir);
    ASSERT_TRUE(state2.has_value());
    EXPECT_EQ(state2->update_state, ota::UpdateState::DOWNLOADING);
    EXPECT_EQ(state2->boot_attempts, 2);
    EXPECT_EQ(state2->rollback_reason, "test_rollback");
}
