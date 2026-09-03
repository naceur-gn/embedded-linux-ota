#include "validation/integrity_validator.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>

namespace ota {

std::string validation_status_to_string(ValidationStatus status) {
    switch (status) {
        case ValidationStatus::VALID:                return "VALID";
        case ValidationStatus::FILE_NOT_FOUND:       return "FILE_NOT_FOUND";
        case ValidationStatus::FILE_NOT_READABLE:    return "FILE_NOT_READABLE";
        case ValidationStatus::FILE_NOT_REGULAR:     return "FILE_NOT_REGULAR";
        case ValidationStatus::INVALID_FILE_TYPE:    return "INVALID_FILE_TYPE";
        case ValidationStatus::SIZE_MISMATCH:        return "SIZE_MISMATCH";
        case ValidationStatus::INVALID_HASH_FORMAT:  return "INVALID_HASH_FORMAT";
        case ValidationStatus::HASH_MISMATCH:        return "HASH_MISMATCH";
        case ValidationStatus::HASH_CALCULATION_ERROR: return "HASH_CALCULATION_ERROR";
        case ValidationStatus::METADATA_ERROR:       return "METADATA_ERROR";
        default:                                     return "UNKNOWN";
    }
}

IntegrityValidator::IntegrityValidator() {
}

ValidationResult IntegrityValidator::validate_file(const std::string& file_path,
                                                   const std::string& expected_hash,
                                                   int64_t expected_size) {
    ValidationResult result;
    result.expected_hash = expected_hash;
    result.expected_size = expected_size;

    if (!file_exists(file_path)) {
        result.status = ValidationStatus::FILE_NOT_FOUND;
        result.error_message = "File not found: " + file_path;
        return result;
    }

    if (!is_regular_file(file_path)) {
        result.status = ValidationStatus::FILE_NOT_REGULAR;
        result.error_message = "Not a regular file: " + file_path;
        return result;
    }

    if (!is_readable(file_path)) {
        result.status = ValidationStatus::FILE_NOT_READABLE;
        result.error_message = "File not readable: " + file_path;
        return result;
    }

    result.actual_size = get_file_size(file_path);

    if (expected_size >= 0 && result.actual_size != expected_size) {
        result.status = ValidationStatus::SIZE_MISMATCH;
        result.error_message = "Size mismatch: expected " + std::to_string(expected_size) +
                              " got " + std::to_string(result.actual_size);
        return result;
    }

    std::string normalized_hash = normalize_hash(expected_hash);
    if (!is_valid_sha256_format(normalized_hash)) {
        result.status = ValidationStatus::INVALID_HASH_FORMAT;
        result.error_message = "Invalid SHA-256 format: " + expected_hash;
        return result;
    }

    result.calculated_hash = calculate_sha256_incremental(file_path);
    if (result.calculated_hash.empty()) {
        result.status = ValidationStatus::HASH_CALCULATION_ERROR;
        result.error_message = "Failed to calculate SHA-256";
        return result;
    }

    if (result.calculated_hash != normalized_hash) {
        result.status = ValidationStatus::HASH_MISMATCH;
        result.error_message = "SHA-256 mismatch: expected " + normalized_hash +
                              " got " + result.calculated_hash;
        return result;
    }

    result.status = ValidationStatus::VALID;
    result.error_message.clear();
    return result;
}

ValidationResult IntegrityValidator::validate_image(const std::string& image_path,
                                                    const std::string& expected_sha256,
                                                    int64_t expected_size) {
    return validate_file(image_path, expected_sha256, expected_size);
}

std::string IntegrityValidator::calculate_sha256(const std::string& file_path) {
    return calculate_sha256_incremental(file_path);
}

bool IntegrityValidator::is_valid_sha256_format(const std::string& hash) {
    if (hash.length() != SHA256_HEX_LENGTH) {
        return false;
    }

    for (char c : hash) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

bool IntegrityValidator::verify_sha256(const std::string& file_path, const std::string& expected_hash) {
    ValidationResult result = validate_file(file_path, expected_hash);
    return result.is_valid();
}

std::string IntegrityValidator::normalize_hash(const std::string& hash) {
    std::string normalized;
    normalized.reserve(hash.length());

    for (char c : hash) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            normalized += std::tolower(static_cast<unsigned char>(c));
        }
    }

    return normalized;
}

bool IntegrityValidator::file_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool IntegrityValidator::is_regular_file(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool IntegrityValidator::is_readable(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return (st.st_mode & S_IRUSR) != 0;
}

int64_t IntegrityValidator::get_file_size(const std::string& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return -1;
    }
    return static_cast<int64_t>(size);
}

std::string IntegrityValidator::calculate_sha256_incremental(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return "";
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    char buffer[READ_BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer))) {
        if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
            EVP_MD_CTX_free(mdctx);
            return "";
        }
    }
    if (file.gcount() > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, file.gcount()) != 1) {
            EVP_MD_CTX_free(mdctx);
            return "";
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return "";
    }

    EVP_MD_CTX_free(mdctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
}

}
