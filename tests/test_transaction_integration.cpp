#include <gtest/gtest.h>
#include "transaction/transaction_manager.h"
#include "transaction/transaction_state_machine.h"
#include "device/device_config.h"
#include "device/device_state.h"
#include "validation/integrity_validator.h"
#include "security/signature_verifier.h"
#include "installation/installer.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace ota;

class TransactionIntegrationTest : public ::testing::Test {
protected:
    std::string test_dir_;
    TransactionManagerConfig config_;

    void SetUp() override {
        test_dir_ = "/tmp/ota_integ_test_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);
        config_ = TransactionManager::get_test_config(test_dir_);
    }

    void TearDown() override {
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

    std::string calculate_sha256(const std::string& path) {
        IntegrityValidator validator;
        return validator.calculate_sha256(path);
    }
};

TEST_F(TransactionIntegrationTest, FullWorkflowWithTransaction) {
    std::string image_path = test_dir_ + "/test-image.bin";
    create_test_image(image_path);
    std::string expected_hash = calculate_sha256(image_path);

    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_TRUE(tm.acquire_lock());

    tm.create_transaction("1.1.0", "1.0.0", "hw-v1", expected_hash);
    EXPECT_TRUE(tm.update_state(TransactionState::CHECKING));
    EXPECT_TRUE(tm.update_state(TransactionState::UPDATE_AVAILABLE));
    EXPECT_TRUE(tm.update_state(TransactionState::DOWNLOADING));
    EXPECT_TRUE(tm.update_download_path(image_path));
    EXPECT_TRUE(tm.update_state(TransactionState::DOWNLOADED));

    EXPECT_TRUE(tm.update_state(TransactionState::VERIFYING));
    IntegrityValidator validator;
    auto validation_result = validator.validate_file(image_path, expected_hash);
    EXPECT_TRUE(validation_result.is_valid());
    EXPECT_TRUE(tm.update_state(TransactionState::VERIFIED));

    InstallConfig install_config;
    install_config.staging_dir = test_dir_ + "/staging";
    install_config.install_target = test_dir_ + "/install-target";
    install_config.state_dir = test_dir_ + "/state";
    install_config.min_free_space_mb = 10;

    std::filesystem::create_directories(install_config.staging_dir);
    std::filesystem::create_directories(install_config.install_target);
    std::filesystem::create_directories(install_config.state_dir);

    InstallManager installer;
    installer.set_config(install_config);

    EXPECT_TRUE(tm.update_state(TransactionState::INSTALLING));
    EXPECT_TRUE(tm.update_installation_target(install_config.install_target));

    InstallInfo install_info;
    install_info.version = "1.1.0";
    install_info.image_path = image_path;
    install_info.expected_sha256 = expected_hash;
    install_info.expected_size = 0;

    struct stat image_stat;
    if (stat(image_path.c_str(), &image_stat) == 0) {
        install_info.expected_size = image_stat.st_size;
    }

    auto install_result = installer.install(install_info);
    EXPECT_TRUE(install_result.is_success());

    EXPECT_TRUE(tm.complete_transaction());
    EXPECT_EQ(tm.get_current_state(), TransactionState::INSTALLED);

    tm.release_lock();
}

TEST_F(TransactionIntegrationTest, TransactionReflectsFailure) {
    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_TRUE(tm.acquire_lock());

    tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
    tm.update_state(TransactionState::CHECKING);
    tm.update_state(TransactionState::UPDATE_AVAILABLE);
    tm.update_state(TransactionState::DOWNLOADING);
    tm.record_failure("NETWORK_ERROR", "Connection refused");

    auto tx = tm.get_current_transaction();
    EXPECT_EQ(tx.state, TransactionState::FAILED);
    EXPECT_EQ(tx.error_code, "NETWORK_ERROR");

    tm.release_lock();
}

TEST_F(TransactionIntegrationTest, StatePersistsAcrossRestart) {
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
        EXPECT_EQ(tx.target_version, "1.1.0");
    }
}

TEST_F(TransactionIntegrationTest, TransactionRecordedInHistory) {
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
        tm.load_history();
        auto history = tm.get_history();
        EXPECT_EQ(history.size(), 1u);
        EXPECT_EQ(history[0].version, "1.1.0");
        EXPECT_EQ(history[0].result, "SUCCESS");
    }
}

TEST_F(TransactionIntegrationTest, MultipleTransactionsInHistory) {
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

        tm.create_transaction("1.2.0", "1.1.0", "hw-v1");
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
        tm.load_history();
        auto history = tm.get_history();
        EXPECT_EQ(history.size(), 2u);
    }
}

TEST_F(TransactionIntegrationTest, FailedTransactionRecorded) {
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
        tm.load_history();
        auto history = tm.get_history();
        EXPECT_EQ(history.size(), 1u);
        EXPECT_EQ(history[0].result, "FAILED");
    }
}
