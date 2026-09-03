#include <gtest/gtest.h>
#include "validation/integrity_validator.h"
#include <fstream>
#include <cstdio>
#include <cstring>

class AttackSimulationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/ota_attack_test";
        system(("mkdir -p " + test_dir).c_str());
    }

    void TearDown() override {
        system(("rm -rf " + test_dir).c_str());
    }

    std::string create_original_image() {
        std::string path = test_dir + "/original.bin";
        std::ofstream file(path, std::ios::binary);
        for (int i = 0; i < 1000; ++i) {
            char c = static_cast<char>(i % 256);
            file.write(&c, 1);
        }
        file.close();
        return path;
    }

    std::string test_dir;
    ota::IntegrityValidator validator;
};

TEST_F(AttackSimulationTest, Attack1ModifiedImage) {
    std::string original_path = create_original_image();
    std::string original_hash = validator.calculate_sha256(original_path);

    std::string modified_path = test_dir + "/modified.bin";
    std::ifstream orig(original_path, std::ios::binary);
    std::ofstream mod(modified_path, std::ios::binary);

    char buffer[1024];
    orig.read(buffer, sizeof(buffer));
    mod.write(buffer, orig.gcount());

    mod.seekp(100);
    char modified_byte = static_cast<char>(0x7F);
    mod.write(&modified_byte, 1);

    orig.close();
    mod.close();

    auto result = validator.validate_file(modified_path, original_hash);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::HASH_MISMATCH);
}

TEST_F(AttackSimulationTest, Attack2WrongMetadataHash) {
    std::string image_path = create_original_image();
    std::string wrong_hash = "0000000000000000000000000000000000000000000000000000000000000000";

    auto result = validator.validate_file(image_path, wrong_hash);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::HASH_MISMATCH);
}

TEST_F(AttackSimulationTest, Attack3MalformedHash) {
    std::string image_path = create_original_image();

    EXPECT_FALSE(validator.validate_file(image_path, "").is_valid());
    EXPECT_FALSE(validator.validate_file(image_path, "abc").is_valid());
    EXPECT_FALSE(validator.validate_file(image_path, "not_a_hash").is_valid());
    EXPECT_FALSE(validator.validate_file(image_path, "xyz0123456789abcdef0123456789abcdef0123456789abcdef0123456789").is_valid());
}

TEST_F(AttackSimulationTest, Attack4TruncatedImage) {
    std::string original_path = create_original_image();
    std::string original_hash = validator.calculate_sha256(original_path);
    int64_t original_size = 1000;

    std::string truncated_path = test_dir + "/truncated.bin";
    std::ifstream orig(original_path, std::ios::binary);
    std::ofstream trunc(truncated_path, std::ios::binary);

    char buffer[512];
    orig.read(buffer, sizeof(buffer));
    trunc.write(buffer, orig.gcount());

    orig.close();
    trunc.close();

    auto result = validator.validate_file(truncated_path, original_hash, original_size);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::SIZE_MISMATCH);
}

TEST_F(AttackSimulationTest, Attack5EmptyImage) {
    std::string empty_path = test_dir + "/empty.bin";
    std::ofstream file(empty_path);
    file.close();

    std::string hash = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    auto result = validator.validate_file(empty_path, hash);
    EXPECT_TRUE(result.is_valid());
}

TEST_F(AttackSimulationTest, Attack6ExtraBytes) {
    std::string original_path = create_original_image();
    std::string original_hash = validator.calculate_sha256(original_path);
    int64_t original_size = 1000;

    std::string extended_path = test_dir + "/extended.bin";
    std::ifstream orig(original_path, std::ios::binary);
    std::ofstream ext(extended_path, std::ios::binary);

    char buffer[1024];
    while (orig.read(buffer, sizeof(buffer))) {
        ext.write(buffer, orig.gcount());
    }
    ext.write(buffer, orig.gcount());

    char extra = 0x42;
    ext.write(&extra, 1);

    orig.close();
    ext.close();

    auto result = validator.validate_file(extended_path, original_hash, original_size);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::SIZE_MISMATCH);
}

TEST_F(AttackSimulationTest, Attack7HashReplayAttack) {
    std::string image1_path = test_dir + "/image1.bin";
    std::string image2_path = test_dir + "/image2.bin";

    {
        std::ofstream f1(image1_path, std::ios::binary);
        for (int i = 0; i < 100; ++i) {
            char c = 'A';
            f1.write(&c, 1);
        }
        f1.close();
    }

    {
        std::ofstream f2(image2_path, std::ios::binary);
        for (int i = 0; i < 100; ++i) {
            char c = 'B';
            f2.write(&c, 1);
        }
        f2.close();
    }

    std::string hash1 = validator.calculate_sha256(image1_path);

    auto result = validator.validate_file(image2_path, hash1);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::HASH_MISMATCH);
}
