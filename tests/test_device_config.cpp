#include <gtest/gtest.h>
#include "device/device_config.h"
#include <fstream>
#include <cstdio>

static const std::string TEST_CONFIG_DIR = "/tmp/ota_test_config";

class DeviceConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        system(("mkdir -p " + TEST_CONFIG_DIR).c_str());
    }

    void TearDown() override {
        system(("rm -rf " + TEST_CONFIG_DIR).c_str());
    }

    void write_config(const std::string& content) {
        std::ofstream file(TEST_CONFIG_DIR + "/device.conf");
        file << content;
    }
};

TEST_F(DeviceConfigTest, ValidConfigurationLoadsSuccessfully) {
    write_config(
        "device_id=device-001\n"
        "hardware_version=revA\n"
        "software_version=1.0.0\n"
        "active_slot=A\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->device_id, "device-001");
    EXPECT_EQ(config->hardware_version, "revA");
    EXPECT_EQ(config->software_version, "1.0.0");
    EXPECT_EQ(config->active_slot, "A");
    EXPECT_TRUE(ota::validate_config(*config));
}

TEST_F(DeviceConfigTest, MissingDeviceIdFailsValidation) {
    write_config(
        "hardware_version=revA\n"
        "software_version=1.0.0\n"
        "active_slot=A\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
    EXPECT_FALSE(ota::config_error_message(*config).empty());
}

TEST_F(DeviceConfigTest, EmptyDeviceIdFailsValidation) {
    write_config(
        "device_id=\n"
        "hardware_version=revA\n"
        "software_version=1.0.0\n"
        "active_slot=A\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(DeviceConfigTest, MissingHardwareVersionFailsValidation) {
    write_config(
        "device_id=device-001\n"
        "software_version=1.0.0\n"
        "active_slot=A\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(DeviceConfigTest, InvalidSoftwareVersionFormatFailsValidation) {
    write_config(
        "device_id=device-001\n"
        "hardware_version=revA\n"
        "software_version=1.0\n"
        "active_slot=A\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(DeviceConfigTest, NonNumericSoftwareVersionFailsValidation) {
    write_config(
        "device_id=device-001\n"
        "hardware_version=revA\n"
        "software_version=abc\n"
        "active_slot=A\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(DeviceConfigTest, InvalidActiveSlotFailsValidation) {
    write_config(
        "device_id=device-001\n"
        "hardware_version=revA\n"
        "software_version=1.0.0\n"
        "active_slot=C\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(DeviceConfigTest, SlotBIsValid) {
    write_config(
        "device_id=device-001\n"
        "hardware_version=revA\n"
        "software_version=1.0.0\n"
        "active_slot=B\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(ota::validate_config(*config));
}

TEST(DeviceConfigEdgeCases, NonexistentConfigFileReturnsNullopt) {
    auto config = ota::load_config("/nonexistent/path/device.conf");
    EXPECT_FALSE(config.has_value());
}

TEST_F(DeviceConfigTest, CommentsAndWhitespaceHandledCorrectly) {
    write_config(
        "# This is a comment\n"
        "device_id=device-001\n"
        "\n"
        "  hardware_version  =  revA  \n"
        "software_version=1.0.0\n"
        "# Another comment\n"
        "active_slot=A\n"
    );

    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->hardware_version, "revA");
    EXPECT_TRUE(ota::validate_config(*config));
}
