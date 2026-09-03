#include "security/signature_verifier.h"
#include "logging/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace ota {

std::string signature_status_to_string(SignatureStatus status) {
    switch (status) {
        case SignatureStatus::SIGNATURE_VALID:
            return "SIGNATURE_VALID";
        case SignatureStatus::SIGNATURE_INVALID:
            return "SIGNATURE_INVALID";
        case SignatureStatus::SIGNATURE_MISSING:
            return "SIGNATURE_MISSING";
        case SignatureStatus::SIGNATURE_FORMAT_INVALID:
            return "SIGNATURE_FORMAT_INVALID";
        case SignatureStatus::PUBLIC_KEY_MISSING:
            return "PUBLIC_KEY_MISSING";
        case SignatureStatus::PUBLIC_KEY_INVALID:
            return "PUBLIC_KEY_INVALID";
        case SignatureStatus::PUBLIC_KEY_CANNOT_LOAD:
            return "PUBLIC_KEY_CANNOT_LOAD";
        case SignatureStatus::SIGNED_DATA_INVALID:
            return "SIGNED_DATA_INVALID";
        case SignatureStatus::VERIFICATION_ERROR:
            return "VERIFICATION_ERROR";
        case SignatureStatus::CRYPTO_ERROR:
            return "CRYPTO_ERROR";
        default:
            return "UNKNOWN";
    }
}

SignatureVerifier::SignatureVerifier() {
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
}

SignatureVerifier::~SignatureVerifier() {
    EVP_cleanup();
    ERR_free_strings();
}

SignatureResult SignatureVerifier::verify_signature(
    const std::string& signed_data,
    const std::string& signature_base64,
    const std::string& public_key_path
) {
    auto& logger = Logger::instance();
    SignatureResult result;

    logger.info("signature-verifier", "Starting signature verification");

    if (signed_data.empty()) {
        result.status = SignatureStatus::SIGNED_DATA_INVALID;
        result.error_message = "Signed data is empty";
        logger.error("signature-verifier", result.error_message);
        return result;
    }

    if (signature_base64.empty()) {
        result.status = SignatureStatus::SIGNATURE_MISSING;
        result.error_message = "Signature is missing";
        logger.error("signature-verifier", result.error_message);
        return result;
    }

    if (!is_valid_signature_format(signature_base64)) {
        result.status = SignatureStatus::SIGNATURE_FORMAT_INVALID;
        result.error_message = "Invalid signature format (not valid base64)";
        logger.error("signature-verifier", result.error_message);
        return result;
    }

    std::ifstream key_file(public_key_path);
    if (!key_file.good()) {
        result.status = SignatureStatus::PUBLIC_KEY_MISSING;
        result.error_message = "Public key not found: " + public_key_path;
        logger.error("signature-verifier", result.error_message);
        return result;
    }
    key_file.close();

    EVP_PKEY* public_key = load_pem_public_key(public_key_path);
    if (!public_key) {
        result.status = SignatureStatus::PUBLIC_KEY_CANNOT_LOAD;
        result.error_message = "Cannot load public key: " + public_key_path;
        logger.error("signature-verifier", result.error_message);
        return result;
    }

    std::string sig_der = base64_decode(signature_base64);
    if (sig_der.empty()) {
        result.status = SignatureStatus::CRYPTO_ERROR;
        result.error_message = "Failed to decode signature";
        EVP_PKEY_free(public_key);
        logger.error("signature-verifier", result.error_message);
        return result;
    }

    result.signed_data_hash = calculate_sha256(signed_data);

    bool valid = verify_ecdsa_signature(
        public_key,
        reinterpret_cast<const unsigned char*>(signed_data.data()),
        signed_data.size(),
        reinterpret_cast<const unsigned char*>(sig_der.data()),
        sig_der.size()
    );

    EVP_PKEY_free(public_key);

    if (valid) {
        result.status = SignatureStatus::SIGNATURE_VALID;
        result.error_message = "";
        logger.info("signature-verifier", "Signature verification successful");
    } else {
        result.status = SignatureStatus::SIGNATURE_INVALID;
        result.error_message = "Signature verification failed";
        logger.error("signature-verifier", result.error_message);
    }

    return result;
}

