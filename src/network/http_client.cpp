#include "network/http_client.h"
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>

namespace ota {

static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total = size * nmemb;
    userp->append(static_cast<char*>(contents), total);
    return total;
}

static size_t write_file_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    FILE* fp = static_cast<FILE*>(userp);
    return fwrite(contents, size, nmemb, fp);
}

struct ProgressData {
    int64_t max_size;
    bool aborted;
};

static int progress_callback(void* clientp, int64_t dltotal, int64_t, int64_t, int64_t) {
    ProgressData* data = static_cast<ProgressData*>(clientp);
    if (data && dltotal > data->max_size) {
        data->aborted = true;
        return 1;
    }
    return 0;
}

HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

void HttpClient::set_server_url(const std::string& url) {
    server_url_ = url;
}

void HttpClient::set_connect_timeout(int seconds) {
    connect_timeout_ = seconds;
}

void HttpClient::set_request_timeout(int seconds) {
    request_timeout_ = seconds;
}

void HttpClient::set_download_timeout(int seconds) {
    download_timeout_ = seconds;
}

void HttpClient::set_max_download_size(int64_t bytes) {
    max_download_size_ = bytes;
}

void HttpClient::set_retry_count(int count) {
    retry_count_ = count;
}

void HttpClient::set_retry_delay(int seconds) {
    retry_delay_ = seconds;
}

void HttpClient::set_ca_cert_path(const std::string& path) {
    ca_cert_path_ = path;
}

HTTPResponse HttpClient::get(const std::string& path) {
    std::string url = server_url_ + path;
    return execute_with_retry(url, HTTPMethod::GET);
}

HTTPResponse HttpClient::download(const std::string& path, const std::string& output_file,
                                 ProgressCallback /* progress */) {
    std::string url = server_url_ + path;
    HTTPResponse response;

    for (int attempt = 0; attempt <= retry_count_; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_));
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            response.error = "Failed to initialize CURL";
            response.is_success = false;
            last_error_ = response.error;
            continue;
        }

        FILE* fp = fopen(output_file.c_str(), "wb");
        if (!fp) {
            curl_easy_cleanup(curl);
            response.error = "Failed to create output file: " + output_file;
            response.is_success = false;
            last_error_ = response.error;
            response.status_code = -1;
            return response;
        }

        ProgressData progress_data{max_download_size_, false};

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, download_timeout_);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);

        if (!ca_cert_path_.empty()) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, ca_cert_path_.c_str());
        }

        CURLcode res = curl_easy_perform(curl);

        fclose(fp);

        if (res == CURLE_ABORTED_BY_CALLBACK || progress_data.aborted) {
            remove(output_file.c_str());
            response.error = "Download size limit exceeded";
            response.is_success = false;
            response.status_code = -1;
            last_error_ = response.error;
            curl_easy_cleanup(curl);
            break;
        }

        if (res != CURLE_OK) {
            remove(output_file.c_str());
            response.error = curl_easy_strerror(res);
            response.is_success = false;
            last_error_ = response.error;
            curl_easy_cleanup(curl);
            continue;
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        response.status_code = static_cast<int>(http_code);

        curl_easy_cleanup(curl);

        if (http_code >= 200 && http_code < 300) {
            response.is_success = true;
            return response;
        }

        remove(output_file.c_str());

        if (http_code == 400 || http_code == 404) {
            response.error = "HTTP error: " + std::to_string(http_code);
            response.is_success = false;
            last_error_ = response.error;
            return response;
        }
    }

    if (response.error.empty()) {
        response.error = "Max retries exceeded";
        response.is_success = false;
        last_error_ = response.error;
    }

    return response;
}

std::string HttpClient::last_error() const {
    return last_error_;
}

HTTPResponse HttpClient::execute_with_retry(const std::string& url, HTTPMethod method,
                                           const std::string& body) {
    HTTPResponse response;

    for (int attempt = 0; attempt <= retry_count_; ++attempt) {
        if (attempt > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(retry_delay_));
        }

        response = execute_request(url, method, body);

        if (response.is_success) {
            return response;
        }

        if (response.status_code == 400 || response.status_code == 404) {
            return response;
        }
    }

    return response;
}

HTTPResponse HttpClient::execute_request(const std::string& url, HTTPMethod method,
                                        const std::string& body) {
    HTTPResponse response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize CURL";
        response.is_success = false;
        last_error_ = response.error;
        return response;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connect_timeout_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, request_timeout_);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (!ca_cert_path_.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca_cert_path_.c_str());
    }

    if (method == HTTPMethod::POST && !body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
        response.is_success = false;
        last_error_ = response.error;
        curl_easy_cleanup(curl);
        return response;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    response.status_code = static_cast<int>(http_code);
    response.is_success = (http_code >= 200 && http_code < 300);

    curl_easy_cleanup(curl);

    return response;
}

}
