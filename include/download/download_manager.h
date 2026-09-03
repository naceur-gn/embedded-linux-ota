#pragma once

#include <string>
#include <cstdint>
#include <functional>

namespace ota {

enum class DownloadError {
    NONE,
    NETWORK_ERROR,
    TIMEOUT,
    SERVER_ERROR,
    FILE_CREATE_ERROR,
    SIZE_LIMIT_EXCEEDED,
    INCOMPLETE_DOWNLOAD,
    CANCELLED
};

using DownloadProgressCallback = std::function<bool(int64_t current, int64_t total)>;

struct DownloadResult {
    bool success = false;
    std::string file_path;
    int64_t file_size = 0;
    std::string sha256;
    DownloadError error = DownloadError::NONE;
    std::string error_message;
};

class DownloadManager {
public:
    DownloadManager();

    void set_download_dir(const std::string& dir);

    void set_max_download_size(int64_t bytes);

    DownloadResult download(const std::string& url, const std::string& version,
                           DownloadProgressCallback progress = nullptr);

    bool cleanup_download(const std::string& file_path);

    std::string get_temp_path(const std::string& version);

    static std::string calculate_sha256(const std::string& file_path);

private:
    std::string create_temp_file(const std::string& version);

    bool ensure_download_dir();

    std::string download_dir_;
    int64_t max_download_size_ = 100 * 1024 * 1024;
};

}
