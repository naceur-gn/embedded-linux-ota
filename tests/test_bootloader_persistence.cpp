#include <gtest/gtest.h>
#include "boot/bootloader/bootloader.hpp"
#include "boot/bootloader/simulated_bootloader.hpp"
#include <filesystem>
#include <fstream>

namespace ota {
namespace {

class BootloaderPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/ota_bootloader_persist_test_" + std::to_string(getpid());
        std::filesystem::create_directories(test_dir_);
        std::filesystem::create_directories(test_dir_ + "/bootloader");

        bootloader_.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
        bootloader_.initialize();
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    SimulatedBootloader bootloader_;
};

TEST_F(BootloaderPersistenceTest, SetNextBootPersists) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);

    SimulatedBootloader new_bootloader;
    new_bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    new_bootloader.initialize();

    SlotId next = new_bootloader.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_B);
    EXPECT_TRUE(new_bootloader.has_pending_boot_slot());
}

TEST_F(BootloaderPersistenceTest, ClearNextBootPersists) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    bootloader_.clear_next_boot_slot();

    SimulatedBootloader new_bootloader;
    new_bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    new_bootloader.initialize();

    EXPECT_FALSE(new_bootloader.has_pending_boot_slot());
    EXPECT_EQ(new_bootloader.get_next_boot_slot(), SlotId::SLOT_A);
}

TEST_F(BootloaderPersistenceTest, BootAttemptsPersist) {
    bootloader_.increment_boot_attempts(SlotId::SLOT_A);
    bootloader_.increment_boot_attempts(SlotId::SLOT_A);
    bootloader_.increment_boot_attempts(SlotId::SLOT_B);

    SimulatedBootloader new_bootloader;
    new_bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    new_bootloader.initialize();

    EXPECT_EQ(new_bootloader.get_boot_attempts(SlotId::SLOT_A), 2);
    EXPECT_EQ(new_bootloader.get_boot_attempts(SlotId::SLOT_B), 1);
}

TEST_F(BootloaderPersistenceTest, CurrentSlotPersists) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    bootloader_.mark_boot_started(SlotId::SLOT_B);

    SimulatedBootloader new_bootloader;
    new_bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    new_bootloader.initialize();

    EXPECT_EQ(new_bootloader.get_current_slot(), SlotId::SLOT_B);
}

TEST_F(BootloaderPersistenceTest, FullStatePersists) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    bootloader_.mark_boot_started(SlotId::SLOT_B);

    bootloader_.set_next_boot_slot(SlotId::SLOT_A);
    bootloader_.mark_boot_started(SlotId::SLOT_A);

    SimulatedBootloader new_bootloader;
    new_bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    new_bootloader.initialize();

    BootloaderState state = new_bootloader.get_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
    EXPECT_FALSE(state.next_boot_slot_set);
    EXPECT_EQ(state.boot_attempts_a, 1);
    EXPECT_EQ(state.boot_attempts_b, 1);
}

TEST_F(BootloaderPersistenceTest, ResetAttemptsPersists) {
    bootloader_.increment_boot_attempts(SlotId::SLOT_A);
    bootloader_.increment_boot_attempts(SlotId::SLOT_A);
    bootloader_.reset_boot_attempts(SlotId::SLOT_A);

    SimulatedBootloader new_bootloader;
    new_bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    new_bootloader.initialize();

    EXPECT_EQ(new_bootloader.get_boot_attempts(SlotId::SLOT_A), 0);
}

TEST_F(BootloaderPersistenceTest, MultipleBootCyclesPersist) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    bootloader_.mark_boot_started(SlotId::SLOT_B);

    bootloader_.set_next_boot_slot(SlotId::SLOT_A);
    bootloader_.mark_boot_started(SlotId::SLOT_A);

    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    bootloader_.mark_boot_started(SlotId::SLOT_B);

    SimulatedBootloader new_bootloader;
    new_bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    new_bootloader.initialize();

    EXPECT_EQ(new_bootloader.get_current_slot(), SlotId::SLOT_B);
    EXPECT_EQ(new_bootloader.get_boot_attempts(SlotId::SLOT_A), 1);
    EXPECT_EQ(new_bootloader.get_boot_attempts(SlotId::SLOT_B), 2);
}

TEST_F(BootloaderPersistenceTest, NoNextBootPersists) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    bootloader_.clear_next_boot_slot();

    SimulatedBootloader new_bootloader;
    new_bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    new_bootloader.initialize();

    EXPECT_FALSE(new_bootloader.has_pending_boot_slot());
    EXPECT_EQ(new_bootloader.get_next_boot_slot(), SlotId::SLOT_A);
}

}
}
