#include <gtest/gtest.h>
#include "security/signature_verifier.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

class SignatureVerifierTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/ota_signature_test";
        system(("mkdir -p " + test_dir).c_str());

        // Generate test keypair
        generate_test_keypair();
    }

    void TearDown() override {
        system(("rm -rf " + test_dir).c_str());
    }

    void generate_test_keypair() {
        private_key_path = test_dir + "/test.key";
        public_key_path = test_dir + "/test.pub";

        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
        EVP_PKEY_keygen_init(ctx);
        EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1);

        EVP_PKEY* pkey = nullptr;
        EVP_PKEY_keygen(ctx, &pkey);
        EVP_PKEY_CTX_free(ctx);

        // Save private key
        FILE* priv_file = fopen(private_key_path.c_str(), "w");
        PEM_write_PrivateKey(priv_file, pkey, nullptr, nullptr, 0, nullptr, nullptr);
        fclose(priv_file);

        // Save public key
        FILE* pub_file = fopen(public_key_path.c_str(), "w");
        PEM_write_PUBKEY(pub_file, pkey);
        fclose(pub_file);

        EVP_PKEY_free(pkey);
    }

    std::string sign_data(const std::string& data, const std::string& key_path) {
        // Load private key
        FILE* fp = fopen(key_path.c_str(), "r");
        EVP_PKEY* pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
        fclose(fp);

        // Create signature
        EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
        EVP_DigestSignInit(mdctx, nullptr, EVP_sha256(), nullptr, pkey);
        EVP_DigestSignUpdate(mdctx, data.data(), data.size());

        size_t sig_len = 0;
        EVP_DigestSignFinal(mdctx, nullptr, &sig_len);

        std::string sig(sig_len, '\0');
        EVP_DigestSignFinal(mdctx, reinterpret_cast<unsigned char*>(&sig[0]), &sig_len);
        sig.resize(sig_len);

        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);

        // Convert to base64
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* bmem = BIO_new(BIO_s_mem());
        b64 = BIO_push(b64, bmem);
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(b64, sig.data(), sig.size());
        BIO_flush(b64);

        BUF_MEM* bptr;
        BIO_get_mem_ptr(b64, &bptr);
        std::string base64_sig(bptr->data, bptr->length);
        BIO_free_all(b64);

        return base64_sig;
    }

    std::string create_test_file(const std::string& name, const std::string& content) {
        std::string path = test_dir + "/" + name;
        std::ofstream file(path);
        file << content;
        file.close();
        return path;
    }

    std::string test_dir;
    std::string private_key_path;
    std::string public_key_path;
    ota::SignatureVerifier verifier;
};

TEST_F(SignatureVerifierTest, ValidSignature) {
    std::string data = "test data to sign";
    std::string sig = sign_data(data, private_key_path);

    auto result = verifier.verify_signature(data, sig, public_key_path);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_VALID);
}

TEST_F(SignatureVerifierTest, InvalidSignature) {
    std::string data = "test data to sign";
    std::string wrong_data = "different data";

    std::string sig = sign_data(data, private_key_path);

    auto result = verifier.verify_signature(wrong_data, sig, public_key_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_INVALID);
}

TEST_F(SignatureVerifierTest, MissingSignature) {
    std::string data = "test data to sign";

    auto result = verifier.verify_signature(data, "", public_key_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_MISSING);
}

TEST_F(SignatureVerifierTest, MissingPublicKey) {
    std::string data = "test data to sign";
    std::string sig = sign_data(data, private_key_path);

    auto result = verifier.verify_signature(data, sig, "/nonexistent/key.pub");
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::PUBLIC_KEY_MISSING);
}

TEST_F(SignatureVerifierTest, InvalidPublicKey) {
    std::string data = "test data to sign";
    std::string sig = sign_data(data, private_key_path);

    std::string invalid_key_path = test_dir + "/invalid.pub";
    std::ofstream(invalid_key_path) << "not a valid key";

    auto result = verifier.verify_signature(data, sig, invalid_key_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::PUBLIC_KEY_CANNOT_LOAD);
}

TEST_F(SignatureVerifierTest, EmptySignedData) {
    std::string sig = sign_data("test", private_key_path);

    auto result = verifier.verify_signature("", sig, public_key_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNED_DATA_INVALID);
}

TEST_F(SignatureVerifierTest, InvalidSignatureFormat) {
    std::string data = "test data";

    auto result = verifier.verify_signature(data, "invalid-base64!!!", public_key_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_FORMAT_INVALID);
}

TEST_F(SignatureVerifierTest, VerifyFileSignature) {
    std::string content = "file content to sign";
    std::string file_path = create_test_file("test.txt", content);

    std::string sig = sign_data(content, private_key_path);

    auto result = verifier.verify_file_signature(file_path, sig, public_key_path);
    EXPECT_TRUE(result.is_valid());
}

TEST_F(SignatureVerifierTest, LoadPublicKey) {
    std::string key = verifier.load_public_key(public_key_path);
    EXPECT_FALSE(key.empty());
    EXPECT_NE(key.find("BEGIN PUBLIC KEY"), std::string::npos);
}

TEST_F(SignatureVerifierTest, LoadPublicKeyNonexistent) {
    std::string key = verifier.load_public_key("/nonexistent/key.pub");
    EXPECT_TRUE(key.empty());
}

TEST_F(SignatureVerifierTest, CanonicalizeMetadata) {
    std::string json = R"({
  "version": "1.0.0",
  "name": "test"
})";

    // The canonicalize function just trims whitespace, not full JSON canonicalization
    std::string canonical = verifier.canonicalize_metadata(json);
    EXPECT_FALSE(canonical.empty());
    EXPECT_NE(canonical.find("version"), std::string::npos);
    EXPECT_NE(canonical.find("name"), std::string::npos);
}

TEST_F(SignatureVerifierTest, IsValidSignatureFormat) {
    EXPECT_TRUE(verifier.is_valid_signature_format("dGVzdA=="));
    EXPECT_TRUE(verifier.is_valid_signature_format("MEUCIQD2PXsxiNXfPXUcJGFIwT5Jy3sCai9jRtsYm5CzTi0VdwIgHOy1Ivdk2O0N8Y849b0cvlI+GBCtqzWcEyv3mKojgAo="));

    EXPECT_FALSE(verifier.is_valid_signature_format(""));
    EXPECT_FALSE(verifier.is_valid_signature_format("invalid!!!"));
}

TEST_F(SignatureVerifierTest, PublicKeyFingerprint) {
    std::string fingerprint = verifier.get_public_key_fingerprint(public_key_path);
    EXPECT_FALSE(fingerprint.empty());
    EXPECT_EQ(fingerprint.length(), 64);

    std::string fingerprint2 = verifier.get_public_key_fingerprint(public_key_path);
    EXPECT_EQ(fingerprint, fingerprint2);
}

TEST_F(SignatureVerifierTest, DifferentKeyRejected) {
    // Generate another keypair
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    EVP_PKEY_keygen_init(ctx);
    EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1);

    EVP_PKEY* other_key = nullptr;
    EVP_PKEY_keygen(ctx, &other_key);
    EVP_PKEY_CTX_free(ctx);

    std::string other_pub_path = test_dir + "/other.pub";
    FILE* pub_file = fopen(other_pub_path.c_str(), "w");
    PEM_write_PUBKEY(pub_file, other_key);
    fclose(pub_file);
    EVP_PKEY_free(other_key);

    std::string data = "test data";
    std::string sig = sign_data(data, private_key_path);

    auto result = verifier.verify_signature(data, sig, other_pub_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_INVALID);
}
