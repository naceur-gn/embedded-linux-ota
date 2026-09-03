#include <gtest/gtest.h>
#include "device/device_config.h"
#include <fstream>
#include <cstdio>

static const std::string TEST_CONFIG_DIR = "/tmp/ota_slot_test";

class SlotTest : public ::testing::Test {
protected:
    void SetUp() override {
        system(("mkdir -p " + TEST_CONFIG_DIR).c_str());
    }

    void TearDown() override {
        system(("rm -rf " + TEST_CONFIG_DIR).c_str());
    }

    void write_config(const std::string& slot) {
        std::ofstream file(TEST_CONFIG_DIR + "/device.conf");
        file << "device_id=device-001\n"
             << "hardware_version=revA\n"
             << "software_version=1.0.0\n"
             << "active_slot=" << slot << "\n";
    }
};

TEST_F(SlotTest, SlotAAccepted) {
    write_config("A");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(ota::validate_config(*config));
    EXPECT_EQ(config->active_slot, "A");
}

TEST_F(SlotTest, SlotBAccepted) {
    write_config("B");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(ota::validate_config(*config));
    EXPECT_EQ(config->active_slot, "B");
}

TEST_F(SlotTest, SlotCRejected) {
    write_config("C");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(SlotTest, SlotEmptyRejected) {
    write_config("");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(SlotTest, SlotLowerCaseRejected) {
    write_config("a");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(SlotTest, SlotLowerCaseBRejected) {
    write_config("b");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}

TEST_F(SlotTest, SlotNumericRejected) {
    write_config("1");
    auto config = ota::load_config(TEST_CONFIG_DIR + "/device.conf");
    ASSERT_TRUE(config.has_value());
    EXPECT_FALSE(ota::validate_config(*config));
}
