#include <gtest/gtest.h>
#include "device/device_state.h"
#include <fstream>
#include <cstdio>

static const std::string TEST_STATE_DIR = "/tmp/ota_test_state";

class DeviceStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        system(("mkdir -p " + TEST_STATE_DIR).c_str());
    }

    void TearDown() override {
        system(("rm -rf " + TEST_STATE_DIR).c_str());
    }
};

TEST_F(DeviceStateTest, InitializeStateCreatesStateFile) {
    bool result = ota::initialize_state(TEST_STATE_DIR, "1.0.0", "A");
    ASSERT_TRUE(result);

    auto state = ota::load_state(TEST_STATE_DIR);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->current_version, "1.0.0");
    EXPECT_EQ(state->active_slot, "A");
    EXPECT_EQ(state->update_state, ota::UpdateState::IDLE);
    EXPECT_EQ(state->boot_attempts, 0);
}

TEST_F(DeviceStateTest, SaveAndLoadStatePreservesAllFields) {
    ota::initialize_state(TEST_STATE_DIR, "1.0.0", "A");

    auto state = ota::load_state(TEST_STATE_DIR);
    ASSERT_TRUE(state.has_value());

    state->pending_slot = "B";
    state->pending_version = "1.1.0";
    state->update_state = ota::UpdateState::DOWNLOADING;
    state->boot_attempts = 3;
    state->rollback_reason = "health_check_failed";

    bool saved = ota::save_state(*state, TEST_STATE_DIR);
    ASSERT_TRUE(saved);

    auto loaded = ota::load_state(TEST_STATE_DIR);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->pending_slot, "B");
    EXPECT_EQ(loaded->pending_version, "1.1.0");
    EXPECT_EQ(loaded->update_state, ota::UpdateState::DOWNLOADING);
    EXPECT_EQ(loaded->boot_attempts, 3);
    EXPECT_EQ(loaded->rollback_reason, "health_check_failed");
}

TEST(DeviceStateEdgeCases, LoadStateFromNonexistentDirectoryReturnsNullopt) {
    auto state = ota::load_state("/nonexistent/path");
    EXPECT_FALSE(state.has_value());
}

TEST(DeviceStateStringConversion, UpdateStateToStringCoversAllStates) {
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::IDLE), "IDLE");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::CHECKING), "CHECKING");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::DOWNLOADING), "DOWNLOADING");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::VERIFYING), "VERIFYING");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::INSTALLING), "INSTALLING");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::PENDING_REBOOT), "PENDING_REBOOT");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::REBOOTING), "REBOOTING");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::HEALTH_CHECK), "HEALTH_CHECK");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::SUCCESS), "SUCCESS");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::CONFIRMED), "CONFIRMED");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::FAILURE), "FAILURE");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::ROLLBACK), "ROLLBACK");
    EXPECT_EQ(ota::update_state_to_string(ota::UpdateState::RECOVERY), "RECOVERY");
}

TEST_F(DeviceStateTest, InitializeStateCreatesDirectoryIfNeeded) {
    std::string nested_dir = TEST_STATE_DIR + "/nested/deep";

    bool result = ota::initialize_state(nested_dir, "2.0.0", "B");
    ASSERT_TRUE(result);

    auto state = ota::load_state(nested_dir);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->current_version, "2.0.0");
    EXPECT_EQ(state->active_slot, "B");
}
