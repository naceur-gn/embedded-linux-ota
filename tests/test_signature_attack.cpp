#include <gtest/gtest.h>
#include "security/signature_verifier.h"
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

class SignatureAttackTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = "/tmp/ota_sig_attack_test";
        system(("mkdir -p " + test_dir).c_str());

        generate_trusted_keypair();
        generate_untrusted_keypair();
    }

    void TearDown() override {
        system(("rm -rf " + test_dir).c_str());
    }

    void generate_keypair(const std::string& priv_path, const std::string& pub_path) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
        EVP_PKEY_keygen_init(ctx);
        EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1);

        EVP_PKEY* pkey = nullptr;
        EVP_PKEY_keygen(ctx, &pkey);
        EVP_PKEY_CTX_free(ctx);

        FILE* priv_file = fopen(priv_path.c_str(), "w");
        PEM_write_PrivateKey(priv_file, pkey, nullptr, nullptr, 0, nullptr, nullptr);
        fclose(priv_file);

        FILE* pub_file = fopen(pub_path.c_str(), "w");
        PEM_write_PUBKEY(pub_file, pkey);
        fclose(pub_file);

        EVP_PKEY_free(pkey);
    }

    void generate_trusted_keypair() {
        trusted_priv_path = test_dir + "/trusted.key";
        trusted_pub_path = test_dir + "/trusted.pub";
        generate_keypair(trusted_priv_path, trusted_pub_path);
    }

    void generate_untrusted_keypair() {
        untrusted_priv_path = test_dir + "/untrusted.key";
        untrusted_pub_path = test_dir + "/untrusted.pub";
        generate_keypair(untrusted_priv_path, untrusted_pub_path);
    }

    std::string sign_data(const std::string& data, const std::string& key_path) {
        FILE* fp = fopen(key_path.c_str(), "r");
        EVP_PKEY* pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
        fclose(fp);

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

    std::string create_metadata(const std::string& version = "1.0.0") {
        return R"({"version":")" + version + R"(","sha256":"abc123","size":1024})";
    }

    std::string test_dir;
    std::string trusted_priv_path;
    std::string trusted_pub_path;
    std::string untrusted_priv_path;
    std::string untrusted_pub_path;
    ota::SignatureVerifier verifier;
};

TEST_F(SignatureAttackTest, Attack1ModifiedMetadata) {
    std::string metadata = create_metadata("1.0.0");
    std::string sig = sign_data(metadata, trusted_priv_path);

    std::string modified_metadata = create_metadata("2.0.0");

    auto result = verifier.verify_signature(modified_metadata, sig, trusted_pub_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_INVALID);
}

TEST_F(SignatureAttackTest, Attack2WrongPublicKey) {
    std::string metadata = create_metadata();
    std::string sig = sign_data(metadata, trusted_priv_path);

    auto result = verifier.verify_signature(metadata, sig, untrusted_pub_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_INVALID);
}

TEST_F(SignatureAttackTest, Attack3ModifiedSignature) {
    std::string metadata = create_metadata();
    std::string sig = sign_data(metadata, trusted_priv_path);

    std::string modified_sig = sig;
    modified_sig[10] = 'A';

    auto result = verifier.verify_signature(metadata, modified_sig, trusted_pub_path);
    EXPECT_FALSE(result.is_valid());
}

TEST_F(SignatureAttackTest, Attack4MissingSignature) {
    std::string metadata = create_metadata();

    auto result = verifier.verify_signature(metadata, "", trusted_pub_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_MISSING);
}

TEST_F(SignatureAttackTest, Attack5MissingPublicKey) {
    std::string metadata = create_metadata();
    std::string sig = sign_data(metadata, trusted_priv_path);

    auto result = verifier.verify_signature(metadata, sig, "/nonexistent/key.pub");
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::PUBLIC_KEY_MISSING);
}

TEST_F(SignatureAttackTest, Attack6InvalidPublicKey) {
    std::string metadata = create_metadata();
    std::string sig = sign_data(metadata, trusted_priv_path);

    std::string invalid_key_path = test_dir + "/invalid.pub";
    std::ofstream(invalid_key_path) << "not a valid key";

    auto result = verifier.verify_signature(metadata, sig, invalid_key_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::PUBLIC_KEY_CANNOT_LOAD);
}

TEST_F(SignatureAttackTest, Attack7EmptyMetadata) {
    std::string sig = sign_data("", trusted_priv_path);

    auto result = verifier.verify_signature("", sig, trusted_pub_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNED_DATA_INVALID);
}

TEST_F(SignatureAttackTest, Attack8TruncatedSignature) {
    std::string metadata = create_metadata();
    std::string sig = sign_data(metadata, trusted_priv_path);

    std::string truncated_sig = sig.substr(0, sig.size() / 2);

    auto result = verifier.verify_signature(metadata, truncated_sig, trusted_pub_path);
    EXPECT_FALSE(result.is_valid());
}

TEST_F(SignatureAttackTest, Attack9ReplayAttack) {
    std::string metadata_v1 = create_metadata("1.0.0");
    std::string metadata_v2 = create_metadata("2.0.0");

    std::string sig_v1 = sign_data(metadata_v1, trusted_priv_path);

    auto result = verifier.verify_signature(metadata_v2, sig_v1, trusted_pub_path);
    EXPECT_FALSE(result.is_valid());
}

TEST_F(SignatureAttackTest, Attack10UntrustedSigner) {
    std::string metadata = create_metadata();
    std::string sig = sign_data(metadata, untrusted_priv_path);

    auto result = verifier.verify_signature(metadata, sig, trusted_pub_path);
    EXPECT_FALSE(result.is_valid());
    EXPECT_EQ(result.status, ota::SignatureStatus::SIGNATURE_INVALID);
}
