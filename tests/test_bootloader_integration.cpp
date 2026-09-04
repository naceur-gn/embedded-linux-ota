#include <gtest/gtest.h>
#include "boot/simulated_boot_control.h"
#include "boot/bootloader/simulated_bootloader.hpp"
#include "slot/slot_manager.h"
#include <filesystem>

namespace ota {
namespace {

class BootloaderIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/ota_bootloader_integration_test_" + std::to_string(getpid());
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

TEST_F(BootloaderIntegrationTest, FullUpdateWorkflowWithBootloader) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    EXPECT_TRUE(boot_control_.prepare_next_boot(slot_manager_.get_inactive_slot(), slot_manager_));

    SlotId next = boot_control_.get_next_boot_slot();
    EXPECT_EQ(next, slot_manager_.get_inactive_slot());
}

TEST_F(BootloaderIntegrationTest, SimulateBootUpdatesBootloader) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(slot_manager_.get_inactive_slot());
    boot_control_.simulate_boot();

    SlotId current = boot_control_.get_current_boot_slot();
    EXPECT_EQ(current, slot_manager_.get_inactive_slot());
}

TEST_F(BootloaderIntegrationTest, MultipleBootCyclesWithBootloader) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    boot_control_.simulate_boot();

    EXPECT_EQ(boot_control_.get_current_boot_slot(), SlotId::SLOT_B);
    EXPECT_EQ(boot_control_.get_boot_attempt_count(SlotId::SLOT_B), 1);

    boot_control_.set_next_boot_slot(SlotId::SLOT_A);
    boot_control_.simulate_boot();

    EXPECT_EQ(boot_control_.get_current_boot_slot(), SlotId::SLOT_A);
    EXPECT_EQ(boot_control_.get_boot_attempt_count(SlotId::SLOT_A), 1);
    EXPECT_EQ(boot_control_.get_boot_attempt_count(SlotId::SLOT_B), 1);
}

TEST_F(BootloaderIntegrationTest, InvalidSlotRejectedBySlotManager) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::EMPTY);

    EXPECT_FALSE(boot_control_.validate_boot_target(SlotId::SLOT_B, slot_manager_));
    EXPECT_FALSE(boot_control_.prepare_next_boot(SlotId::SLOT_B, slot_manager_));
}

TEST_F(BootloaderIntegrationTest, BootControlAndBootloaderConsistency) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    BootState boot_state = boot_control_.get_boot_state();
    EXPECT_EQ(boot_state.current_slot, SlotId::SLOT_A);
    EXPECT_EQ(boot_state.next_slot, SlotId::SLOT_B);
}

TEST_F(BootloaderIntegrationTest, BootControlPersistenceWithBootloader) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    SimulatedBootControl new_boot_control;
    new_boot_control.set_config(boot_control_.get_test_config(test_dir_));
    new_boot_control.initialize();

    SlotId next = new_boot_control.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_B);
}

TEST_F(BootloaderIntegrationTest, SimulatedBootPersistenceWithBootloader) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    boot_control_.simulate_boot();

    SimulatedBootControl new_boot_control;
    new_boot_control.set_config(boot_control_.get_test_config(test_dir_));
    new_boot_control.initialize();

    EXPECT_EQ(new_boot_control.get_current_boot_slot(), SlotId::SLOT_B);
    EXPECT_EQ(new_boot_control.get_boot_attempt_count(SlotId::SLOT_B), 1);
}

TEST_F(BootloaderIntegrationTest, SetBootloaderOnBootControl) {
    auto bootloader = std::make_shared<SimulatedBootloader>();
    bootloader->set_state_dir(test_dir_ + "/boot/bootloader", test_dir_ + "/boot/bootloader/bootloader_state.json");

    boot_control_.set_bootloader(bootloader);

    EXPECT_EQ(boot_control_.get_bootloader(), bootloader);
}

TEST_F(BootloaderIntegrationTest, BootControlUsesBootloader) {
    auto bootloader = std::make_shared<SimulatedBootloader>();
    bootloader->set_state_dir(test_dir_ + "/boot/bootloader", test_dir_ + "/boot/bootloader/bootloader_state.json");

    boot_control_.set_bootloader(bootloader);
    boot_control_.initialize();

    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    BootloaderState bl_state = bootloader->get_state();
    EXPECT_TRUE(bl_state.next_boot_slot_set);
    EXPECT_EQ(bl_state.next_boot_slot, SlotId::SLOT_B);
}

TEST_F(BootloaderIntegrationTest, FullChainSimulation) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    EXPECT_TRUE(boot_control_.prepare_next_boot(slot_manager_.get_inactive_slot(), slot_manager_));

    EXPECT_TRUE(boot_control_.simulate_boot());

    BootState state = boot_control_.get_boot_state();
    EXPECT_EQ(state.current_slot, slot_manager_.get_inactive_slot());
    EXPECT_EQ(state.boot_attempts.at(slot_manager_.get_inactive_slot()), 1);
}

}
}
