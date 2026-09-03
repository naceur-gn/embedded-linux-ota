#include <gtest/gtest.h>
#include "client/response_parser.h"
#include "client/update_manager.h"

class FailureInjectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.device_id = "device-001";
        config.hardware_version = "revA";
        config.software_version = "1.0.0";
        config.active_slot = "A";
    }

    ota::DeviceConfig config;
};

TEST(FailureInjectionTest, ServerUnavailableReturnsError) {
    ota::DeviceConfig config;
    config.device_id = "device-001";
    config.hardware_version = "revA";
    config.software_version = "1.0.0";
    config.active_slot = "A";

    ota::UpdateManager manager;
    manager.set_server_url("http://localhost:19999");
    manager.set_device_config(config);

    auto info = manager.check_for_update();
    EXPECT_EQ(info.result, ota::UpdateCheckResult::ERROR);
    EXPECT_FALSE(info.error_message.empty());
}

TEST(FailureInjectionTest, InvalidJsonResponseRejected) {
    std::string invalid_json = "not valid json at all";
    ota::UpdateMetadata metadata;

    EXPECT_FALSE(ota::ResponseParser::parse_update_response(invalid_json, metadata));
}

TEST(FailureInjectionTest, EmptyResponseRejected) {
    ota::UpdateMetadata metadata;
    EXPECT_FALSE(ota::ResponseParser::parse_update_response("", metadata));
}

TEST(FailureInjectionTest, MissingRequiredFieldsRejected) {
    std::string json = R"({
        "update_available": true,
        "version": "1.1.0"
    })";

    ota::UpdateMetadata metadata;
    EXPECT_FALSE(ota::ResponseParser::parse_update_response(json, metadata));
}

TEST(FailureInjectionTest, InvalidVersionFormatRejected) {
    ota::UpdateMetadata metadata;
    metadata.update_available = true;
    metadata.version = "1.0";
    metadata.hardware_version = "revA";
    metadata.image_path = "/image.bin";
    metadata.sha256 = "abc";
    metadata.image_size = 100;

    std::string error = ota::ResponseParser::get_validation_error(metadata);
    EXPECT_FALSE(error.empty());
}

TEST(FailureInjectionTest, IncompatibleHardwareDetected) {
    ota::UpdateManager manager;
    EXPECT_FALSE(manager.is_compatible("revA", "revB"));
}

TEST(FailureInjectionTest, DowngradePrevented) {
    ota::UpdateManager manager;
    EXPECT_FALSE(manager.compare_versions("1.1.0", "1.0.0"));
}

TEST(FailureInjectionTest, SameVersionNoUpdate) {
    ota::UpdateManager manager;
    EXPECT_FALSE(manager.compare_versions("1.0.0", "1.0.0"));
}

TEST(FailureInjectionTest, DownloadManagerInvalidPath) {
    ota::DownloadManager manager;
    EXPECT_FALSE(manager.cleanup_download("/etc/passwd"));
}

TEST(FailureInjectionTest, DownloadManagerEmptyPath) {
    ota::DownloadManager manager;
    EXPECT_FALSE(manager.cleanup_download(""));
}
