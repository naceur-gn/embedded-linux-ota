#include <gtest/gtest.h>
#include "slot/slot_manager.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace ota;

class SlotManagerTest : public ::testing::Test {
protected:
    std::string test_dir_;
    SlotConfig config_;
    SlotManager sm_;

    void SetUp() override {
        test_dir_ = "/tmp/ota_slot_test_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);
        config_ = SlotManager::get_test_config(test_dir_);
        sm_.set_config(config_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
};

TEST_F(SlotManagerTest, InitializeSlots) {
    EXPECT_TRUE(sm_.initialize_slots());

    EXPECT_EQ(sm_.get_active_slot(), SlotId::SLOT_A);
    EXPECT_EQ(sm_.get_inactive_slot(), SlotId::SLOT_B);
}

TEST_F(SlotManagerTest, GetActiveSlot) {
    sm_.initialize_slots();
    EXPECT_EQ(sm_.get_active_slot(), SlotId::SLOT_A);
}

TEST_F(SlotManagerTest, GetInactiveSlot) {
    sm_.initialize_slots();
    EXPECT_EQ(sm_.get_inactive_slot(), SlotId::SLOT_B);
}

TEST_F(SlotManagerTest, ReverseConfiguration) {
    config_.default_active_slot = SlotId::SLOT_B;
    sm_.set_config(config_);
    sm_.initialize_slots();

    EXPECT_EQ(sm_.get_active_slot(), SlotId::SLOT_B);
    EXPECT_EQ(sm_.get_inactive_slot(), SlotId::SLOT_A);
}

TEST_F(SlotManagerTest, GetSlotInfo) {
    sm_.initialize_slots();

    auto slot_a = sm_.get_slot_info(SlotId::SLOT_A);
    EXPECT_EQ(slot_a.slot_id, SlotId::SLOT_A);
    EXPECT_EQ(slot_a.state, SlotState::ACTIVE);
    EXPECT_EQ(slot_a.version, "1.0.0");

    auto slot_b = sm_.get_slot_info(SlotId::SLOT_B);
    EXPECT_EQ(slot_b.slot_id, SlotId::SLOT_B);
    EXPECT_EQ(slot_b.state, SlotState::INACTIVE);
}

TEST_F(SlotManagerTest, SetSlotState) {
    sm_.initialize_slots();
    EXPECT_TRUE(sm_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED));

    auto slot_b = sm_.get_slot_info(SlotId::SLOT_B);
    EXPECT_EQ(slot_b.state, SlotState::PREPARED);
}

TEST_F(SlotManagerTest, SetSlotVersion) {
    sm_.initialize_slots();
    EXPECT_TRUE(sm_.set_slot_version(SlotId::SLOT_B, "1.1.0"));

    EXPECT_EQ(sm_.get_slot_version(SlotId::SLOT_B), "1.1.0");
}

TEST_F(SlotManagerTest, SetSlotSha256) {
    sm_.initialize_slots();
    EXPECT_TRUE(sm_.set_slot_sha256(SlotId::SLOT_B, "abc123"));

    auto slot_b = sm_.get_slot_info(SlotId::SLOT_B);
    EXPECT_EQ(slot_b.sha256, "abc123");
}

TEST_F(SlotManagerTest, IsSlotValid) {
    sm_.initialize_slots();

    EXPECT_TRUE(sm_.is_slot_valid(SlotId::SLOT_A));
    EXPECT_FALSE(sm_.is_slot_valid(SlotId::SLOT_B));
}

TEST_F(SlotManagerTest, IsSlotActive) {
    sm_.initialize_slots();

    EXPECT_TRUE(sm_.is_slot_active(SlotId::SLOT_A));
    EXPECT_FALSE(sm_.is_slot_active(SlotId::SLOT_B));
}

TEST_F(SlotManagerTest, IsSlotEmpty) {
    sm_.initialize_slots();

    EXPECT_FALSE(sm_.is_slot_empty(SlotId::SLOT_A));
    EXPECT_FALSE(sm_.is_slot_empty(SlotId::SLOT_B));
}

TEST_F(SlotManagerTest, ValidateSlot) {
    sm_.initialize_slots();

    EXPECT_TRUE(sm_.validate_slot(SlotId::SLOT_A));
    EXPECT_TRUE(sm_.validate_slot(SlotId::SLOT_B));
}

TEST_F(SlotManagerTest, ValidateSlotIntegrity) {
    sm_.initialize_slots();
    sm_.set_slot_sha256(SlotId::SLOT_A, "abc123");

    EXPECT_TRUE(sm_.validate_slot_integrity(SlotId::SLOT_A, "abc123"));
    EXPECT_FALSE(sm_.validate_slot_integrity(SlotId::SLOT_A, "def456"));
}

TEST_F(SlotManagerTest, PrepareInactiveSlot) {
    sm_.initialize_slots();

    EXPECT_TRUE(sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123"));

    auto slot_b = sm_.get_slot_info(SlotId::SLOT_B);
    EXPECT_EQ(slot_b.version, "1.1.0");
    EXPECT_EQ(slot_b.hardware_version, "hw-v1");
    EXPECT_EQ(slot_b.sha256, "abc123");
    EXPECT_EQ(slot_b.state, SlotState::PREPARED);
}

TEST_F(SlotManagerTest, CannotPrepareActiveSlot) {
    sm_.initialize_slots();

    SlotId active = sm_.get_active_slot();
    SlotInfo active_info = sm_.get_slot_info(active);

    if (active == SlotId::SLOT_A) {
        EXPECT_TRUE(sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123"));
    } else {
        EXPECT_TRUE(sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123"));
    }
}

TEST_F(SlotManagerTest, SwitchActiveSlot) {
    sm_.initialize_slots();
    sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123");

    EXPECT_TRUE(sm_.switch_active_slot());

    EXPECT_EQ(sm_.get_active_slot(), SlotId::SLOT_B);
    EXPECT_EQ(sm_.get_inactive_slot(), SlotId::SLOT_A);

    auto slot_a = sm_.get_slot_info(SlotId::SLOT_A);
    EXPECT_EQ(slot_a.state, SlotState::INACTIVE);

    auto slot_b = sm_.get_slot_info(SlotId::SLOT_B);
    EXPECT_EQ(slot_b.state, SlotState::ACTIVE);
}

TEST_F(SlotManagerTest, CannotSwitchToInvalidSlot) {
    sm_.initialize_slots();

    EXPECT_FALSE(sm_.switch_active_slot());
}

TEST_F(SlotManagerTest, SlotIdToString) {
    EXPECT_EQ(slot_id_to_string(SlotId::SLOT_A), "A");
    EXPECT_EQ(slot_id_to_string(SlotId::SLOT_B), "B");
}

TEST_F(SlotManagerTest, StringToSlotId) {
    EXPECT_EQ(string_to_slot_id("A"), SlotId::SLOT_A);
    EXPECT_EQ(string_to_slot_id("B"), SlotId::SLOT_B);
    EXPECT_EQ(string_to_slot_id("a"), SlotId::SLOT_A);
    EXPECT_EQ(string_to_slot_id("b"), SlotId::SLOT_B);
    EXPECT_EQ(string_to_slot_id("C"), SlotId::SLOT_A);
}

TEST_F(SlotManagerTest, SlotStateToString) {
    EXPECT_EQ(slot_state_to_string(SlotState::EMPTY), "EMPTY");
    EXPECT_EQ(slot_state_to_string(SlotState::ACTIVE), "ACTIVE");
    EXPECT_EQ(slot_state_to_string(SlotState::INACTIVE), "INACTIVE");
    EXPECT_EQ(slot_state_to_string(SlotState::PREPARED), "PREPARED");
    EXPECT_EQ(slot_state_to_string(SlotState::BOOTABLE), "BOOTABLE");
    EXPECT_EQ(slot_state_to_string(SlotState::INVALID), "INVALID");
}

TEST_F(SlotManagerTest, StringToSlotState) {
    EXPECT_EQ(string_to_slot_state("EMPTY"), SlotState::EMPTY);
    EXPECT_EQ(string_to_slot_state("ACTIVE"), SlotState::ACTIVE);
    EXPECT_EQ(string_to_slot_state("INACTIVE"), SlotState::INACTIVE);
    EXPECT_EQ(string_to_slot_state("PREPARED"), SlotState::PREPARED);
    EXPECT_EQ(string_to_slot_state("BOOTABLE"), SlotState::BOOTABLE);
    EXPECT_EQ(string_to_slot_state("INVALID"), SlotState::INVALID);
    EXPECT_EQ(string_to_slot_state("UNKNOWN"), SlotState::EMPTY);
}

TEST_F(SlotManagerTest, SlotInfoIsValid) {
    SlotInfo info;
    info.slot_id = SlotId::SLOT_A;
    info.version = "1.0.0";
    info.state = SlotState::ACTIVE;
    EXPECT_TRUE(info.is_valid());

    info.state = SlotState::EMPTY;
    EXPECT_FALSE(info.is_valid());

    info.state = SlotState::INVALID;
    EXPECT_FALSE(info.is_valid());

    info.version = "";
    info.state = SlotState::ACTIVE;
    EXPECT_FALSE(info.is_valid());
}
