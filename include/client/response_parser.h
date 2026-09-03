#pragma once

#include <string>
#include <cstdint>

namespace ota {

struct UpdateMetadata {
    bool update_available = false;
    std::string version;
    std::string hardware_version;
    std::string image_path;
    int64_t image_size = 0;
    std::string sha256;
    std::string release_type;
    std::string timestamp;
    std::string current_version;
};

class ResponseParser {
public:
    static bool parse_update_response(const std::string& json, UpdateMetadata& metadata);

    static bool validate_metadata(const UpdateMetadata& metadata);

    static std::string get_validation_error(const UpdateMetadata& metadata);

private:
    static bool parse_json_field(const std::string& json, const std::string& key, std::string& value);

    static bool parse_json_bool(const std::string& json, const std::string& key, bool& value);

    static bool parse_json_int64(const std::string& json, const std::string& key, int64_t& value);
};

}
