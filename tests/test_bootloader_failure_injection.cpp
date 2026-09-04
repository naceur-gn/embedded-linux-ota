#include <gtest/gtest.h>
#include "boot/bootloader/bootloader.hpp"
#include "boot/bootloader/simulated_bootloader.hpp"
#include <filesystem>
#include <fstream>

namespace ota {
namespace {

class BootloaderFailureInjectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/ota_bootloader_failure_test_" + std::to_string(getpid());
        std::filesystem::create_directories(test_dir_);
        std::filesystem::create_directories(test_dir_ + "/bootloader");
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
};

TEST_F(BootloaderFailureInjectionTest, CorruptedBootloaderState) {
    std::ofstream ofs(test_dir_ + "/bootloader/bootloader_state.json");
    ofs << "{corrupted json content";
    ofs.close();

    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");

    EXPECT_TRUE(bootloader.initialize());

    BootloaderState state = bootloader.get_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
}

TEST_F(BootloaderFailureInjectionTest, MissingBootloaderStateFile) {
    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/nonexistent.json");

    EXPECT_TRUE(bootloader.initialize());

    BootloaderState state = bootloader.get_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
    EXPECT_EQ(state.boot_attempts_a, 0);
    EXPECT_EQ(state.boot_attempts_b, 0);
}

TEST_F(BootloaderFailureInjectionTest, InvalidSlotID) {
    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    bootloader.initialize();

    EXPECT_FALSE(bootloader.set_next_boot_slot(static_cast<SlotId>(99)));
    EXPECT_EQ(bootloader.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderFailureInjectionTest, InvalidNextSlotValue) {
    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    bootloader.initialize();

    EXPECT_FALSE(bootloader.set_next_boot_slot(static_cast<SlotId>(100)));
    EXPECT_EQ(bootloader.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderFailureInjectionTest, CorruptedTargetSlot) {
    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    bootloader.initialize();

    EXPECT_FALSE(bootloader.validate_slot(static_cast<SlotId>(99)));
}

TEST_F(BootloaderFailureInjectionTest, InvalidBootStateAfterCorruption) {
    std::ofstream ofs(test_dir_ + "/bootloader/bootloader_state.json");
    ofs << R"({
        "current_slot": "INVALID",
        "next_boot_slot": "B",
        "next_boot_slot_set": true,
        "boot_attempts_a": -1,
        "boot_attempts_b": "invalid"
    })";
    ofs.close();

    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");

    EXPECT_FALSE(bootloader.initialize());
    EXPECT_EQ(bootloader.get_last_error(), BootloaderError::BOOTLOADER_STATE_CORRUPTED);
}

TEST_F(BootloaderFailureInjectionTest, CorruptedCurrentSlotValue) {
    std::ofstream ofs(test_dir_ + "/bootloader/bootloader_state.json");
    ofs << R"({
        "current_slot": "C",
        "next_boot_slot": "B",
        "next_boot_slot_set": true,
        "boot_attempts_a": 0,
        "boot_attempts_b": 0
    })";
    ofs.close();

    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");

    EXPECT_FALSE(bootloader.initialize());
    EXPECT_EQ(bootloader.get_last_error(), BootloaderError::BOOTLOADER_STATE_CORRUPTED);
}

TEST_F(BootloaderFailureInjectionTest, PermissionFailure) {
    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    bootloader.initialize();

    chmod(test_dir_.c_str(), 0444);

    EXPECT_FALSE(bootloader.set_next_boot_slot(SlotId::SLOT_B));
    EXPECT_EQ(bootloader.get_last_error(), BootloaderError::PERSISTENCE_ERROR);

    chmod(test_dir_.c_str(), 0755);
}

TEST_F(BootloaderFailureInjectionTest, AtomicWriteFailure) {
    std::ofstream ofs(test_dir_ + "/bootloader/bootloader_state.json.tmp");
    ofs << "test";
    ofs.close();

    chmod((test_dir_ + "/bootloader").c_str(), 0444);

    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    bootloader.initialize();

    chmod((test_dir_ + "/bootloader").c_str(), 0755);
}

TEST_F(BootloaderFailureInjectionTest, ProcessInterruptionDuringPersistence) {
    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    bootloader.initialize();

    bootloader.set_next_boot_slot(SlotId::SLOT_B);

    std::ifstream ifs(test_dir_ + "/bootloader/bootloader_state.json");
    EXPECT_TRUE(ifs.good());
    ifs.close();
}

TEST_F(BootloaderFailureInjectionTest, InvalidMarkBootStarted) {
    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    bootloader.initialize();

    EXPECT_FALSE(bootloader.mark_boot_started(static_cast<SlotId>(99)));
    EXPECT_EQ(bootloader.get_last_error(), BootloaderError::INVALID_SLOT);
}

TEST_F(BootloaderFailureInjectionTest, InvalidResetAttempts) {
    SimulatedBootloader bootloader;
    bootloader.set_state_dir(test_dir_ + "/bootloader", test_dir_ + "/bootloader/bootloader_state.json");
    bootloader.initialize();

    EXPECT_FALSE(bootloader.reset_boot_attempts(static_cast<SlotId>(99)));
    EXPECT_EQ(bootloader.get_last_error(), BootloaderError::INVALID_SLOT);
}

}
}
