#include "client/response_parser.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace ota {

bool ResponseParser::parse_update_response(const std::string& json, UpdateMetadata& metadata) {
    if (json.empty()) {
        return false;
    }

    bool update_available = false;
    if (!parse_json_bool(json, "update_available", update_available)) {
        return false;
    }

    metadata.update_available = update_available;

    if (!update_available) {
        parse_json_field(json, "current_version", metadata.current_version);
        parse_json_field(json, "hardware_version", metadata.hardware_version);
        return true;
    }

    if (!parse_json_field(json, "version", metadata.version)) {
        return false;
    }

    if (!parse_json_field(json, "hardware_version", metadata.hardware_version)) {
        return false;
    }

    if (!parse_json_field(json, "image", metadata.image_path)) {
        return false;
    }

    if (!parse_json_int64(json, "size", metadata.image_size)) {
        return false;
    }

    if (!parse_json_field(json, "sha256", metadata.sha256)) {
        return false;
    }

    parse_json_field(json, "release_type", metadata.release_type);
    parse_json_field(json, "timestamp", metadata.timestamp);

    return true;
}

bool ResponseParser::validate_metadata(const UpdateMetadata& metadata) {
    return !get_validation_error(metadata).empty() == false;
}

std::string ResponseParser::get_validation_error(const UpdateMetadata& metadata) {
    if (!metadata.update_available) {
        return "";
    }

    if (metadata.version.empty()) {
        return "Missing version";
    }

    if (metadata.hardware_version.empty()) {
        return "Missing hardware_version";
    }

    if (metadata.image_path.empty()) {
        return "Missing image path";
    }

    if (metadata.sha256.empty()) {
        return "Missing sha256";
    }

    if (metadata.image_size <= 0) {
        return "Invalid image size";
    }

    std::regex semver_regex(R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$)");
    if (!std::regex_match(metadata.version, semver_regex)) {
        return "Invalid version format";
    }

    return "";
}

bool ResponseParser::parse_json_field(const std::string& json, const std::string& key, std::string& value) {
    std::string search_key = "\"" + key + "\"";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return false;
    }

    pos = json.find(":", pos + search_key.length());
    if (pos == std::string::npos) {
        return false;
    }

    pos = json.find_first_not_of(" \t\n\r", pos + 1);
    if (pos == std::string::npos) {
        return false;
    }

    if (json[pos] == '"') {
        size_t start = pos + 1;
        size_t end = json.find("\"", start);
        if (end == std::string::npos) {
            return false;
        }
        value = json.substr(start, end - start);
        return true;
    }

    size_t end = json.find_first_of(",}\n", pos);
    if (end == std::string::npos) {
        end = json.length();
    }
    value = json.substr(pos, end - pos);
    return true;
}

bool ResponseParser::parse_json_bool(const std::string& json, const std::string& key, bool& value) {
    std::string str_value;
    if (!parse_json_field(json, key, str_value)) {
        return false;
    }

    if (str_value == "true" || str_value == "1") {
        value = true;
        return true;
    }
    if (str_value == "false" || str_value == "0") {
        value = false;
        return true;
    }

    return false;
}

bool ResponseParser::parse_json_int64(const std::string& json, const std::string& key, int64_t& value) {
    std::string str_value;
    if (!parse_json_field(json, key, str_value)) {
        return false;
    }

    try {
        value = std::stoll(str_value);
        return true;
    } catch (...) {
        return false;
    }
}

}
