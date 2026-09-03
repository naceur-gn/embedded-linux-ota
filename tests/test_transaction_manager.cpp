#include <gtest/gtest.h>
#include "transaction/transaction_manager.h"
#include "transaction/transaction_state_machine.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <thread>
#include <chrono>

using namespace ota;

class TransactionManagerTest : public ::testing::Test {
protected:
    std::string test_dir_;
    TransactionManagerConfig config_;
    TransactionManager tm_;

    void SetUp() override {
        test_dir_ = "/tmp/ota_test_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);
        config_ = TransactionManager::get_test_config(test_dir_);
        tm_.set_config(config_);
    }

    void TearDown() override {
        tm_.release_lock();
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
};

TEST_F(TransactionManagerTest, GenerateTransactionId) {
    std::string id1 = tm_.generate_transaction_id();
    std::string id2 = tm_.generate_transaction_id();

    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_NE(id1, id2);
    EXPECT_EQ(id1.size(), 36);
    EXPECT_EQ(id2.size(), 36);
}

TEST_F(TransactionManagerTest, GetCurrentTimestamp) {
    std::string ts = tm_.get_current_timestamp();
    EXPECT_FALSE(ts.empty());
    EXPECT_NE(ts.find("T"), std::string::npos);
    EXPECT_NE(ts.find("Z"), std::string::npos);
}

TEST_F(TransactionManagerTest, CreateTransaction) {
    EXPECT_TRUE(tm_.create_transaction("1.1.0", "1.0.0", "hw-v1"));

    auto tx = tm_.get_current_transaction();
    EXPECT_FALSE(tx.transaction_id.empty());
    EXPECT_EQ(tx.state, TransactionState::IDLE);
    EXPECT_EQ(tx.target_version, "1.1.0");
    EXPECT_EQ(tx.source_version, "1.0.0");
    EXPECT_EQ(tx.hardware_version, "hw-v1");
    EXPECT_EQ(tx.owner_pid, getpid());
}

TEST_F(TransactionManagerTest, UpdateState) {
    tm_.create_transaction("1.1.0", "1.0.0", "hw-v1");
    EXPECT_TRUE(tm_.update_state(TransactionState::CHECKING));
    EXPECT_EQ(tm_.get_current_state(), TransactionState::CHECKING);

    EXPECT_TRUE(tm_.update_state(TransactionState::UPDATE_AVAILABLE));
    EXPECT_EQ(tm_.get_current_state(), TransactionState::UPDATE_AVAILABLE);
}

TEST_F(TransactionManagerTest, InvalidTransitionRejected) {
    tm_.create_transaction("1.1.0", "1.0.0", "hw-v1");
    EXPECT_TRUE(tm_.update_state(TransactionState::CHECKING));
    EXPECT_FALSE(tm_.update_state(TransactionState::INSTALLED));
    EXPECT_EQ(tm_.get_current_state(), TransactionState::CHECKING);
}

TEST_F(TransactionManagerTest, RecordFailure) {
    tm_.create_transaction("1.1.0", "1.0.0", "hw-v1");
    tm_.update_state(TransactionState::CHECKING);
    EXPECT_TRUE(tm_.record_failure("SIGNATURE_INVALID", "Digital signature verification failed"));

    auto tx = tm_.get_current_transaction();
    EXPECT_EQ(tx.state, TransactionState::FAILED);
    EXPECT_EQ(tx.error_code, "SIGNATURE_INVALID");
    EXPECT_EQ(tx.error_message, "Digital signature verification failed");
}

TEST_F(TransactionManagerTest, CompleteTransaction) {
    tm_.create_transaction("1.1.0", "1.0.0", "hw-v1");
    EXPECT_TRUE(tm_.update_state(TransactionState::CHECKING));
    EXPECT_TRUE(tm_.update_state(TransactionState::UPDATE_AVAILABLE));
    EXPECT_TRUE(tm_.update_state(TransactionState::DOWNLOADING));
    EXPECT_TRUE(tm_.update_state(TransactionState::DOWNLOADED));
    EXPECT_TRUE(tm_.update_state(TransactionState::VERIFYING));
    EXPECT_TRUE(tm_.update_state(TransactionState::VERIFIED));
    EXPECT_TRUE(tm_.update_state(TransactionState::INSTALLING));
    EXPECT_TRUE(tm_.complete_transaction());

    EXPECT_EQ(tm_.get_current_state(), TransactionState::INSTALLED);
}

TEST_F(TransactionManagerTest, HasActiveTransaction) {
    EXPECT_FALSE(tm_.has_active_transaction());

    tm_.create_transaction("1.1.0", "1.0.0", "hw-v1");
    EXPECT_FALSE(tm_.has_active_transaction());

    tm_.update_state(TransactionState::CHECKING);
    EXPECT_TRUE(tm_.has_active_transaction());

    tm_.update_state(TransactionState::UPDATE_AVAILABLE);
    tm_.update_state(TransactionState::DOWNLOADING);
    tm_.update_state(TransactionState::DOWNLOADED);
    tm_.update_state(TransactionState::VERIFYING);
    tm_.update_state(TransactionState::VERIFIED);
    tm_.update_state(TransactionState::INSTALLING);
    tm_.update_state(TransactionState::INSTALLED);
    EXPECT_FALSE(tm_.has_active_transaction());
}

TEST_F(TransactionManagerTest, AbortTransaction) {
    tm_.create_transaction("1.1.0", "1.0.0", "hw-v1");
    tm_.update_state(TransactionState::CHECKING);
    EXPECT_TRUE(tm_.abort_transaction());

    auto tx = tm_.get_current_transaction();
    EXPECT_EQ(tx.state, TransactionState::FAILED);
    EXPECT_EQ(tx.error_code, "ABORTED");
}

TEST_F(TransactionManagerTest, UpdateDownloadPath) {
    tm_.create_transaction("1.1.0", "1.0.0", "hw-v1");
    EXPECT_TRUE(tm_.update_download_path("/var/lib/ota/downloads/1.1.0.bin"));

    auto tx = tm_.get_current_transaction();
    EXPECT_EQ(tx.download_path, "/var/lib/ota/downloads/1.1.0.bin");
}

TEST_F(TransactionManagerTest, UpdateInstallationTarget) {
    tm_.create_transaction("1.1.0", "1.0.0", "hw-v1");
    EXPECT_TRUE(tm_.update_installation_target("/var/lib/ota/install-target"));

    auto tx = tm_.get_current_transaction();
    EXPECT_EQ(tx.installation_target, "/var/lib/ota/install-target");
}
