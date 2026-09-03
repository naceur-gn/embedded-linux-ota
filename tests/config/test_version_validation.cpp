#include <gtest/gtest.h>
#include "device/device_config.h"
#include <fstream>
#include <cstdio>

static const std::string TEST_CONFIG_DIR = "/tmp/ota_version_test";

class VersionTest : public ::testing::Test {
protected:
    void SetUp() override {
        system(("mkdir -p " + TEST_CONFIG_DIR).c_str());
    }

    void TearDown() override {
        system(("rm -rf " + TEST_CONFIG_DIR).c_str());
    }

    void write_config(const std::string& version) {
        std::ofstream file(TEST_CONFIG_DIR + "/device.conf");
        file << "device_id=device-001\n"
             << "hardware_version=revA\n"
             << "software_version=" << version << "\n"
             << "active_slot=A\n";
    }
};

TEST_F(VersionTest, ValidVersion1_0_0) {
    write_config("1.0.0");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(ota::validate_config(*config));
}

TEST_F(VersionTest, ValidVersion1_1_0) {
    write_config("1.1.0");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(ota::validate_config(*config));
}

TEST_F(VersionTest, ValidVersion2_0_0) {
    write_config("2.0.0");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(ota::validate_config(*config));
}

TEST_F(VersionTest, ValidVersion10_25_3) {
    write_config("10.25.3");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(ota::validate_config(*config));
}

TEST_F(VersionTest, InvalidVersionSingleNumber) {
    write_config("1");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(VersionTest, InvalidVersionTwoParts) {
    write_config("1.0");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(VersionTest, InvalidVersionAlpha) {
    write_config("abc");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(VersionTest, InvalidVersionNonNumeric) {
    write_config("1.x.0");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(VersionTest, InvalidVersionFourParts) {
    write_config("1.0.0.1");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(VersionTest, InvalidVersionEmpty) {
    write_config("");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(VersionTest, ValidVersionWithLeadingZeroRejected) {
    write_config("01.0.0");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}
