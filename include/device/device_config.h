#pragma once

#include <string>
#include <optional>

namespace ota {

struct DeviceConfig {
    std::string device_id;
    std::string hardware_version;
    std::string software_version;
    std::string active_slot;
};

std::optional<DeviceConfig> load_config(const std::string& config_path);

bool validate_config(const DeviceConfig& config);

std::string config_error_message(const DeviceConfig& config);

}
