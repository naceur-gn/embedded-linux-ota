#include <gtest/gtest.h>
#include "installation/installer.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

class InstallAttackTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/ota_install_attack_test";
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

TEST_F(InstallAttackTest, Attack1PathTraversalAbsolute) {
    std::string content = "malicious content";
    std::string image_path = create_test_image("malicious.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "/etc/passwd";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::PATH_TRAVERSAL_DETECTED);
}

TEST_F(InstallAttackTest, Attack2PathTraversalRelative) {
    std::string content = "malicious content";
    std::string image_path = create_test_image("malicious.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "../../etc/shadow";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::PATH_TRAVERSAL_DETECTED);
}

TEST_F(InstallAttackTest, Attack3PathTraversalMixed) {
    std::string content = "malicious content";
    std::string image_path = create_test_image("malicious.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "valid/../../etc/passwd";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::PATH_TRAVERSAL_DETECTED);
}

TEST_F(InstallAttackTest, Attack4HashMismatch) {
    std::string content = "original content";
    std::string image_path = create_test_image("original.bin", content);

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = image_path;
    info.expected_sha256 = "0000000000000000000000000000000000000000000000000000000000000000";
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::VERIFICATION_FAILED);
}

TEST_F(InstallAttackTest, Attack5SizeMismatch) {
    std::string content = "original content";
    std::string image_path = create_test_image("sizetest.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = 999999;

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::VERIFICATION_FAILED);
}

TEST_F(InstallAttackTest, Attack6SymlinkAttack) {
    std::string symlink_path = test_dir + "/malicious-link";
    symlink("/etc/passwd", symlink_path.c_str());

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = symlink_path;
    info.expected_sha256 = "abc123";
    info.expected_size = 100;

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
}

TEST_F(InstallAttackTest, Attack7DirectoryAsImage) {
    std::string dir_path = test_dir + "/directory-image";
    mkdir(dir_path.c_str(), 0755);

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = dir_path;
    info.expected_sha256 = "abc123";
    info.expected_size = 100;

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::INVALID_ARTIFACT);
}

TEST_F(InstallAttackTest, Attack8ExistingTarget) {
    std::string content = "existing content";
    std::string existing_dir = config.install_target + "/1.0.0";
    system(("mkdir -p " + existing_dir).c_str());
    std::ofstream(existing_dir + "/image.bin") << content;

    std::string new_content = "new content";
    std::string image_path = create_test_image("new.bin", new_content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = new_content.length();

    auto result = installer.install(info);

    EXPECT_TRUE(result.is_success());
}

TEST_F(InstallAttackTest, Attack9PartialInstallationCleanup) {
    std::string content = "test content";
    std::string image_path = create_test_image("partial.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "1.0.0";
    info.image_path = image_path;
    info.expected_sha256 = "0000000000000000000000000000000000000000000000000000000000000000";
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());

    struct stat st;
    EXPECT_NE(stat((config.staging_dir + "/1.0.0").c_str(), &st), 0);
}

TEST_F(InstallAttackTest, Attack10DoubleSlashInVersion) {
    std::string content = "test content";
    std::string image_path = create_test_image("double.bin", content);
    std::string sha256 = calculate_sha256(image_path);

    ota::InstallInfo info;
    info.version = "1.0.0//../../etc/passwd";
    info.image_path = image_path;
    info.expected_sha256 = sha256;
    info.expected_size = content.length();

    auto result = installer.install(info);

    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(result.status, ota::InstallStatus::PATH_TRAVERSAL_DETECTED);
}
