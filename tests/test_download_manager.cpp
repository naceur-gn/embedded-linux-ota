#include <gtest/gtest.h>
#include "download/download_manager.h"
#include <fstream>
#include <cstdio>

class DownloadManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/ota_download_test";
        system(("mkdir -p " + test_dir).c_str());
        manager.set_download_dir(test_dir);
    }

    void TearDown() override {
        system(("rm -rf " + test_dir).c_str());
    }

    std::string test_dir;
    ota::DownloadManager manager;
};

TEST_F(DownloadManagerTest, GetTempPath) {
    std::string path = manager.get_temp_path("1.0.0");
    EXPECT_EQ(path, test_dir + "/1.0.0.img");
}

TEST_F(DownloadManagerTest, CleanupDownload) {
    std::string test_file = test_dir + "/test.bin";
    std::ofstream file(test_file);
    file << "test data";
    file.close();

    EXPECT_TRUE(manager.cleanup_download(test_file));
    EXPECT_FALSE(std::ifstream(test_file).good());
}

TEST_F(DownloadManagerTest, CleanupEmptyPath) {
    EXPECT_FALSE(manager.cleanup_download(""));
}

TEST_F(DownloadManagerTest, CleanupPathOutsideDownloadDir) {
    EXPECT_FALSE(manager.cleanup_download("/tmp/other/file.bin"));
}

TEST_F(DownloadManagerTest, CalculateSha256) {
    std::string test_file = test_dir + "/test.bin";
    std::ofstream file(test_file);
    file << "hello world";
    file.close();

    std::string sha256 = ota::DownloadManager::calculate_sha256(test_file);
    EXPECT_FALSE(sha256.empty());
    EXPECT_EQ(sha256.length(), 64);
}

TEST_F(DownloadManagerTest, CalculateSha256Nonexistent) {
    std::string sha256 = ota::DownloadManager::calculate_sha256("/nonexistent/file.bin");
    EXPECT_TRUE(sha256.empty());
}
