#pragma once

#include <string>
#include <cstdint>
#include <openssl/evp.h>

namespace ota {

enum class ValidationStatus {
    VALID,
    FILE_NOT_FOUND,
    FILE_NOT_READABLE,
    FILE_NOT_REGULAR,
    INVALID_FILE_TYPE,
    SIZE_MISMATCH,
    INVALID_HASH_FORMAT,
    HASH_MISMATCH,
    HASH_CALCULATION_ERROR,
    METADATA_ERROR
};

std::string validation_status_to_string(ValidationStatus status);

struct ValidationResult {
    ValidationStatus status;
    std::string expected_hash;
    std::string calculated_hash;
    int64_t expected_size;
    int64_t actual_size;
    std::string error_message;

    bool is_valid() const { return status == ValidationStatus::VALID; }
};

class IntegrityValidator {
public:
    IntegrityValidator();

    ValidationResult validate_file(const std::string& file_path,
                                   const std::string& expected_hash,
                                   int64_t expected_size = -1);

    ValidationResult validate_image(const std::string& image_path,
                                    const std::string& expected_sha256,
                                    int64_t expected_size);

    std::string calculate_sha256(const std::string& file_path);

    bool is_valid_sha256_format(const std::string& hash);

    bool verify_sha256(const std::string& file_path, const std::string& expected_hash);

    std::string normalize_hash(const std::string& hash);

private:
    bool file_exists(const std::string& path);

    bool is_regular_file(const std::string& path);

    bool is_readable(const std::string& path);

    int64_t get_file_size(const std::string& path);

    std::string calculate_sha256_incremental(const std::string& file_path);

    static const int SHA256_HEX_LENGTH = 64;
    static const int READ_BUFFER_SIZE = 8192;
};

}
