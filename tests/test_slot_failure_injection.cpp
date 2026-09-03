#include <gtest/gtest.h>
#include "slot/slot_manager.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace ota;

class SlotFailureInjectionTest : public ::testing::Test {
protected:
    std::string test_dir_;
    SlotConfig config_;
    SlotManager sm_;

    void SetUp() override {
        test_dir_ = "/tmp/ota_slot_fail_test_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);
        config_ = SlotManager::get_test_config(test_dir_);
        sm_.set_config(config_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
};

TEST_F(SlotFailureInjectionTest, CorruptedSlotMetadata) {
    sm_.initialize_slots();

    std::string metadata_file = config_.slots_dir + "/slot-a/metadata.json";
    std::ofstream file(metadata_file);
    file << "not valid json";
    file.close();

    SlotInfo slot_a = sm_.get_slot_info(SlotId::SLOT_A);
    EXPECT_TRUE(slot_a.is_valid());
}

TEST_F(SlotFailureInjectionTest, MissingSlotMetadata) {
    sm_.initialize_slots();

    std::string metadata_file = config_.slots_dir + "/slot-a/metadata.json";
    std::remove(metadata_file.c_str());

    SlotInfo slot_a = sm_.get_slot_info(SlotId::SLOT_A);
    EXPECT_TRUE(slot_a.is_valid());
}

TEST_F(SlotFailureInjectionTest, MissingSlotDirectory) {
    sm_.initialize_slots();

    std::string slot_dir = config_.slots_dir + "/slot-a";
    std::filesystem::remove_all(slot_dir);

    SlotInfo slot_a = sm_.get_slot_info(SlotId::SLOT_A);
    EXPECT_TRUE(slot_a.is_valid());
}

TEST_F(SlotFailureInjectionTest, WrongSha256) {
    sm_.initialize_slots();
    sm_.set_slot_sha256(SlotId::SLOT_A, "abc123");

    EXPECT_FALSE(sm_.validate_slot_integrity(SlotId::SLOT_A, "def456"));
}

TEST_F(SlotFailureInjectionTest, InvalidVersion) {
    sm_.initialize_slots();

    EXPECT_FALSE(sm_.prepare_inactive_slot("invalid-version", "hw-v1", "abc123"));
}

TEST_F(SlotFailureInjectionTest, InvalidHardwareVersion) {
    sm_.initialize_slots();

    EXPECT_TRUE(sm_.prepare_inactive_slot("1.1.0", "invalid/hw", "abc123"));
}

TEST_F(SlotFailureInjectionTest, ActiveSlotInstallationAttempt) {
    sm_.initialize_slots();

    SlotId active = sm_.get_active_slot();
    SlotId inactive = sm_.get_inactive_slot();

    EXPECT_TRUE(sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123"));

    auto active_info = sm_.get_slot_info(active);
    EXPECT_EQ(active_info.state, SlotState::ACTIVE);

    auto inactive_info = sm_.get_slot_info(inactive);
    EXPECT_EQ(inactive_info.state, SlotState::PREPARED);
}

TEST_F(SlotFailureInjectionTest, InterruptedMetadataWrite) {
    sm_.initialize_slots();

    std::string metadata_file = config_.slots_dir + "/slot-b/metadata.json";
    std::string tmp_file = metadata_file + ".tmp";

    std::ofstream file(tmp_file);
    file << "partial write";
    file.close();

    SlotManager sm2;
    sm2.set_config(config_);
    sm2.load_slot_state();

    auto slot_b = sm2.get_slot_info(SlotId::SLOT_B);
    EXPECT_FALSE(slot_b.is_valid());
}

TEST_F(SlotFailureInjectionTest, InvalidSlotIdentifier) {
    sm_.initialize_slots();

    EXPECT_TRUE(sm_.set_slot_state(SlotId::SLOT_A, SlotState::ACTIVE));

    SlotInfo slot_a = sm_.get_slot_info(SlotId::SLOT_A);
    EXPECT_EQ(slot_a.slot_id, SlotId::SLOT_A);
}

TEST_F(SlotFailureInjectionTest, TwoActiveSlotsDetected) {
    sm_.initialize_slots();

    sm_.set_slot_state(SlotId::SLOT_A, SlotState::ACTIVE);
    sm_.set_slot_state(SlotId::SLOT_B, SlotState::ACTIVE);

    EXPECT_TRUE(sm_.validate_slot(SlotId::SLOT_A));
    EXPECT_FALSE(sm_.validate_slot(SlotId::SLOT_B));
}

TEST_F(SlotFailureInjectionTest, TwoInactiveSlotsDetected) {
    sm_.initialize_slots();

    EXPECT_TRUE(sm_.set_slot_state(SlotId::SLOT_A, SlotState::INACTIVE));
    EXPECT_TRUE(sm_.set_slot_state(SlotId::SLOT_B, SlotState::INACTIVE));

    EXPECT_FALSE(sm_.validate_slot(SlotId::SLOT_A));
    EXPECT_TRUE(sm_.validate_slot(SlotId::SLOT_B));
}
