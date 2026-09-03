#include <gtest/gtest.h>
#include "client/response_parser.h"
#include "client/update_manager.h"

class ResponseParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        valid_response = R"({
            "update_available": true,
            "version": "1.1.0",
            "hardware_version": "revA",
            "image": "/releases/1.1.0/image.bin",
            "size": 1024,
            "sha256": "abc123def456",
            "release_type": "system",
            "timestamp": "2026-09-03T12:00:00Z"
        })";

        no_update_response = R"({
            "update_available": false,
            "current_version": "1.1.0",
            "hardware_version": "revA"
        })";
    }

    std::string valid_response;
    std::string no_update_response;
};

TEST_F(ResponseParserTest, ParseValidResponse) {
    ota::UpdateMetadata metadata;
    ASSERT_TRUE(ota::ResponseParser::parse_update_response(valid_response, metadata));
    EXPECT_TRUE(metadata.update_available);
    EXPECT_EQ(metadata.version, "1.1.0");
    EXPECT_EQ(metadata.hardware_version, "revA");
    EXPECT_EQ(metadata.image_path, "/releases/1.1.0/image.bin");
    EXPECT_EQ(metadata.image_size, 1024);
    EXPECT_EQ(metadata.sha256, "abc123def456");
}

TEST_F(ResponseParserTest, ParseNoUpdateResponse) {
    ota::UpdateMetadata metadata;
    ASSERT_TRUE(ota::ResponseParser::parse_update_response(no_update_response, metadata));
    EXPECT_FALSE(metadata.update_available);
    EXPECT_EQ(metadata.current_version, "1.1.0");
}

TEST_F(ResponseParserTest, ParseEmptyResponse) {
    ota::UpdateMetadata metadata;
    EXPECT_FALSE(ota::ResponseParser::parse_update_response("", metadata));
}

TEST_F(ResponseParserTest, ParseMissingVersion) {
    std::string response = R"({
        "update_available": true,
        "hardware_version": "revA",
        "image": "/releases/1.1.0/image.bin",
        "size": 1024,
        "sha256": "abc123"
    })";
    ota::UpdateMetadata metadata;
    EXPECT_FALSE(ota::ResponseParser::parse_update_response(response, metadata));
}

TEST_F(ResponseParserTest, ParseMissingImage) {
    std::string response = R"({
        "update_available": true,
        "version": "1.1.0",
        "hardware_version": "revA",
        "size": 1024,
        "sha256": "abc123"
    })";
    ota::UpdateMetadata metadata;
    EXPECT_FALSE(ota::ResponseParser::parse_update_response(response, metadata));
}

TEST_F(ResponseParserTest, ParseMissingSha256) {
    std::string response = R"({
        "update_available": true,
        "version": "1.1.0",
        "hardware_version": "revA",
        "image": "/releases/1.1.0/image.bin",
        "size": 1024
    })";
    ota::UpdateMetadata metadata;
    EXPECT_FALSE(ota::ResponseParser::parse_update_response(response, metadata));
}

TEST_F(ResponseParserTest, ParseMissingSize) {
    std::string response = R"({
        "update_available": true,
        "version": "1.1.0",
        "hardware_version": "revA",
        "image": "/releases/1.1.0/image.bin",
        "sha256": "abc123"
    })";
    ota::UpdateMetadata metadata;
    EXPECT_FALSE(ota::ResponseParser::parse_update_response(response, metadata));
}

TEST_F(ResponseParserTest, ParseInvalidJson) {
    std::string response = "not valid json";
    ota::UpdateMetadata metadata;
    EXPECT_FALSE(ota::ResponseParser::parse_update_response(response, metadata));
}

TEST_F(ResponseParserTest, ValidateValidMetadata) {
    ota::UpdateMetadata metadata;
    metadata.update_available = true;
    metadata.version = "1.1.0";
    metadata.hardware_version = "revA";
    metadata.image_path = "/releases/1.1.0/image.bin";
    metadata.sha256 = "abc123";
    metadata.image_size = 1024;

    EXPECT_TRUE(ota::ResponseParser::validate_metadata(metadata));
}

TEST_F(ResponseParserTest, ValidateInvalidVersion) {
    ota::UpdateMetadata metadata;
    metadata.update_available = true;
    metadata.version = "1.0";
    metadata.hardware_version = "revA";
    metadata.image_path = "/releases/1.0/image.bin";
    metadata.sha256 = "abc123";
    metadata.image_size = 1024;

    EXPECT_FALSE(ota::ResponseParser::validate_metadata(metadata));
}
