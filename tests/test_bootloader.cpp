#include <gtest/gtest.h>
#include "boot/bootloader/bootloader.hpp"
#include "boot/bootloader/simulated_bootloader.hpp"
#include <filesystem>
#include <fstream>

namespace ota {
namespace {

class BootloaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/ota_bootloader_test_" + std::to_string(getpid());
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

TEST_F(BootloaderTest, InitialState) {
    BootloaderState state = bootloader_.get_state();

    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
    EXPECT_EQ(state.next_boot_slot, SlotId::SLOT_A);
    EXPECT_FALSE(state.next_boot_slot_set);
    EXPECT_EQ(state.boot_attempts_a, 0);
    EXPECT_EQ(state.boot_attempts_b, 0);
}

TEST_F(BootloaderTest, DefaultCurrentSlot) {
    EXPECT_EQ(bootloader_.get_current_slot(), SlotId::SLOT_A);
}

TEST_F(BootloaderTest, NoPendingBootSlot) {
    EXPECT_FALSE(bootloader_.has_pending_boot_slot());
}

TEST_F(BootloaderTest, ZeroBootAttempts) {
    EXPECT_EQ(bootloader_.get_boot_attempts(SlotId::SLOT_A), 0);
    EXPECT_EQ(bootloader_.get_boot_attempts(SlotId::SLOT_B), 0);
}

TEST_F(BootloaderTest, SetNextBootSlotA) {
    EXPECT_TRUE(bootloader_.set_next_boot_slot(SlotId::SLOT_A));

    BootloaderState state = bootloader_.get_state();
    EXPECT_TRUE(state.next_boot_slot_set);
    EXPECT_EQ(state.next_boot_slot, SlotId::SLOT_A);
}

TEST_F(BootloaderTest, SetNextBootSlotB) {
    EXPECT_TRUE(bootloader_.set_next_boot_slot(SlotId::SLOT_B));

    BootloaderState state = bootloader_.get_state();
    EXPECT_TRUE(state.next_boot_slot_set);
    EXPECT_EQ(state.next_boot_slot, SlotId::SLOT_B);
}

TEST_F(BootloaderTest, GetNextBootSlot) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);

    SlotId next = bootloader_.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_B);
}

TEST_F(BootloaderTest, GetNextBootSlotDefault) {
    SlotId next = bootloader_.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_A);
}

TEST_F(BootloaderTest, ClearNextBootSlot) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    EXPECT_TRUE(bootloader_.clear_next_boot_slot());

    SlotId next = bootloader_.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_A);
    EXPECT_FALSE(bootloader_.has_pending_boot_slot());
}

TEST_F(BootloaderTest, InvalidSlotRejected) {
    EXPECT_FALSE(bootloader_.set_next_boot_slot(static_cast<SlotId>(99)));
    EXPECT_EQ(bootloader_.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderTest, EmptySlotRejected) {
    EXPECT_FALSE(bootloader_.set_next_boot_slot(static_cast<SlotId>(-1)));
    EXPECT_EQ(bootloader_.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderTest, CorruptedSlotRejected) {
    EXPECT_FALSE(bootloader_.set_next_boot_slot(static_cast<SlotId>(100)));
    EXPECT_EQ(bootloader_.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderTest, ValidateSlotA) {
    EXPECT_TRUE(bootloader_.validate_slot(SlotId::SLOT_A));
}

TEST_F(BootloaderTest, ValidateSlotB) {
    EXPECT_TRUE(bootloader_.validate_slot(SlotId::SLOT_B));
}

TEST_F(BootloaderTest, ValidateInvalidSlot) {
    EXPECT_FALSE(bootloader_.validate_slot(static_cast<SlotId>(99)));
}

TEST_F(BootloaderTest, SimulateBootCurrentA) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);

    EXPECT_TRUE(bootloader_.mark_boot_started(SlotId::SLOT_B));

    BootloaderState state = bootloader_.get_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_B);
    EXPECT_FALSE(state.next_boot_slot_set);
}

