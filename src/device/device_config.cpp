#include "device/device_config.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace ota {

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static bool is_valid_semver(const std::string& version) {
    std::regex semver_regex(R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$)");
    return std::regex_match(version, semver_regex);
}

std::optional<DeviceConfig> load_config(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    DeviceConfig config;
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));

        if (key == "device_id") {
            config.device_id = value;
        } else if (key == "hardware_version") {
            config.hardware_version = value;
        } else if (key == "software_version") {
            config.software_version = value;
        } else if (key == "active_slot") {
            config.active_slot = value;
        }
    }

    return config;
}

bool validate_config(const DeviceConfig& config) {
    if (config.device_id.empty()) {
        return false;
    }

    if (config.hardware_version.empty()) {
        return false;
    }

    if (!is_valid_semver(config.software_version)) {
        return false;
    }

    if (config.active_slot != "A" && config.active_slot != "B") {
        return false;
    }

    return true;
}

std::string config_error_message(const DeviceConfig& config) {
    if (config.device_id.empty()) {
        return "device_id is empty";
    }

    if (config.hardware_version.empty()) {
        return "hardware_version is empty";
    }

    if (!is_valid_semver(config.software_version)) {
        return "software_version is not valid semver (expected MAJOR.MINOR.PATCH): " + config.software_version;
    }

    if (config.active_slot != "A" && config.active_slot != "B") {
        return "active_slot must be A or B, got: " + config.active_slot;
    }

    return "";
}

}
