#include <gtest/gtest.h>
#include "boot/simulated_boot_control.h"
#include "slot/slot_manager.h"
#include <filesystem>
#include <fstream>

namespace ota {
namespace {

class BootControlTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/ota_boot_test_" + std::to_string(getpid());
        std::filesystem::create_directories(test_dir_);
        std::filesystem::create_directories(test_dir_ + "/boot");
        std::filesystem::create_directories(test_dir_ + "/slots");

        SlotConfig slot_config;
        slot_config.slots_dir = test_dir_ + "/slots";
        slot_config.state_file = test_dir_ + "/slots/global.json";
        slot_config.default_active_slot = SlotId::SLOT_A;
        slot_config.default_version = "1.0.0";
        slot_config.default_hardware_version = "hw-v1";

        slot_manager_.set_config(slot_config);
        slot_manager_.initialize_slots();

        SimulatedBootConfig boot_config;
        boot_config.boot_state_dir = test_dir_ + "/boot";
        boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

        boot_control_.set_config(boot_config);
        boot_control_.initialize();
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    SlotManager slot_manager_;
    SimulatedBootControl boot_control_;
};

TEST_F(BootControlTest, InitialState) {
    BootState state = boot_control_.get_boot_state();

    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
    EXPECT_EQ(state.next_slot, SlotId::SLOT_A);
    EXPECT_EQ(state.boot_attempts.at(SlotId::SLOT_A), 0);
    EXPECT_EQ(state.boot_attempts.at(SlotId::SLOT_B), 0);
}

TEST_F(BootControlTest, SetNextBoot) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);

    EXPECT_TRUE(boot_control_.set_next_boot_slot(SlotId::SLOT_B));

    BootState state = boot_control_.get_boot_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
    EXPECT_EQ(state.next_slot, SlotId::SLOT_B);
}

TEST_F(BootControlTest, GetNextBoot) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    SlotId next = boot_control_.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_B);
}

TEST_F(BootControlTest, ClearNextBoot) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    EXPECT_TRUE(boot_control_.clear_next_boot_slot());

    SlotId next = boot_control_.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_A);
}

TEST_F(BootControlTest, InvalidSlotRejected) {
    EXPECT_FALSE(boot_control_.set_next_boot_slot(static_cast<SlotId>(99)));
}

TEST_F(BootControlTest, EmptySlotRejected) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::EMPTY);
    EXPECT_FALSE(boot_control_.validate_boot_target(SlotId::SLOT_B, slot_manager_));
}

TEST_F(BootControlTest, InvalidSlotRejectedByValidation) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::INVALID);

    EXPECT_FALSE(boot_control_.validate_boot_target(SlotId::SLOT_B, slot_manager_));
}

TEST_F(BootControlTest, CurrentSlotRejected) {
    EXPECT_FALSE(boot_control_.validate_boot_target(SlotId::SLOT_A, slot_manager_));
}

TEST_F(BootControlTest, BootAttemptIncrement) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);
    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    boot_control_.simulate_boot();

    EXPECT_EQ(boot_control_.get_boot_attempt_count(SlotId::SLOT_B), 1);
}

TEST_F(BootControlTest, MultipleBootAttempts) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    boot_control_.simulate_boot();

    boot_control_.set_next_boot_slot(SlotId::SLOT_A);
    boot_control_.simulate_boot();

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    boot_control_.simulate_boot();

    EXPECT_EQ(boot_control_.get_boot_attempt_count(SlotId::SLOT_B), 2);
    EXPECT_EQ(boot_control_.get_boot_attempt_count(SlotId::SLOT_A), 1);
}

TEST_F(BootControlTest, ResetBootAttempts) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);
    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    boot_control_.simulate_boot();

    EXPECT_EQ(boot_control_.get_boot_attempt_count(SlotId::SLOT_B), 1);

    EXPECT_TRUE(boot_control_.reset_boot_attempt_count(SlotId::SLOT_B));

    EXPECT_EQ(boot_control_.get_boot_attempt_count(SlotId::SLOT_B), 0);
}

TEST_F(BootControlTest, SimulatedBoot) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);
    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    EXPECT_TRUE(boot_control_.simulate_boot());

    BootState state = boot_control_.get_boot_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_B);
    EXPECT_EQ(state.next_slot, SlotId::SLOT_B);
    EXPECT_EQ(state.boot_attempts.at(SlotId::SLOT_B), 1);
}

TEST_F(BootControlTest, SimulatedBootClearsNextSlot) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);
    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    boot_control_.simulate_boot();

    SlotId next = boot_control_.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_B);
}

TEST_F(BootControlTest, ActiveSlotProtection) {
    EXPECT_FALSE(boot_control_.prepare_next_boot(SlotId::SLOT_A, slot_manager_));
}

TEST_F(BootControlTest, PrepareNextBootSuccess) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);

    EXPECT_TRUE(boot_control_.prepare_next_boot(SlotId::SLOT_B, slot_manager_));

    SlotId next = boot_control_.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_B);
}

TEST_F(BootControlTest, PrepareInvalidSlotRejected) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::INVALID);

    EXPECT_FALSE(boot_control_.prepare_next_boot(SlotId::SLOT_B, slot_manager_));
}

TEST_F(BootControlTest, NoNextBootWithoutSet) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);

    SlotId next = boot_control_.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_A);
}

TEST_F(BootControlTest, SetNextBootPersists) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);
    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    SimulatedBootControl new_boot_control;
    new_boot_control.set_config(boot_control_.get_test_config(test_dir_));
    new_boot_control.initialize();

    SlotId next = new_boot_control.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_B);
}

TEST_F(BootControlTest, CurrentSlotPersists) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);
    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    boot_control_.simulate_boot();

    SimulatedBootControl new_boot_control;
    new_boot_control.set_config(boot_control_.get_test_config(test_dir_));
    new_boot_control.initialize();

    SlotId current = new_boot_control.get_current_boot_slot();
    EXPECT_EQ(current, SlotId::SLOT_B);
}

TEST_F(BootControlTest, BootAttemptsPersist) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::PREPARED);
    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    boot_control_.simulate_boot();

    SimulatedBootControl new_boot_control;
    new_boot_control.set_config(boot_control_.get_test_config(test_dir_));
    new_boot_control.initialize();

    EXPECT_EQ(new_boot_control.get_boot_attempt_count(SlotId::SLOT_B), 1);
}

}
}