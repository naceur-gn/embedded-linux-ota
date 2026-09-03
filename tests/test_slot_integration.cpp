#include <gtest/gtest.h>
#include "slot/slot_manager.h"
#include "transaction/transaction_manager.h"
#include "installation/installer.h"
#include "validation/integrity_validator.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace ota;

class SlotIntegrationTest : public ::testing::Test {
protected:
    std::string test_dir_;
    SlotConfig slot_config_;
    TransactionManagerConfig tm_config_;
    SlotManager sm_;
    TransactionManager tm_;

    void SetUp() override {
        test_dir_ = "/tmp/ota_slot_integ_test_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);

        slot_config_ = SlotManager::get_test_config(test_dir_);
        sm_.set_config(slot_config_);

        tm_config_.state_dir = test_dir_ + "/state";
        tm_config_.history_dir = test_dir_ + "/state/history";
        tm_config_.lock_file = test_dir_ + "/ota.lock";
        tm_config_.max_history_entries = 10;
        tm_.set_config(tm_config_);
    }

    void TearDown() override {
        tm_.release_lock();
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

    std::string create_test_image(const std::string& path, size_t size = 1024) {
        std::ofstream file(path, std::ios::binary);
        for (size_t i = 0; i < size; ++i) {
            char c = static_cast<char>(i % 256);
            file.write(&c, 1);
        }
        file.close();
        return path;
    }
};

TEST_F(SlotIntegrationTest, FullWorkflowWithSlots) {
    sm_.initialize_slots();

    EXPECT_EQ(sm_.get_active_slot(), SlotId::SLOT_A);
    EXPECT_EQ(sm_.get_inactive_slot(), SlotId::SLOT_B);

    EXPECT_TRUE(sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123"));

    auto slot_b = sm_.get_slot_info(SlotId::SLOT_B);
    EXPECT_EQ(slot_b.version, "1.1.0");
    EXPECT_EQ(slot_b.state, SlotState::PREPARED);

    auto slot_a = sm_.get_slot_info(SlotId::SLOT_A);
    EXPECT_EQ(slot_a.version, "1.0.0");
    EXPECT_EQ(slot_a.state, SlotState::ACTIVE);
}

TEST_F(SlotIntegrationTest, SlotStatePersistsAcrossRestart) {
    sm_.initialize_slots();
    sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123");

    SlotManager sm2;
    sm2.set_config(slot_config_);
    sm2.load_slot_state();

    EXPECT_EQ(sm2.get_active_slot(), SlotId::SLOT_A);
    EXPECT_EQ(sm2.get_inactive_slot(), SlotId::SLOT_B);

    auto slot_b = sm2.get_slot_info(SlotId::SLOT_B);
    EXPECT_EQ(slot_b.version, "1.1.0");
    EXPECT_EQ(slot_b.state, SlotState::PREPARED);
}

TEST_F(SlotIntegrationTest, TransactionRecordsSlotInfo) {
    sm_.initialize_slots();

    EXPECT_TRUE(tm_.acquire_lock());
    EXPECT_TRUE(tm_.create_transaction("1.1.0", "1.0.0", "hw-v1"));

    EXPECT_TRUE(tm_.update_active_slot("A"));
    EXPECT_TRUE(tm_.update_target_slot("B"));

    auto tx = tm_.get_current_transaction();
    EXPECT_EQ(tx.active_slot, "A");
    EXPECT_EQ(tx.target_slot, "B");
}

TEST_F(SlotIntegrationTest, SlotManagerProtectsActiveSlot) {
    sm_.initialize_slots();

    SlotId active = sm_.get_active_slot();
    SlotId inactive = sm_.get_inactive_slot();

    EXPECT_TRUE(sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123"));

    auto active_info = sm_.get_slot_info(active);
    EXPECT_EQ(active_info.state, SlotState::ACTIVE);

    auto inactive_info = sm_.get_slot_info(inactive);
    EXPECT_EQ(inactive_info.state, SlotState::PREPARED);
}

TEST_F(SlotIntegrationTest, CannotSwitchToEmptySlot) {
    sm_.initialize_slots();

    EXPECT_FALSE(sm_.switch_active_slot());
}

TEST_F(SlotIntegrationTest, SwitchAfterPrepare) {
    sm_.initialize_slots();
    sm_.prepare_inactive_slot("1.1.0", "hw-v1", "abc123");

    EXPECT_TRUE(sm_.switch_active_slot());

    EXPECT_EQ(sm_.get_active_slot(), SlotId::SLOT_B);
    EXPECT_EQ(sm_.get_inactive_slot(), SlotId::SLOT_A);
}

TEST_F(SlotIntegrationTest, SlotValidationDetectsInconsistency) {
    sm_.initialize_slots();

    SlotInfo slot_a = sm_.get_slot_info(SlotId::SLOT_A);
    EXPECT_TRUE(sm_.validate_slot(SlotId::SLOT_A));

    sm_.set_slot_state(SlotId::SLOT_B, SlotState::ACTIVE);

    EXPECT_FALSE(sm_.validate_slot(SlotId::SLOT_B));
}

TEST_F(SlotIntegrationTest, SlotIntegrityCheck) {
    sm_.initialize_slots();
    sm_.set_slot_sha256(SlotId::SLOT_A, "abc123");

    EXPECT_TRUE(sm_.validate_slot_integrity(SlotId::SLOT_A, "abc123"));
    EXPECT_FALSE(sm_.validate_slot_integrity(SlotId::SLOT_A, "def456"));
}
