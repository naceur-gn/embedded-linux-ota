#include <gtest/gtest.h>
#include "boot/simulated_boot_control.h"
#include "slot/slot_manager.h"
#include "transaction/transaction_manager.h"
#include <filesystem>

namespace ota {
namespace {

class BootIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/ota_boot_integration_test_" + std::to_string(getpid());
        std::filesystem::create_directories(test_dir_);
        std::filesystem::create_directories(test_dir_ + "/boot");
        std::filesystem::create_directories(test_dir_ + "/slots");
        std::filesystem::create_directories(test_dir_ + "/state");
        std::filesystem::create_directories(test_dir_ + "/state/history");

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

        TransactionManagerConfig tm_config;
        tm_config.state_dir = test_dir_ + "/state";
        tm_config.history_dir = test_dir_ + "/state/history";
        tm_config.lock_file = test_dir_ + "/state/ota.lock";
        tm_config.max_history_entries = 10;

        transaction_manager_.set_config(tm_config);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    std::string test_dir_;
    SlotManager slot_manager_;
    SimulatedBootControl boot_control_;
    TransactionManager transaction_manager_;
};

TEST_F(BootIntegrationTest, FullUpdateWorkflow) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    EXPECT_TRUE(boot_control_.prepare_next_boot(slot_manager_.get_inactive_slot(), slot_manager_));

    SlotId next = boot_control_.get_next_boot_slot();
    EXPECT_EQ(next, slot_manager_.get_inactive_slot());

    BootState state = boot_control_.get_boot_state();
    EXPECT_EQ(state.current_slot, SlotId::SLOT_A);
    EXPECT_EQ(state.next_slot, slot_manager_.get_inactive_slot());
}

TEST_F(BootIntegrationTest, TransactionRecordsBootSlot) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    transaction_manager_.acquire_lock();
    transaction_manager_.create_transaction("2.0.0", "1.0.0", "hw-v1", "abc123");

    transaction_manager_.update_active_slot(slot_id_to_string(slot_manager_.get_active_slot()));
    transaction_manager_.update_target_slot(slot_id_to_string(slot_manager_.get_inactive_slot()));

    transaction_manager_.persist_transaction();
    transaction_manager_.release_lock();

    transaction_manager_.load_transaction();
    TransactionRecord tx = transaction_manager_.get_current_transaction();

    EXPECT_EQ(tx.active_slot, "A");
    EXPECT_EQ(tx.target_slot, "B");
}

TEST_F(BootIntegrationTest, SimulateBootUpdatesSlotManager) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(slot_manager_.get_inactive_slot());
    boot_control_.simulate_boot();

    SlotId current = boot_control_.get_current_boot_slot();
    EXPECT_EQ(current, slot_manager_.get_inactive_slot());
}

TEST_F(BootIntegrationTest, MultipleBootCycles) {
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

TEST_F(BootIntegrationTest, BootStatePersistenceAcrossRestarts) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);

    SimulatedBootControl new_boot_control;
    new_boot_control.set_config(boot_control_.get_test_config(test_dir_));
    new_boot_control.initialize();

    SlotId next = new_boot_control.get_next_boot_slot();
    EXPECT_EQ(next, SlotId::SLOT_B);
}

TEST_F(BootIntegrationTest, SimulatedBootPersistence) {
    slot_manager_.prepare_inactive_slot("2.0.0", "hw-v1", "abc123");

    boot_control_.set_next_boot_slot(SlotId::SLOT_B);
    boot_control_.simulate_boot();

    SimulatedBootControl new_boot_control;
    new_boot_control.set_config(boot_control_.get_test_config(test_dir_));
    new_boot_control.initialize();

    EXPECT_EQ(new_boot_control.get_current_boot_slot(), SlotId::SLOT_B);
    EXPECT_EQ(new_boot_control.get_boot_attempt_count(SlotId::SLOT_B), 1);
}

}
}