SignatureResult SignatureVerifier::verify_file_signature(
    const std::string& file_path,
    const std::string& signature_base64,
    const std::string& public_key_path
) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.good()) {
        SignatureResult result;
        result.status = SignatureStatus::SIGNED_DATA_INVALID;
        result.error_message = "Cannot open file: " + file_path;
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return verify_signature(content, signature_base64, public_key_path);
}

std::string SignatureVerifier::load_public_key(const std::string& public_key_path) {
    std::ifstream file(public_key_path);
    if (!file.good()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string SignatureVerifier::canonicalize_metadata(const std::string& metadata_json) {
    std::istringstream stream(metadata_json);
    std::string line;
    std::string result;

    while (std::getline(stream, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
        trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

        if (!trimmed.empty()) {
            if (!result.empty()) {
                result += "\n";
            }
            result += trimmed;
        }
    }

    return result;
}

bool SignatureVerifier::is_valid_signature_format(const std::string& signature_base64) {
    if (signature_base64.empty()) {
        return false;
    }

    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

    for (char c : signature_base64) {
        if (chars.find(c) == std::string::npos) {
            return false;
        }
    }

    size_t padding = 0;
    if (signature_base64.size() >= 2) {
        if (signature_base64[signature_base64.size() - 1] == '=') {
            padding++;
            if (signature_base64.size() >= 3 &&
                signature_base64[signature_base64.size() - 2] == '=') {
                padding++;
            }
        }
    }

    size_t len = signature_base64.size() - padding;
    return len > 0;
}

std::string SignatureVerifier::get_public_key_fingerprint(const std::string& public_key_path) {
    EVP_PKEY* key = load_pem_public_key(public_key_path);
    if (!key) {
        return "";
    }

    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, key);

    char* data;
    long len = BIO_get_mem_data(bio, &data);
    std::string key_pem(data, len);

    BIO_free(bio);
    EVP_PKEY_free(key);

    return calculate_sha256(key_pem);
}

EVP_PKEY* SignatureVerifier::load_pem_public_key(const std::string& public_key_path) {
    FILE* fp = fopen(public_key_path.c_str(), "r");
    if (!fp) {
        return nullptr;
    }

    EVP_PKEY* pkey = PEM_read_PUBKEY(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    return pkey;
}

std::string SignatureVerifier::calculate_sha256(const std::string& data) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }

    unsigned char hash[SHA256_DIGEST_SIZE];
    unsigned int hash_len = 0;

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    EVP_MD_CTX_free(ctx);

    std::string result;
    result.reserve(hash_len * 2);
    for (unsigned int i = 0; i < hash_len; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        result += buf;
    }

    return result;
}

std::string SignatureVerifier::base64_decode(const std::string& encoded) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    bmem = BIO_push(b64, bmem);

    BIO_set_flags(bmem, BIO_FLAGS_BASE64_NO_NL);

    std::string decoded;
    decoded.resize(encoded.size());

    int len = BIO_read(bmem, &decoded.data()[0], static_cast<int>(decoded.size()));
    BIO_free_all(bmem);

    if (len <= 0) {
        return "";
    }

    decoded.resize(len);
    return decoded;
}

bool SignatureVerifier::verify_ecdsa_signature(
    EVP_PKEY* public_key,
    const unsigned char* data,
    size_t data_len,
    const unsigned char* sig,
    size_t sig_len
) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        return false;
    }

    bool result = false;

    if (EVP_DigestVerifyInit(mdctx, nullptr, EVP_sha256(), nullptr, public_key) == 1 &&
        EVP_DigestVerifyUpdate(mdctx, data, data_len) == 1) {
        int ret = EVP_DigestVerifyFinal(mdctx, sig, sig_len);
        result = (ret == 1);
    }

    EVP_MD_CTX_free(mdctx);
    return result;
}

std::string SignatureVerifier::get_openssl_error() {
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return "";
    }

    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}

}
