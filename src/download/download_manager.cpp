#include "download/download_manager.h"
#include "network/http_client.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <openssl/evp.h>

namespace ota {

DownloadManager::DownloadManager() {
    download_dir_ = "/var/lib/ota/downloads";
}

void DownloadManager::set_download_dir(const std::string& dir) {
    download_dir_ = dir;
}

void DownloadManager::set_max_download_size(int64_t bytes) {
    max_download_size_ = bytes;
}

DownloadResult DownloadManager::download(const std::string& url, const std::string& version,
                                        DownloadProgressCallback progress) {
    DownloadResult result;

    if (!ensure_download_dir()) {
        result.error = DownloadError::FILE_CREATE_ERROR;
        result.error_message = "Failed to create download directory";
        return result;
    }

    std::string temp_path = get_temp_path(version);

    std::ifstream check(temp_path);
    if (check.good()) {
        check.close();
        remove(temp_path.c_str());
    }

    result.file_path = temp_path;

    HttpClient client;
    client.set_max_download_size(max_download_size_);

    HTTPResponse response = client.download(url, temp_path,
        [&progress](int64_t current, int64_t total) -> bool {
            if (progress) {
                return progress(current, total);
            }
            return true;
        });

    if (!response.is_success) {
        remove(temp_path.c_str());
        result.error = DownloadError::NETWORK_ERROR;
        result.error_message = response.error;
        return result;
    }

    std::ifstream file(temp_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        remove(temp_path.c_str());
        result.error = DownloadError::FILE_CREATE_ERROR;
        result.error_message = "Failed to open downloaded file";
        return result;
    }

    result.file_size = file.tellg();
    file.close();

    if (result.file_size == 0) {
        remove(temp_path.c_str());
        result.error = DownloadError::INCOMPLETE_DOWNLOAD;
        result.error_message = "Downloaded file is empty";
        return result;
    }

    result.sha256 = calculate_sha256(temp_path);
    result.success = true;

    return result;
}

bool DownloadManager::cleanup_download(const std::string& file_path) {
    if (file_path.empty()) {
        return false;
    }

    if (file_path.find(download_dir_) == std::string::npos) {
        return false;
    }

    return remove(file_path.c_str()) == 0;
}

std::string DownloadManager::get_temp_path(const std::string& version) {
    return download_dir_ + "/" + version + ".img";
}

std::string DownloadManager::calculate_sha256(const std::string& file_path) {
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

    char buffer[4096];
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

bool DownloadManager::ensure_download_dir() {
    std::error_code ec;
    std::filesystem::create_directories(download_dir_, ec);
    return !ec;
}

}
