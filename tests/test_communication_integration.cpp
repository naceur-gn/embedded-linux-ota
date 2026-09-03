#include <gtest/gtest.h>
#include "client/update_manager.h"
#include "device/device_config.h"
#include "download/download_manager.h"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/ota_integration_test";
        system(("mkdir -p " + test_dir).c_str());
        system(("mkdir -p " + test_dir + "/releases").c_str());
        system(("mkdir -p " + test_dir + "/downloads").c_str());

        create_test_release();
    }

    void TearDown() override {
        system(("rm -rf " + test_dir).c_str());
    }

    void create_test_release() {
        std::string release_dir = test_dir + "/releases/1.1.0";
        system(("mkdir -p " + release_dir).c_str());

        test_image_data = "test firmware image data for integration test";
        std::ofstream image(release_dir + "/image.bin");
        image << test_image_data;
        image.close();

        std::ofstream metadata(release_dir + "/metadata.json");
        metadata << R"({
            "version": "1.1.0",
            "hardware_version": "revA",
            "release_type": "system",
            "image": "image.bin",
            "sha256": "test_hash",
            "size": )"
                 << test_image_data.length()
                 << R"(,
            "timestamp": "2026-09-03T12:00:00Z"
        })";
        metadata.close();
    }

    std::string test_dir;
    std::string test_image_data;
};

TEST_F(IntegrationTest, ResponseParserWithRealJson) {
    std::string json = R"({
        "update_available": true,
        "version": "1.1.0",
        "hardware_version": "revA",
        "image": "/releases/1.1.0/image.bin",
        "size": 43,
        "sha256": "test_hash"
    })";

    ota::UpdateMetadata metadata;
    ASSERT_TRUE(ota::ResponseParser::parse_update_response(json, metadata));
    EXPECT_TRUE(metadata.update_available);
    EXPECT_EQ(metadata.version, "1.1.0");
    EXPECT_EQ(metadata.hardware_version, "revA");
    EXPECT_EQ(metadata.image_size, 43);
}

TEST_F(IntegrationTest, VersionComparison) {
    ota::UpdateManager manager;

    EXPECT_TRUE(manager.compare_versions("1.0.0", "1.1.0"));
    EXPECT_FALSE(manager.compare_versions("1.1.0", "1.1.0"));
    EXPECT_FALSE(manager.compare_versions("1.2.0", "1.1.0"));
}

TEST_F(IntegrationTest, CompatibilityCheck) {
    ota::UpdateManager manager;

    EXPECT_TRUE(manager.is_compatible("revA", "revA"));
    EXPECT_FALSE(manager.is_compatible("revA", "revB"));
}

TEST_F(IntegrationTest, DownloadManagerCleanup) {
    std::string download_dir = "/tmp/ota_dl_test_cleanup";
    system(("mkdir -p " + download_dir).c_str());

    ota::DownloadManager manager;
    manager.set_download_dir(download_dir);

    std::string test_file = download_dir + "/test.bin";
    std::ofstream file(test_file);
    file << "test data";
    file.close();

    EXPECT_TRUE(manager.cleanup_download(test_file));
    EXPECT_FALSE(std::ifstream(test_file).good());

    system(("rm -rf " + download_dir).c_str());
}

TEST_F(IntegrationTest, Sha256Calculation) {
    std::string test_file = "/tmp/ota_sha256_test.bin";
    std::ofstream file(test_file);
    file << "hello world";
    file.close();

    std::string sha256 = ota::DownloadManager::calculate_sha256(test_file);
    EXPECT_FALSE(sha256.empty());
    EXPECT_EQ(sha256.length(), 64);

    remove(test_file.c_str());
}
