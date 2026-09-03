#include <gtest/gtest.h>
#include "client/update_manager.h"
#include "device/device_config.h"

class UpdateManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.device_id = "device-001";
        config.hardware_version = "revA";
        config.software_version = "1.0.0";
        config.active_slot = "A";

        manager.set_device_config(config);
    }

    ota::DeviceConfig config;
    ota::UpdateManager manager;
};

TEST_F(UpdateManagerTest, CompareVersionsNewer) {
    EXPECT_TRUE(manager.compare_versions("1.0.0", "1.1.0"));
    EXPECT_TRUE(manager.compare_versions("1.0.0", "2.0.0"));
    EXPECT_TRUE(manager.compare_versions("1.9.9", "2.0.0"));
}

TEST_F(UpdateManagerTest, CompareVersionsSame) {
    EXPECT_FALSE(manager.compare_versions("1.0.0", "1.0.0"));
    EXPECT_FALSE(manager.compare_versions("1.1.0", "1.1.0"));
}

TEST_F(UpdateManagerTest, CompareVersionsOlder) {
    EXPECT_FALSE(manager.compare_versions("1.1.0", "1.0.0"));
    EXPECT_FALSE(manager.compare_versions("2.0.0", "1.0.0"));
}

TEST_F(UpdateManagerTest, IsCompatibleSameHardware) {
    EXPECT_TRUE(manager.is_compatible("revA", "revA"));
    EXPECT_TRUE(manager.is_compatible("revB", "revB"));
}

TEST_F(UpdateManagerTest, IsCompatibleDifferentHardware) {
    EXPECT_FALSE(manager.is_compatible("revA", "revB"));
    EXPECT_FALSE(manager.is_compatible("revB", "revA"));
}

TEST_F(UpdateManagerTest, GetCurrentVersion) {
    EXPECT_EQ(manager.get_current_version(), "1.0.0");
}

TEST_F(UpdateManagerTest, GetDeviceId) {
    EXPECT_EQ(manager.get_device_id(), "device-001");
}

TEST_F(UpdateManagerTest, GetHardwareVersion) {
    EXPECT_EQ(manager.get_hardware_version(), "revA");
}

TEST_F(UpdateManagerTest, SetServerUrl) {
    manager.set_server_url("http://example.com:8080");
}

TEST_F(UpdateManagerTest, SetDownloadDir) {
    manager.set_download_dir("/tmp/test-downloads");
}
