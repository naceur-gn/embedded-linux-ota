#include <gtest/gtest.h>
#include "boot/simulated_boot_control.h"
#include "slot/slot_manager.h"
#include <filesystem>
#include <fstream>

namespace ota {
namespace {

class BootFailureInjectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/ota_boot_failure_test_" + std::to_string(getpid());
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
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    SlotManager slot_manager_;
};

TEST_F(BootFailureInjectionTest, CorruptedBootState) {
    std::ofstream ofs(test_dir_ + "/boot/boot_state.json");
    ofs << "{corrupted json content";
    ofs.close();

    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);

    EXPECT_TRUE(boot_control.initialize());

    BootState state = boot_control.get_boot_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
}

TEST_F(BootFailureInjectionTest, MissingBootStateFile) {
    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/nonexistent.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);

    EXPECT_TRUE(boot_control.initialize());

    BootState state = boot_control.get_boot_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
    EXPECT_EQ(state.boot_attempts.at(SlotId::SLOT_A), 0);
    EXPECT_EQ(state.boot_attempts.at(SlotId::SLOT_B), 0);
}

TEST_F(BootFailureInjectionTest, InvalidSlotID) {
    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    EXPECT_FALSE(boot_control.set_next_boot_slot(static_cast<SlotId>(99)));
    EXPECT_FALSE(boot_control.set_next_boot_slot(static_cast<SlotId>(-1)));
}

TEST_F(BootFailureInjectionTest, InvalidNextSlotValue) {
    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    EXPECT_FALSE(boot_control.set_next_boot_slot(static_cast<SlotId>(100)));
}

TEST_F(BootFailureInjectionTest, CorruptedTargetSlot) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::INVALID);

    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    EXPECT_FALSE(boot_control.validate_boot_target(SlotId::SLOT_B, slot_manager_));
    EXPECT_FALSE(boot_control.prepare_next_boot(SlotId::SLOT_B, slot_manager_));
}

TEST_F(BootFailureInjectionTest, EmptyTargetSlot) {
    slot_manager_.set_slot_state(SlotId::SLOT_B, SlotState::EMPTY);

    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    EXPECT_FALSE(boot_control.validate_boot_target(SlotId::SLOT_B, slot_manager_));
    EXPECT_FALSE(boot_control.prepare_next_boot(SlotId::SLOT_B, slot_manager_));
}

TEST_F(BootFailureInjectionTest, ActiveSlotAsTarget) {
    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    EXPECT_FALSE(boot_control.validate_boot_target(SlotId::SLOT_A, slot_manager_));
    EXPECT_FALSE(boot_control.prepare_next_boot(SlotId::SLOT_A, slot_manager_));
}

TEST_F(BootFailureInjectionTest, PermissionFailure) {
    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    chmod(test_dir_.c_str(), 0444);

    EXPECT_FALSE(boot_control.set_next_boot_slot(SlotId::SLOT_B));

    chmod(test_dir_.c_str(), 0755);
}

TEST_F(BootFailureInjectionTest, AtomicWriteFailure) {
    std::ofstream ofs(test_dir_ + "/boot/boot_state.json.tmp");
    ofs << "test";
    ofs.close();

    chmod((test_dir_ + "/boot").c_str(), 0444);

    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    chmod((test_dir_ + "/boot").c_str(), 0755);
}

TEST_F(BootFailureInjectionTest, InvalidBootStateAfterCorruption) {
    std::ofstream ofs(test_dir_ + "/boot/boot_state.json");
    ofs << R"({
        "current_slot": "A",
        "next_slot_set": true,
        "next_slot": "B",
        "boot_attempts": {
            "A": -1,
            "B": "invalid"
        }
    })";
    ofs.close();

    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    BootState state = boot_control.get_boot_state();
    EXPECT_EQ(state.boot_attempts.at(SlotId::SLOT_A), 0);
    EXPECT_EQ(state.boot_attempts.at(SlotId::SLOT_B), 0);
}

TEST_F(BootFailureInjectionTest, ProcessInterruptionDuringPersistence) {
    SimulatedBootConfig boot_config;
    boot_config.boot_state_dir = test_dir_ + "/boot";
    boot_config.boot_state_file = test_dir_ + "/boot/boot_state.json";

    SimulatedBootControl boot_control;
    boot_control.set_config(boot_config);
    boot_control.initialize();

    boot_control.set_next_boot_slot(SlotId::SLOT_B);

    std::ifstream ifs(boot_config.boot_state_file);
    EXPECT_TRUE(ifs.good());
    ifs.close();
}

}
}