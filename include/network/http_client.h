#pragma once

#include <string>
#include <functional>
#include <cstdint>

namespace ota {

enum class HTTPMethod {
    GET,
    POST
};

enum class HTTPStatus {
    OK = 200,
    BAD_REQUEST = 400,
    NOT_FOUND = 404,
    SERVER_ERROR = 500
};

struct HTTPResponse {
    int status_code;
    std::string body;
    std::string error;
    bool is_success;
    HTTPResponse() : status_code(0), is_success(false) {}
};

using ProgressCallback = std::function<bool(int64_t current, int64_t total)>;

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    void set_server_url(const std::string& url);

    void set_connect_timeout(int seconds);

    void set_request_timeout(int seconds);

    void set_download_timeout(int seconds);

    void set_max_download_size(int64_t bytes);

    void set_retry_count(int count);

    void set_retry_delay(int seconds);

    void set_ca_cert_path(const std::string& path);

    HTTPResponse get(const std::string& path);

    HTTPResponse download(const std::string& path, const std::string& output_file,
                         ProgressCallback progress = nullptr);

    std::string last_error() const;

private:
    HTTPResponse execute_with_retry(const std::string& url, HTTPMethod method,
                                   const std::string& body = "");

    HTTPResponse execute_request(const std::string& url, HTTPMethod method,
                                const std::string& body = "");

    std::string server_url_;
    int connect_timeout_ = 10;
    int request_timeout_ = 30;
    int download_timeout_ = 300;
    int64_t max_download_size_ = 100 * 1024 * 1024;
    int retry_count_ = 3;
    int retry_delay_ = 2;
    std::string ca_cert_path_;
    std::string last_error_;
};

}