TEST_F(BootloaderTest, BootAttemptIncrement) {
    bootloader_.increment_boot_attempts(SlotId::SLOT_A);
    EXPECT_EQ(bootloader_.get_boot_attempts(SlotId::SLOT_A), 1);

    bootloader_.increment_boot_attempts(SlotId::SLOT_A);
    EXPECT_EQ(bootloader_.get_boot_attempts(SlotId::SLOT_A), 2);
}

TEST_F(BootloaderTest, MultipleBootAttempts) {
    bootloader_.increment_boot_attempts(SlotId::SLOT_B);
    bootloader_.increment_boot_attempts(SlotId::SLOT_B);

    bootloader_.increment_boot_attempts(SlotId::SLOT_A);

    EXPECT_EQ(bootloader_.get_boot_attempts(SlotId::SLOT_B), 2);
    EXPECT_EQ(bootloader_.get_boot_attempts(SlotId::SLOT_A), 1);
}

TEST_F(BootloaderTest, ResetBootAttempts) {
    bootloader_.increment_boot_attempts(SlotId::SLOT_A);
    bootloader_.increment_boot_attempts(SlotId::SLOT_A);

    EXPECT_TRUE(bootloader_.reset_boot_attempts(SlotId::SLOT_A));

    EXPECT_EQ(bootloader_.get_boot_attempts(SlotId::SLOT_A), 0);
}

TEST_F(BootloaderTest, MarkBootStartedIncrementsAttempts) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);

    EXPECT_TRUE(bootloader_.mark_boot_started(SlotId::SLOT_B));

    EXPECT_EQ(bootloader_.get_boot_attempts(SlotId::SLOT_B), 1);
}

TEST_F(BootloaderTest, MarkBootStartedClearsPending) {
    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    EXPECT_TRUE(bootloader_.has_pending_boot_slot());

    bootloader_.mark_boot_started(SlotId::SLOT_B);
    EXPECT_FALSE(bootloader_.has_pending_boot_slot());
}

TEST_F(BootloaderTest, NoErrorInitially) {
    EXPECT_EQ(bootloader_.get_last_error(), BootloaderError::NONE);
}

TEST_F(BootloaderTest, ErrorClearedAfterSuccess) {
    bootloader_.set_next_boot_slot(static_cast<SlotId>(99));
    EXPECT_NE(bootloader_.get_last_error(), BootloaderError::NONE);

    bootloader_.set_next_boot_slot(SlotId::SLOT_B);
    EXPECT_EQ(bootloader_.get_last_error(), BootloaderError::NONE);
}

TEST_F(BootloaderTest, InvalidSlotResetAttempts) {
    EXPECT_FALSE(bootloader_.reset_boot_attempts(static_cast<SlotId>(99)));
    EXPECT_EQ(bootloader_.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderTest, InvalidSlotIncrementAttempts) {
    EXPECT_FALSE(bootloader_.increment_boot_attempts(static_cast<SlotId>(99)));
    EXPECT_EQ(bootloader_.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderTest, InvalidMarkBootStarted) {
    EXPECT_FALSE(bootloader_.mark_boot_started(static_cast<SlotId>(99)));
    EXPECT_EQ(bootloader_.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderTest, DefaultConfig) {
    SimulatedBootloaderConfig config = bootloader_.get_default_config();
    EXPECT_EQ(config.state_dir, "/var/lib/ota/bootloader");
    EXPECT_EQ(config.state_file, "/var/lib/ota/bootloader/bootloader_state.json");
}

TEST_F(BootloaderTest, TestConfig) {
    SimulatedBootloaderConfig config = SimulatedBootloader::get_test_config("/tmp/test");
    EXPECT_EQ(config.state_dir, "/tmp/test/bootloader");
    EXPECT_EQ(config.state_file, "/tmp/test/bootloader/bootloader_state.json");
}

}
}
