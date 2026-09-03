#include "client/update_manager.h"
#include <sstream>
#include <algorithm>

namespace ota {

UpdateManager::UpdateManager() {
    download_manager_.set_download_dir("/var/lib/ota/downloads");
}

void UpdateManager::set_server_url(const std::string& url) {
    server_url_ = url;
    http_client_.set_server_url(url);
}

void UpdateManager::set_download_dir(const std::string& dir) {
    download_manager_.set_download_dir(dir);
}

void UpdateManager::set_device_config(const DeviceConfig& config) {
    device_config_ = config;
}

UpdateInfo UpdateManager::check_for_update() {
    UpdateInfo info;
    info.result = UpdateCheckResult::ERROR;

    if (server_url_.empty()) {
        info.error_message = "Server URL not configured";
        return info;
    }

    std::string path = "/api/v1/update?device_id=" + device_config_.device_id +
                      "&hardware_version=" + device_config_.hardware_version +
                      "&current_version=" + device_config_.software_version;

    HTTPResponse response = http_client_.get(path);

    if (!response.is_success) {
        info.error_message = "Failed to connect to server: " + response.error;
        return info;
    }

    UpdateMetadata metadata;
    if (!ResponseParser::parse_update_response(response.body, metadata)) {
        info.error_message = "Failed to parse server response";
        return info;
    }

    if (!metadata.update_available) {
        info.result = UpdateCheckResult::NO_UPDATE;
        info.metadata = metadata;
        return info;
    }

    std::string validation_error = ResponseParser::get_validation_error(metadata);
    if (!validation_error.empty()) {
        info.error_message = "Invalid metadata: " + validation_error;
        return info;
    }

    if (!is_compatible(device_config_.hardware_version, metadata.hardware_version)) {
        info.result = UpdateCheckResult::INCOMPATIBLE;
        info.error_message = "Hardware version mismatch: device=" +
                            device_config_.hardware_version +
                            " release=" + metadata.hardware_version;
        return info;
    }

    if (!compare_versions(device_config_.software_version, metadata.version)) {
        info.result = UpdateCheckResult::NO_UPDATE;
        info.metadata = metadata;
        return info;
    }

    info.result = UpdateCheckResult::UPDATE_AVAILABLE;
    info.metadata = metadata;
    return info;
}

DownloadResult UpdateManager::download_update(const std::string& version,
                                             DownloadProgressCallback progress) {
    std::string url = server_url_ + "/releases/" + version + "/image.bin";
    return download_manager_.download(url, version, progress);
}

bool UpdateManager::compare_versions(const std::string& v1, const std::string& v2) {
    auto parse = [](const std::string& v) -> std::tuple<int, int, int> {
        int major = 0, minor = 0, patch = 0;
        std::istringstream ss(v);
        char dot;
        ss >> major >> dot >> minor >> dot >> patch;
        return std::make_tuple(major, minor, patch);
    };

    return parse(v1) < parse(v2);
}

bool UpdateManager::is_compatible(const std::string& device_hw, const std::string& release_hw) {
    return device_hw == release_hw;
}

std::string UpdateManager::get_current_version() const {
    return device_config_.software_version;
}

std::string UpdateManager::get_device_id() const {
    return device_config_.device_id;
}

std::string UpdateManager::get_hardware_version() const {
    return device_config_.hardware_version;
}

}
