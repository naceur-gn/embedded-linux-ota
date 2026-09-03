#include <gtest/gtest.h>
#include "installation/installer.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

class InstallManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/ota_install_test";
        system(("mkdir -p " + test_dir + "/staging").c_str());
        system(("mkdir -p " + test_dir + "/install-target").c_str());
        system(("mkdir -p " + test_dir + "/state").c_str());

        config.staging_dir = test_dir + "/staging";
        config.install_target = test_dir + "/install-target";
        config.state_dir = test_dir + "/state";
        config.min_free_space_mb = 1;

        installer.set_config(config);
    }

    void TearDown() override {
        system(("rm -rf " + test_dir).c_str());
    }

    std::string create_test_image(const std::string& name, const std::string& content) {
        std::string path = test_dir + "/" + name;
        std::ofstream file(path, std::ios::binary);
        file.write(content.c_str(), content.length());
        file.close();
        return path;
    }

    std::string calculate_sha256(const std::string& path) {
        return installer.calculate_sha256(path);
    }

    std::string test_dir;
    ota::InstallConfig config;
    ota::InstallManager installer;
};

TEST_F(InstallManagerTest, SuccessfulInstallation) {
    std::string content = "test update content";
    std::string image_path = create_test_image("test-image.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::INSTALLED);
    EXPECT_FALSE(result.installed_path.empty());
    EXPECT_EQ(result.calculated_sha256, sha256);
}

TEST_F(InstallManagerTest, InvalidVersionPathTraversal) {
    std::string content = "test content";
    std::string image_path = create_test_image("test.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "../../etc/passwd";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::PATH_TRAVERSAL_DETECTED);
}

TEST_F(InstallManagerTest, InvalidImageNotFound) {
    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = "/nonexistent/image.bin";
    info.expected_sha256 = "abc123";
    info.expected_size = 100;

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::INVALID_ARTIFACT);
}

TEST_F(InstallManagerTest, EmptyVersionRejected) {
    std::string content = "test content";
    std::string image_path = create_test_image("test.bin", content);

    ota::InstallInfo info;
    info.version = "";
    info.image_path = image_path;
    info.expected_sha256 = "abc123";
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::INVALID_ARTIFACT);
}

TEST_F(InstallManagerTest, EmptyImagePathRejected) {
    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = "";
    info.expected_sha256 = "abc123";
    info.expected_size = 100;

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::INVALID_ARTIFACT);
}

TEST_F(InstallManagerTest, ValidatePathValid) {
    EXPECT_TRUE(installer.validate_path("1.0.0"));
    EXPECT_TRUE(installer.validate_path("version-1.0.0"));
    EXPECT_TRUE(installer.validate_path("v1.0.0-release"));
}

TEST_F(InstallManagerTest, ValidatePathTraversal) {
    EXPECT_FALSE(installer.validate_path("../etc/passwd"));
    EXPECT_FALSE(installer.validate_path("../../etc/shadow"));
    EXPECT_FALSE(installer.validate_path("/etc/passwd"));
    EXPECT_FALSE(installer.validate_path("version/../../etc"));
}

TEST_F(InstallManagerTest, ValidatePathDoubleSlash) {
    EXPECT_FALSE(installer.validate_path("version//etc/passwd"));
}

TEST_F(InstallManagerTest, CheckDiskSpace) {
    EXPECT_TRUE(installer.check_disk_space(1024));
}

TEST_F(InstallManagerTest, CalculateSha256) {
    std::string content = "test content for hashing";
    std::string image_path = create_test_image("hash-test.bin", content);

    std::string sha256 = installer.calculate_sha256(image_path);

    EXPECT_FALSE(sha256.empty());
    EXPECT_EQ(sha256.length(), 64);
}

TEST_F(InstallManagerTest, CalculateSha256Nonexistent) {
    std::string sha256 = installer.calculate_sha256("/nonexistent/file.bin");
    EXPECT_TRUE(sha256.empty());
}

TEST_F(InstallManagerTest, LoadSaveInstallationState) {
    ota::InstallationState state;
    state.version = "1.0.0";
    state.status = "installed";
    state.sha256 = "abc123def456";
    state.installed_at = "1234567890";
    state.target = "/var/lib/ota/install-target/1.0.0";

    EXPECT_TRUE(installer.save_installation_state(state));

    auto loaded = installer.load_installation_state("1.0.0");
    EXPECT_EQ(loaded.version, "1.0.0");
    EXPECT_EQ(loaded.status, "installed");
    EXPECT_EQ(loaded.sha256, "abc123def456");
}

TEST_F(InstallManagerTest, LoadInstallationStateNonexistent) {
    auto state = installer.load_installation_state("nonexistent");
    EXPECT_TRUE(state.version.empty());
    EXPECT_TRUE(state.status.empty());
}

TEST_F(InstallManagerTest, CleanupStaging) {
    system(("mkdir -p " + config.staging_dir + "/1.0.0").c_str());
    std::ofstream(config.staging_dir + "/1.0.0/image.bin") << "test";

    EXPECT_TRUE(installer.cleanup_staging("1.0.0"));

    struct stat st;
    EXPECT_NE(stat((config.staging_dir + "/1.0.0").c_str(), &st), 0);
}

TEST_F(InstallManagerTest, CleanupInstallTarget) {
    system(("mkdir -p " + config.install_target + "/1.0.0").c_str());
    std::ofstream(config.install_target + "/1.0.0/image.bin") << "test";

    EXPECT_TRUE(installer.cleanup_install_target("1.0.0"));

    struct stat st;
    EXPECT_NE(stat((config.install_target + "/1.0.0").c_str(), &st), 0);
}

TEST_F(InstallManagerTest, GetDefaultConfig) {
    ota::InstallConfig default_config = installer.get_default_config();

    EXPECT_FALSE(default_config.staging_dir.empty());
    EXPECT_FALSE(default_config.install_target.empty());
    EXPECT_FALSE(default_config.state_dir.empty());
    EXPECT_GT(default_config.min_free_space_mb, 0);
}

TEST_F(InstallManagerTest, InstallProgressCallback) {
    std::string content = "test content";
    std::string image_path = create_test_image("progress.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = content.length();

    bool progress_called = false;
    auto result = installer.install(info,
        [&](int percent, const std::string& message) -> bool {
            progress_called = true;
            EXPECT_GE(percent, 0);
            EXPECT_LE(percent, 100);
            EXPECT_FALSE(message.empty());
            return true;
        });

    EXPECT_TRUE(progress_called);
    EXPECT_TRUE(result.is_success());
}

TEST_F(InstallManagerTest, InstallCancelledByUser) {
    std::string content = "test content";
    std::string image_path = create_test_image("cancel.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = content.length();

    auto result = installer.install(info,
        [](int /* percent */, const std::string& /* message */) -> bool {
            return false;
        });

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::INSTALLATION_FAILED);
}
