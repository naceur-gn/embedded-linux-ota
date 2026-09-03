#include <gtest/gtest.h>
#include "validation/integrity_validator.h"
#include <fstream>
#include <cstdio>
#include <cstring>
#include <algorithm>

class IntegrityValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/ota_integrity_test";
        system(("mkdir -p " + test_dir).c_str());
    }

    void TearDown() override {
        system(("rm -rf " + test_dir).c_str());
    }

    std::string create_test_file(const std::string& name, const std::string& content) {
        std::string path = test_dir + "/" + name;
        std::ofstream file(path, std::ios::binary);
        file.write(content.c_str(), content.length());
        file.close();
        return path;
    }

    std::string create_binary_file(const std::string& name, size_t size) {
        std::string path = test_dir + "/" + name;
        std::ofstream file(path, std::ios::binary);
        for (size_t i = 0; i < size; ++i) {
            char c = static_cast<char>(i % 256);
            file.write(&c, 1);
        }
        file.close();
        return path;
    }

    std::string test_dir;
    ota::IntegrityValidator validator;
};

TEST_F(IntegrityValidatorTest, CalculateSha256EmptyFile) {
    std::string path = create_test_file("empty.bin", "");
    std::string sha256 = validator.calculate_sha256(path);

    EXPECT_FALSE(sha256.empty());
    EXPECT_EQ(sha256.length(), 64);

    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_EQ(sha256, expected);
}

TEST_F(IntegrityValidatorTest, CalculateSha256TextFile) {
    std::string path = create_test_file("hello.txt", "hello world");
    std::string sha256 = validator.calculate_sha256(path);

    EXPECT_FALSE(sha256.empty());
    EXPECT_EQ(sha256.length(), 64);

    std::string expected = "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9";
    EXPECT_EQ(sha256, expected);
}

TEST_F(IntegrityValidatorTest, CalculateSha256BinaryFile) {
    std::string path = create_binary_file("binary.bin", 1024);
    std::string sha256 = validator.calculate_sha256(path);

    EXPECT_FALSE(sha256.empty());
    EXPECT_EQ(sha256.length(), 64);
}

TEST_F(IntegrityValidatorTest, CalculateSha256NonexistentFile) {
    std::string sha256 = validator.calculate_sha256("/nonexistent/file.bin");
    EXPECT_TRUE(sha256.empty());
}

TEST_F(IntegrityValidatorTest, IsValidSha256Format) {
    EXPECT_TRUE(validator.is_valid_sha256_format(
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));

    EXPECT_TRUE(validator.is_valid_sha256_format(
        "ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));

    EXPECT_FALSE(validator.is_valid_sha256_format(""));
    EXPECT_FALSE(validator.is_valid_sha256_format("abc"));
    EXPECT_FALSE(validator.is_valid_sha256_format("xyz0123456789abcdef0123456789abcdef0123456789abcdef0123456789"));
    EXPECT_FALSE(validator.is_valid_sha256_format("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b85"));
    EXPECT_FALSE(validator.is_valid_sha256_format("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b8550"));
}

TEST_F(IntegrityValidatorTest, NormalizeHash) {
    EXPECT_EQ(validator.normalize_hash("ABC123"), "abc123");
    EXPECT_EQ(validator.normalize_hash("AbC123"), "abc123");
    EXPECT_EQ(validator.normalize_hash("abc 123"), "abc123");
    EXPECT_EQ(validator.normalize_hash("abc\n123"), "abc123");
    EXPECT_EQ(validator.normalize_hash("ABC\t123"), "abc123");
}

TEST_F(IntegrityValidatorTest, ValidateFileValid) {
    std::string path = create_test_file("valid.bin", "test data");
    std::string sha256 = validator.calculate_sha256(path);

    auto result = validator.validate_file(path, sha256);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::VALID);
}

TEST_F(IntegrityValidatorTest, ValidateFileHashMismatch) {
    std::string path = create_test_file("mismatch.bin", "test data");
    std::string wrong_hash = "0000000000000000000000000000000000000000000000000000000000000000";

    auto result = validator.validate_file(path, wrong_hash);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::HASH_MISMATCH);
}

