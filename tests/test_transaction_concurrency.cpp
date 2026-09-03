#include <gtest/gtest.h>
#include "transaction/transaction_manager.h"
#include "transaction/transaction_state_machine.h"
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <thread>
#include <atomic>

using namespace ota;

class TransactionConcurrencyTest : public ::testing::Test {
protected:
    std::string test_dir_;
    TransactionManagerConfig config_;

    void SetUp() override {
        test_dir_ = "/tmp/ota_conc_test_" + std::to_string(getpid()) + "_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);
        config_ = TransactionManager::get_test_config(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
};

TEST_F(TransactionConcurrencyTest, SecondLockRejectedFromDifferentProcess) {
    TransactionManager tm1;
    tm1.set_config(config_);
    EXPECT_TRUE(tm1.acquire_lock());

    pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0) {
        TransactionManager tm_child;
        tm_child.set_config(config_);
        bool got_lock = tm_child.acquire_lock();
        _exit(got_lock ? 0 : 1);
    }

    int status;
    waitpid(pid, &status, 0);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_NE(WEXITSTATUS(status), 0);

    tm1.release_lock();
}

TEST_F(TransactionConcurrencyTest, LockReleasedAfterProcessExit) {
    pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0) {
        TransactionManager tm;
        tm.set_config(config_);
        tm.acquire_lock();
        _exit(0);
    }

    int status;
    waitpid(pid, &status, 0);
    EXPECT_TRUE(WIFEXITED(status));

    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_TRUE(tm.acquire_lock());
}

TEST_F(TransactionConcurrencyTest, LockReleasedAfterDestroy) {
    {
        TransactionManager tm;
        tm.set_config(config_);
        tm.acquire_lock();
    }

    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_TRUE(tm.acquire_lock());
}

TEST_F(TransactionConcurrencyTest, ConcurrentCreateRejected) {
    pid_t pid1 = fork();
    ASSERT_NE(pid1, -1);

    if (pid1 == 0) {
        TransactionManager tm;
        tm.set_config(config_);
        if (tm.acquire_lock()) {
            tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
            sleep(2);
            _exit(0);
        }
        _exit(1);
    }

    usleep(100000);

    TransactionManager tm2;
    tm2.set_config(config_);
    EXPECT_FALSE(tm2.acquire_lock());

    int status;
    waitpid(pid1, &status, 0);
}

TEST_F(TransactionConcurrencyTest, LockFileExists) {
    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_TRUE(tm.acquire_lock());

    EXPECT_TRUE(std::filesystem::exists(config_.lock_file));
}

TEST_F(TransactionConcurrencyTest, LockFileContainsPid) {
    TransactionManager tm;
    tm.set_config(config_);
    EXPECT_TRUE(tm.acquire_lock());

    std::ifstream lock_file(config_.lock_file);
    std::string pid_str;
    lock_file >> pid_str;
    EXPECT_EQ(pid_str, std::to_string(getpid()));
}

TEST_F(TransactionConcurrencyTest, ConcurrentModificationRejected) {
    pid_t pid1 = fork();
    ASSERT_NE(pid1, -1);

    if (pid1 == 0) {
        TransactionManager tm;
        tm.set_config(config_);
        if (tm.acquire_lock()) {
            tm.create_transaction("1.1.0", "1.0.0", "hw-v1");
            tm.update_state(TransactionState::CHECKING);
            sleep(1);
            tm.update_state(TransactionState::DOWNLOADING);
            _exit(0);
        }
        _exit(1);
    }

    usleep(100000);

    TransactionManager tm2;
    tm2.set_config(config_);
    EXPECT_FALSE(tm2.acquire_lock());

    int status;
    waitpid(pid1, &status, 0);
    EXPECT_TRUE(WIFEXITED(status));
}
