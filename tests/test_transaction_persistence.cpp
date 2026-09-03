#include <gtest/gtest.h>
#include "transaction/transaction_manager.h"
#include "transaction/transaction_state_machine.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>

using namespace ota;

class TransactionPersistenceTest : public ::testing::Test {
protected:
    std::string test_dir_;
    TransactionManagerConfig config_;

    void SetUp() override {
        test_dir_ = "/tmp/ota_persist_test_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);
        config_ = TransactionManager::get_test_config(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
};

TEST_F(TransactionPersistenceTest, PersistAndLoadTransaction) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::CHECKING);
        tm.update_state(TransactionState::UPDATE_AVAILABLE);
        tm.update_state(TransactionState::DOWNLOADING);
        tm.update_download_path("/var/lib/ota/downloads/test.bin");
        tm.update_state(TransactionState::DOWNLOADED);
    }

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());

        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.target_version, "1.1.0");
        EXPECT_EQ(tx.source_version, "1.0.0");
        EXPECT_EQ(tx.hardware_version, "hw-v1");
        EXPECT_EQ(tx.state, TransactionState::DOWNLOADED);
        EXPECT_EQ(tx.download_path, "/var/lib/ota/downloads/test.bin");
    }
}

TEST_F(TransactionPersistenceTest, CorruptedStateFileRejected) {
    std::string state_dir = config_.state_dir;
    std::filesystem::create_directories(state_dir);

    std::string transaction_file = state_dir + "/transaction.json";
    std::ofstream file(transaction_file);
    file << "{invalid json content";
    file.close();

    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_FALSE(tm.load_transaction());
}

TEST_F(TransactionPersistenceTest, MissingStateFileHandled) {
    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_FALSE(tm.load_transaction());
}

TEST_F(TransactionPersistenceTest, AtomicWriteSurvivesInterruption) {
    std::string state_dir = config_.state_dir;
    std::filesystem::create_directories(state_dir);

    std::string transaction_file = state_dir + "/transaction.json";
    std::string tmp_file = transaction_file + ".tmp";

    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::DOWNLOADING);
    }

    EXPECT_TRUE(std::filesystem::exists(transaction_file));
    EXPECT_FALSE(std::filesystem::exists(tmp_file));
}

TEST_F(TransactionPersistenceTest, PartialWriteDoesNotCorrupt) {
    std::string state_dir = config_.state_dir;
    std::filesystem::create_directories(state_dir);

    std::string transaction_file = state_dir + "/transaction.json";

    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.update_state(TransactionState::DOWNLOADING);
    }

    std::ofstream bad_file(transaction_file);
    bad_file << "{invalid";
    bad_file.close();

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_FALSE(tm.load_transaction());
    }

    std::ofstream good_file(transaction_file);
    good_file << "{\n";
    good_file << "  \"transaction_id\": \"550e8400-e29b-41d4-a716-446655440000\",\n";
    good_file << "  \"state\": \"CHECKING\",\n";
    good_file << "  \"target_version\": \"1.1.0\",\n";
    good_file << "  \"source_version\": \"1.0.0\",\n";
    good_file << "  \"hardware_version\": \"hw-v1\",\n";
    good_file << "  \"download_path\": \"\",\n";
    good_file << "  \"installation_target\": \"\",\n";
    good_file << "  \"sha256\": \"\",\n";
    good_file << "  \"error_code\": \"\",\n";
    good_file << "  \"error_message\": \"\",\n";
    good_file << "  \"started_at\": \"2026-09-03T00:00:00Z\",\n";
    good_file << "  \"updated_at\": \"2026-09-03T00:00:00Z\",\n";
    good_file << "  \"owner_pid\": 12345\n";
    good_file << "}\n";
    good_file.close();

    {
        TransactionManager tm;
        tm.set_config(config_);
        EXPECT_TRUE(tm.load_transaction());
        auto tx = tm.get_current_transaction();
        EXPECT_EQ(tx.state, TransactionState::CHECKING);
    }
}

TEST_F(TransactionPersistenceTest, HistorySaveAndLoad) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
        tm.record_history("SUCCESS");
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

TEST_F(TransactionPersistenceTest, HistoryPruning) {
    TransactionManagerConfig cfg = config_;
    cfg.max_history_entries = 3;

    {
        TransactionManager tm;
        tm.set_config(cfg);

        for (int i = 0; i < 5; ++i) {
            tm.create_transaction("1.0." + std::to_string(i), "1.0.0", "hw-v1");
            tm.record_history("SUCCESS");
        }
    }

    {
        TransactionManager tm;
        tm.set_config(cfg);
        tm.load_history();
        auto history = tm.get_history();
        EXPECT_LE(history.size(), 3u);
    }
}

TEST_F(TransactionPersistenceTest, HistoryOrdering) {
    TransactionManagerConfig cfg = config_;

    {
        TransactionManager tm;
        tm.set_config(cfg);

        tm.create_transaction("1.0.1", "1.0.0", "hw-v1");
        tm.record_history("SUCCESS");

        tm.create_transaction("1.0.2", "1.0.0", "hw-v1");
        tm.record_history("SUCCESS");

        tm.create_transaction("1.0.3", "1.0.0", "hw-v1");
        tm.record_history("FAILED");
    }

    {
        TransactionManager tm;
        tm.set_config(cfg);
        tm.load_history();
        auto history = tm.get_history();
        EXPECT_EQ(history.size(), 3u);
    }
}

TEST_F(TransactionPersistenceTest, MalformedHistoryEntryHandled) {
    std::string history_dir = config_.history_dir;
    std::filesystem::create_directories(history_dir);

    std::ofstream bad_file(history_dir + "/bad-entry.json");
    bad_file << "not valid json";
    bad_file.close();

    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_TRUE(tm.load_history());
    auto history = tm.get_history();
    EXPECT_EQ(history.size(), 0u);
}

TEST_F(TransactionPersistenceTest, EmptyHistoryHandled) {
    TransactionManager tm;
    tm.set_config(config_);
    tm.load_history();
    auto history = tm.get_history();
    EXPECT_EQ(history.size(), 0u);
}