TEST_F(IntegrityValidatorTest, ValidateFileNotFound) {
    auto result = validator.validate_file("/nonexistent/file.bin",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::FILE_NOT_FOUND);
}

TEST_F(IntegrityValidatorTest, ValidateFileInvalidHashFormat) {
    std::string path = create_test_file("invalid_hash.bin", "test data");

    auto result = validator.validate_file(path, "invalid");
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::INVALID_HASH_FORMAT);
}

TEST_F(IntegrityValidatorTest, ValidateFileSizeMismatch) {
    std::string path = create_test_file("size.bin", "test data");
    std::string sha256 = validator.calculate_sha256(path);

    auto result = validator.validate_file(path, sha256, 999);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::SIZE_MISMATCH);
}

TEST_F(IntegrityValidatorTest, ValidateFileSizeMatch) {
    std::string path = create_test_file("size_ok.bin", "test data");
    std::string sha256 = validator.calculate_sha256(path);
    int64_t size = 9;

    auto result = validator.validate_file(path, sha256, size);
    EXPECT_TRUE(result.is_valid());
}

TEST_F(IntegrityValidatorTest, VerifySha256) {
    std::string path = create_test_file("verify.bin", "verify me");
    std::string sha256 = validator.calculate_sha256(path);

    EXPECT_TRUE(validator.verify_sha256(path, sha256));
    EXPECT_FALSE(validator.verify_sha256(path, "wrong_hash"));
}

TEST_F(IntegrityValidatorTest, ValidateDirectory) {
    std::string dir_path = test_dir + "/subdir";
    system(("mkdir -p " + dir_path).c_str());

    auto result = validator.validate_file(dir_path,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::ValidationStatus::FILE_NOT_REGULAR);
}

TEST_F(IntegrityValidatorTest, ValidateLargeFile) {
    std::string path = create_binary_file("large.bin", 1024 * 1024);
    std::string sha256 = validator.calculate_sha256(path);

    EXPECT_FALSE(sha256.empty());
    EXPECT_EQ(sha256.length(), 64);

    auto result = validator.validate_file(path, sha256);
    EXPECT_TRUE(result.is_valid());
}

TEST_F(IntegrityValidatorTest, ValidationStatusToString) {
    EXPECT_EQ(validation_status_to_string(ota::ValidationStatus::VALID), "VALID");
    EXPECT_EQ(validation_status_to_string(ota::ValidationStatus::FILE_NOT_FOUND), "FILE_NOT_FOUND");
    EXPECT_EQ(validation_status_to_string(ota::ValidationStatus::HASH_MISMATCH), "HASH_MISMATCH");
    EXPECT_EQ(validation_status_to_string(ota::ValidationStatus::SIZE_MISMATCH), "SIZE_MISMATCH");
}

TEST_F(IntegrityValidatorTest, ValidateHashCaseInsensitive) {
    std::string path = create_test_file("case.bin", "test data");
    std::string sha256 = validator.calculate_sha256(path);

    std::string upper_hash = sha256;
    std::transform(upper_hash.begin(), upper_hash.end(), upper_hash.begin(), ::toupper);

    auto result = validator.validate_file(path, upper_hash);
    EXPECT_TRUE(result.is_valid());
}

TEST_F(IntegrityValidatorTest, ValidateHashWithWhitespace) {
    std::string path = create_test_file("space.bin", "test data");
    std::string sha256 = validator.calculate_sha256(path);

    std::string spaced_hash = sha256.substr(0, 8) + " " + sha256.substr(8, 8) + " " + sha256.substr(16);

    auto result = validator.validate_file(path, spaced_hash);
    EXPECT_TRUE(result.is_valid());
}
