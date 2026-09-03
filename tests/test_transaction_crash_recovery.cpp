#include <gtest/gtest.h>
#include "transaction/transaction_manager.h"
#include "transaction/transaction_state_machine.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

using namespace ota;

class TransactionCrashRecoveryTest : public ::testing::Test {
protected:
    std::string test_dir_;
    TransactionManagerConfig config_;

    void SetUp() override {
        test_dir_ = "/tmp/ota_crash_test_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);
        config_ = TransactionManager::get_test_config(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
};

TEST_F(TransactionCrashRecoveryTest, DetectInstallingStateAfterRestart) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::CHECKING);
        tm.update_state(TransactionState::UPDATE_AVAILABLE);
        tm.update_state(TransactionState::DOWNLOADING);
        tm.update_state(TransactionState::DOWNLOADED);
        tm.update_state(TransactionState::VERIFYING);
        tm.update_state(TransactionState::VERIFIED);
        tm.update_state(TransactionState::INSTALLING);
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());

        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.state, TransactionState::INSTALLING);

        EXPECT_TRUE(tm.has_incomplete_transaction());
        std::string msg = tm.detect_incomplete_state();
        EXPECT_NE(msg.find("INSTALLING"), std::string::npos);
        EXPECT_NE(msg.find("Recovery required"), std::string::npos);
    }
}

TEST_F(TransactionCrashRecoveryTest, DetectDownloadingStateAfterRestart) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::CHECKING);
        tm.update_state(TransactionState::UPDATE_AVAILABLE);
        tm.update_state(TransactionState::DOWNLOADING);
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());

        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.state, TransactionState::DOWNLOADING);

        EXPECT_TRUE(tm.has_incomplete_transaction());
    }
}

TEST_F(TransactionCrashRecoveryTest, DetectVerifyingStateAfterRestart) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::CHECKING);
        tm.update_state(TransactionState::UPDATE_AVAILABLE);
        tm.update_state(TransactionState::DOWNLOADING);
        tm.update_state(TransactionState::DOWNLOADED);
        tm.update_state(TransactionState::VERIFYING);
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());

        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.state, TransactionState::VERIFYING);

        EXPECT_TRUE(tm.has_incomplete_transaction());
    }
}

TEST_F(TransactionCrashRecoveryTest, SuccessfulInstallationDetected) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::CHECKING);
        tm.update_state(TransactionState::UPDATE_AVAILABLE);
        tm.update_state(TransactionState::DOWNLOADING);
        tm.update_state(TransactionState::DOWNLOADED);
        tm.update_state(TransactionState::VERIFYING);
        tm.update_state(TransactionState::VERIFIED);
        tm.update_state(TransactionState::INSTALLING);
        tm.complete_transaction();
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());

        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.state, TransactionState::INSTALLED);

        EXPECT_FALSE(tm.has_incomplete_transaction());
    }
}

TEST_F(TransactionCrashRecoveryTest, FailedTransactionDetected) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::CHECKING);
        tm.update_state(TransactionState::UPDATE_AVAILABLE);
        tm.update_state(TransactionState::DOWNLOADING);
        tm.record_failure("NETWORK_ERROR", "Connection timeout");
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());

        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.state, TransactionState::FAILED);
        EXPECT_EQ(tx.error_code, "NETWORK_ERROR");
        EXPECT_EQ(tx.error_message, "Connection timeout");

        EXPECT_TRUE(tm.has_incomplete_transaction());
    }
}

TEST_F(TransactionCrashRecoveryTest, IdleStateAfterRestart) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::CHECKING);
        tm.update_state(TransactionState::UPDATE_AVAILABLE);
        tm.update_state(TransactionState::DOWNLOADING);
        tm.update_state(TransactionState::DOWNLOADED);
        tm.update_state(TransactionState::VERIFYING);
        tm.update_state(TransactionState::VERIFIED);
        tm.update_state(TransactionState::INSTALLING);
        tm.complete_transaction();
        tm.update_state(TransactionState::IDLE);
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());

        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.state, TransactionState::IDLE);

        EXPECT_FALSE(tm.has_incomplete_transaction());
    }
}

TEST_F(TransactionCrashRecoveryTest, TransactionIdPreserved) {
    std::string saved_id;
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::DOWNLOADING);
        saved_id = tm.get_current_transaction().transaction_id;
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());
        EXPECT_EQ(tm.get_current_transaction().transaction_id, saved_id);
    }
}

TEST_F(TransactionCrashRecoveryTest, ErrorDetailsPreserved) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::CHECKING);
        tm.record_failure("SIGNATURE_INVALID", "Bad signature");
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());
        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.error_code, "SIGNATURE_INVALID");
        EXPECT_EQ(tx.error_message, "Bad signature");
    }
}
