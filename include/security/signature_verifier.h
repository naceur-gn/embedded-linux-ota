#pragma once

#include <string>
#include <cstdint>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

namespace ota {

enum class SignatureStatus {
    SIGNATURE_VALID,
    SIGNATURE_INVALID,
    SIGNATURE_MISSING,
    SIGNATURE_FORMAT_INVALID,
    PUBLIC_KEY_MISSING,
    PUBLIC_KEY_INVALID,
    PUBLIC_KEY_CANNOT_LOAD,
    SIGNED_DATA_INVALID,
    VERIFICATION_ERROR,
    CRYPTO_ERROR
};

std::string signature_status_to_string(SignatureStatus status);

struct SignatureResult {
    SignatureStatus status;
    std::string error_message;
    std::string signed_data_hash;
    std::string public_key_info;

    bool is_valid() const { return status == SignatureStatus::SIGNATURE_VALID; }
};

class SignatureVerifier {
public:
    SignatureVerifier();
    ~SignatureVerifier();

    SignatureResult verify_signature(
        const std::string& signed_data,
        const std::string& signature_base64,
        const std::string& public_key_path
    );

    SignatureResult verify_file_signature(
        const std::string& file_path,
        const std::string& signature_base64,
        const std::string& public_key_path
    );

    std::string load_public_key(const std::string& public_key_path);

    std::string canonicalize_metadata(const std::string& metadata_json);

    bool is_valid_signature_format(const std::string& signature_base64);

    std::string get_public_key_fingerprint(const std::string& public_key_path);

private:
    EVP_PKEY* load_pem_public_key(const std::string& public_key_path);

    std::string calculate_sha256(const std::string& data);

    std::string base64_decode(const std::string& encoded);

    bool verify_ecdsa_signature(
        EVP_PKEY* public_key,
        const unsigned char* data,
        size_t data_len,
        const unsigned char* sig,
        size_t sig_len
    );

    std::string get_openssl_error();

    static const int SHA256_DIGEST_SIZE = 32;
};

}
