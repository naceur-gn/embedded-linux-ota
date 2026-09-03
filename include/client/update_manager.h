#pragma once

#include <string>
#include <memory>
#include "network/http_client.h"
#include "client/response_parser.h"
#include "download/download_manager.h"
#include "device/device_config.h"
#include "device/device_state.h"

namespace ota {

enum class UpdateCheckResult {
    UPDATE_AVAILABLE,
    NO_UPDATE,
    INCOMPATIBLE,
    ERROR
};

struct UpdateInfo {
    UpdateCheckResult result;
    UpdateMetadata metadata;
    std::string error_message;
};

class UpdateManager {
public:
    UpdateManager();

    void set_server_url(const std::string& url);

    void set_download_dir(const std::string& dir);

    void set_device_config(const DeviceConfig& config);

    UpdateInfo check_for_update();

    DownloadResult download_update(const std::string& version,
                                  DownloadProgressCallback progress = nullptr);

    bool compare_versions(const std::string& v1, const std::string& v2);

    bool is_compatible(const std::string& device_hw, const std::string& release_hw);

    std::string get_current_version() const;

    std::string get_device_id() const;

    std::string get_hardware_version() const;

private:
    HttpClient http_client_;
    DownloadManager download_manager_;
    DeviceConfig device_config_;
    std::string server_url_;
};

}
