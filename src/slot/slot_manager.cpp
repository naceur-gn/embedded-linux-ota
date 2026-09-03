#include "slot/slot_manager.h"
#include "logging/logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <sys/stat.h>

namespace ota {

std::string slot_id_to_string(SlotId slot) {
    switch (slot) {
        case SlotId::SLOT_A: return "A";
        case SlotId::SLOT_B: return "B";
        default: return "UNKNOWN";
    }
}

SlotId string_to_slot_id(const std::string& str) {
    if (str == "A" || str == "a") return SlotId::SLOT_A;
    if (str == "B" || str == "b") return SlotId::SLOT_B;
    return SlotId::SLOT_A;
}

std::string slot_state_to_string(SlotState state) {
    switch (state) {
        case SlotState::EMPTY:     return "EMPTY";
        case SlotState::ACTIVE:    return "ACTIVE";
        case SlotState::INACTIVE:  return "INACTIVE";
        case SlotState::PREPARED:  return "PREPARED";
        case SlotState::BOOTABLE:  return "BOOTABLE";
        case SlotState::INVALID:   return "INVALID";
        default:                   return "UNKNOWN";
    }
}

SlotState string_to_slot_state(const std::string& str) {
    if (str == "EMPTY")     return SlotState::EMPTY;
    if (str == "ACTIVE")    return SlotState::ACTIVE;
    if (str == "INACTIVE")  return SlotState::INACTIVE;
    if (str == "PREPARED")  return SlotState::PREPARED;
    if (str == "BOOTABLE")  return SlotState::BOOTABLE;
    if (str == "INVALID")   return SlotState::INVALID;
    return SlotState::EMPTY;
}

bool SlotInfo::is_valid() const {
    return !version.empty() &&
           state != SlotState::INVALID &&
           state != SlotState::EMPTY;
}

SlotManager::SlotManager() {
    config_ = get_default_config();
    slot_a_.slot_id = SlotId::SLOT_A;
    slot_a_.state = SlotState::EMPTY;
    slot_b_.slot_id = SlotId::SLOT_B;
    slot_b_.state = SlotState::EMPTY;
    active_slot_ = SlotId::SLOT_A;
}

void SlotManager::set_config(const SlotConfig& config) {
    config_ = config;
}

bool SlotManager::initialize_slots() {
    ensure_directory_exists(config_.slots_dir);
    ensure_directory_exists(config_.slots_dir + "/slot-a");
    ensure_directory_exists(config_.slots_dir + "/slot-b");
    ensure_directory_exists(config_.slots_dir + "/slot-a/system");
    ensure_directory_exists(config_.slots_dir + "/slot-b/system");

    if (!load_slot_state()) {
        slot_a_.slot_id = SlotId::SLOT_A;
        slot_a_.version = "";
        slot_a_.hardware_version = "";
        slot_a_.state = SlotState::EMPTY;
        slot_a_.sha256 = "";
        slot_a_.installed_at = "";

        slot_b_.slot_id = SlotId::SLOT_B;
        slot_b_.version = "";
        slot_b_.hardware_version = "";
        slot_b_.state = SlotState::EMPTY;
        slot_b_.sha256 = "";
        slot_b_.installed_at = "";

        active_slot_ = config_.default_active_slot;

        if (active_slot_ == SlotId::SLOT_A) {
            slot_a_.state = SlotState::ACTIVE;
            slot_b_.state = SlotState::INACTIVE;
        } else {
            slot_b_.state = SlotState::ACTIVE;
            slot_a_.state = SlotState::INACTIVE;
        }

        if (!config_.default_version.empty()) {
            if (active_slot_ == SlotId::SLOT_A) {
                slot_a_.version = config_.default_version;
                slot_a_.hardware_version = config_.default_hardware_version;
            } else {
                slot_b_.version = config_.default_version;
                slot_b_.hardware_version = config_.default_hardware_version;
            }
        }

        write_slot_metadata(SlotId::SLOT_A, slot_a_);
        write_slot_metadata(SlotId::SLOT_B, slot_b_);
        write_global_state();
    }

    Logger::instance().info("slot", "A/B slot system initialized. Active: " + slot_id_to_string(active_slot_));
    return true;
}

SlotId SlotManager::get_active_slot() const {
    return active_slot_;
}

SlotId SlotManager::get_inactive_slot() const {
    return (active_slot_ == SlotId::SLOT_A) ? SlotId::SLOT_B : SlotId::SLOT_A;
}

SlotInfo SlotManager::get_slot_info(SlotId slot) const {
    if (slot == SlotId::SLOT_A) {
        return slot_a_;
    }
    return slot_b_;
}

bool SlotManager::set_slot_state(SlotId slot, SlotState state) {
    if (slot == SlotId::SLOT_A) {
        slot_a_.state = state;
    } else {
        slot_b_.state = state;
    }

    Logger::instance().info("slot",
        "Slot " + slot_id_to_string(slot) + " state changed to " + slot_state_to_string(state));
    return persist_slot_state();
}

bool SlotManager::set_slot_version(SlotId slot, const std::string& version) {
    if (slot == SlotId::SLOT_A) {
        slot_a_.version = version;
    } else {
        slot_b_.version = version;
    }
    return persist_slot_state();
}

bool SlotManager::set_slot_sha256(SlotId slot, const std::string& sha256) {
    if (slot == SlotId::SLOT_A) {
        slot_a_.sha256 = sha256;
    } else {
        slot_b_.sha256 = sha256;
    }
    return persist_slot_state();
}

bool SlotManager::set_slot_hardware_version(SlotId slot, const std::string& hw_version) {
    if (slot == SlotId::SLOT_A) {
        slot_a_.hardware_version = hw_version;
    } else {
        slot_b_.hardware_version = hw_version;
    }
    return persist_slot_state();
}

std::string SlotManager::get_slot_version(SlotId slot) const {
    if (slot == SlotId::SLOT_A) {
        return slot_a_.version;
    }
    return slot_b_.version;
}

bool SlotManager::is_slot_valid(SlotId slot) const {
    if (slot == SlotId::SLOT_A) {
        return slot_a_.is_valid();
    }
    return slot_b_.is_valid();
}

bool SlotManager::is_slot_active(SlotId slot) const {
    if (slot == SlotId::SLOT_A) {
        return slot_a_.state == SlotState::ACTIVE;
    }
    return slot_b_.state == SlotState::ACTIVE;
}

bool SlotManager::is_slot_empty(SlotId slot) const {
    if (slot == SlotId::SLOT_A) {
        return slot_a_.state == SlotState::EMPTY;
    }
    return slot_b_.state == SlotState::EMPTY;
}

bool SlotManager::validate_slot(SlotId slot) const {
    SlotInfo info = get_slot_info(slot);

    if (info.slot_id != slot) {
        Logger::instance().error("slot",
            "Slot " + slot_id_to_string(slot) + " has invalid slot ID");
        return false;
    }

    if (info.state == SlotState::INVALID) {
        Logger::instance().error("slot",
            "Slot " + slot_id_to_string(slot) + " is marked INVALID");
        return false;
    }

    if (info.state == SlotState::ACTIVE && slot != active_slot_) {
        Logger::instance().error("slot",
            "Slot " + slot_id_to_string(slot) + " claims ACTIVE but global active is " +
            slot_id_to_string(active_slot_));
        return false;
    }

    if (info.state == SlotState::INACTIVE && slot == active_slot_) {
        Logger::instance().error("slot",
            "Slot " + slot_id_to_string(slot) + " claims INACTIVE but is the global active slot");
        return false;
    }

    if (!info.version.empty()) {
        bool valid_version = true;
        for (char c : info.version) {
            if (!isdigit(c) && c != '.') {
                valid_version = false;
                break;
            }
        }
        if (!valid_version) {
            Logger::instance().error("slot",
                "Slot " + slot_id_to_string(slot) + " has invalid version: " + info.version);
            return false;
        }
    }

    return true;
}

bool SlotManager::validate_slot_integrity(SlotId slot, const std::string& expected_sha256) const {
    SlotInfo info = get_slot_info(slot);

    if (!info.is_valid()) {
        Logger::instance().error("slot",
            "Slot " + slot_id_to_string(slot) + " is not valid for integrity check");
        return false;
    }

    if (info.sha256.empty()) {
        Logger::instance().error("slot",
            "Slot " + slot_id_to_string(slot) + " has no SHA-256 hash");
        return false;
    }

    if (info.sha256 != expected_sha256) {
        Logger::instance().error("slot",
            "Slot " + slot_id_to_string(slot) + " SHA-256 mismatch: expected=" +
            expected_sha256 + " actual=" + info.sha256);
        return false;
    }

    return true;
}

bool SlotManager::prepare_inactive_slot(const std::string& version,
                                        const std::string& hardware_version,
                                        const std::string& sha256) {
    SlotId inactive = get_inactive_slot();

    if (inactive == active_slot_) {
        Logger::instance().error("slot", "Cannot prepare active slot " + slot_id_to_string(inactive));
        return false;
    }

    if (!version.empty()) {
        bool valid_version = true;
        for (char c : version) {
            if (!isdigit(c) && c != '.') {
                valid_version = false;
                break;
            }
        }
        if (!valid_version) {
            Logger::instance().error("slot", "Invalid version format: " + version);
            return false;
        }
    }

    SlotInfo& slot_info = (inactive == SlotId::SLOT_A) ? slot_a_ : slot_b_;
    slot_info.version = version;
    slot_info.hardware_version = hardware_version;
    slot_info.sha256 = sha256;
    slot_info.state = SlotState::PREPARED;

    char time_buf[64];
    std::time_t now = std::time(nullptr);
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    slot_info.installed_at = time_buf;

    Logger::instance().info("slot",
        "Slot " + slot_id_to_string(inactive) + " prepared for version " + version);

    return persist_slot_state();
}

bool SlotManager::switch_active_slot() {
    SlotId inactive = get_inactive_slot();

    SlotInfo& inactive_info = (inactive == SlotId::SLOT_A) ? slot_a_ : slot_b_;
    SlotInfo& active_info = (active_slot_ == SlotId::SLOT_A) ? slot_a_ : slot_b_;

    if (inactive_info.state != SlotState::PREPARED && inactive_info.state != SlotState::BOOTABLE) {
        Logger::instance().error("slot",
            "Cannot switch to slot " + slot_id_to_string(inactive) +
            " - state is " + slot_state_to_string(inactive_info.state));
        return false;
    }

    active_info.state = SlotState::INACTIVE;
    inactive_info.state = SlotState::ACTIVE;
    active_slot_ = inactive;

    Logger::instance().info("slot",
        "Active slot switched to " + slot_id_to_string(active_slot_));

    return persist_slot_state();
}

SlotInfo SlotManager::get_slot_a_info() const {
    return slot_a_;
}

SlotInfo SlotManager::get_slot_b_info() const {
    return slot_b_;
}

bool SlotManager::persist_slot_state() {
    write_slot_metadata(SlotId::SLOT_A, slot_a_);
    write_slot_metadata(SlotId::SLOT_B, slot_b_);
    write_global_state();
    return true;
}

bool SlotManager::load_slot_state() {
    if (!read_global_state()) {
        return false;
    }

    read_slot_metadata(SlotId::SLOT_A, slot_a_);
    read_slot_metadata(SlotId::SLOT_B, slot_b_);

    return true;
}

SlotConfig SlotManager::get_default_config() {
    SlotConfig config;
    config.slots_dir = "/var/lib/ota/slots";
    config.state_file = "/var/lib/ota/slots/global.json";
    config.default_active_slot = SlotId::SLOT_A;
    config.default_version = "1.0.0";
    config.default_hardware_version = "hw-v1";
    return config;
}

SlotConfig SlotManager::get_test_config(const std::string& base_dir) {
    SlotConfig config;
    config.slots_dir = base_dir + "/slots";
    config.state_file = base_dir + "/slots/global.json";
    config.default_active_slot = SlotId::SLOT_A;
    config.default_version = "1.0.0";
    config.default_hardware_version = "hw-v1";
    return config;
}

std::string SlotManager::get_slot_dir(SlotId slot) const {
    return config_.slots_dir + "/slot-" + slot_id_to_string(slot);
}

std::string SlotManager::get_slot_metadata_file(SlotId slot) const {
    return get_slot_dir(slot) + "/metadata.json";
}

bool SlotManager::ensure_slot_directory(SlotId slot) {
    std::string dir = get_slot_dir(slot);
    ensure_directory_exists(dir);
    ensure_directory_exists(dir + "/system");
    return true;
}

bool SlotManager::write_slot_metadata(SlotId slot, const SlotInfo& info) {
    ensure_slot_directory(slot);

    std::string path = get_slot_metadata_file(slot);

    std::stringstream json;
    json << "{\n";
    json << "  \"slot\": \"" << escape_json_string(slot_id_to_string(info.slot_id)) << "\",\n";
    json << "  \"version\": \"" << escape_json_string(info.version) << "\",\n";
    json << "  \"hardware_version\": \"" << escape_json_string(info.hardware_version) << "\",\n";
    json << "  \"state\": \"" << escape_json_string(slot_state_to_string(info.state)) << "\",\n";
    json << "  \"sha256\": \"" << escape_json_string(info.sha256) << "\",\n";
    json << "  \"installed_at\": \"" << escape_json_string(info.installed_at) << "\"\n";
    json << "}\n";

    std::string tmp_path = path + ".tmp";
    std::ofstream file(tmp_path);
    if (!file.is_open()) {
        Logger::instance().error("slot", "Cannot write metadata file: " + tmp_path);
        return false;
    }

    file << json.str();
    file.flush();
    file.close();

    if (rename(tmp_path.c_str(), path.c_str()) != 0) {
        Logger::instance().error("slot", "Cannot rename metadata file");
        return false;
    }

    return true;
}

bool SlotManager::read_slot_metadata(SlotId slot, SlotInfo& info) const {
    std::string path = get_slot_metadata_file(slot);

    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    if (json.empty()) {
        return false;
    }

    info.slot_id = slot;
    info.version = extract_json_value(json, "version");
    info.hardware_version = extract_json_value(json, "hardware_version");
    info.state = string_to_slot_state(extract_json_value(json, "state"));
    info.sha256 = extract_json_value(json, "sha256");
    info.installed_at = extract_json_value(json, "installed_at");

    return true;
}

bool SlotManager::write_global_state() {
    ensure_directory_exists(config_.slots_dir);

    std::stringstream json;
    json << "{\n";
    json << "  \"active_slot\": \"" << escape_json_string(slot_id_to_string(active_slot_)) << "\"\n";
    json << "}\n";

    std::string tmp_path = config_.state_file + ".tmp";
    std::ofstream file(tmp_path);
    if (!file.is_open()) {
        Logger::instance().error("slot", "Cannot write global state file");
        return false;
    }

    file << json.str();
    file.flush();
    file.close();

    if (rename(tmp_path.c_str(), config_.state_file.c_str()) != 0) {
        Logger::instance().error("slot", "Cannot rename global state file");
        return false;
    }

    return true;
}

bool SlotManager::read_global_state() {
    std::ifstream file(config_.state_file);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();

    if (json.empty()) {
        return false;
    }

    std::string active_slot_str = extract_json_value(json, "active_slot");
    if (!active_slot_str.empty()) {
        active_slot_ = string_to_slot_id(active_slot_str);
        return true;
    }

    return false;
}

bool SlotManager::ensure_directory_exists(const std::string& path) {
    std::error_code ec;
    if (!std::filesystem::create_directories(path, ec)) {
        return !ec;
    }
    return true;
}

std::string SlotManager::escape_json_string(const std::string& str) const {
    std::string result;
    result.reserve(str.size() + 16);

    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

std::string SlotManager::unescape_json_string(const std::string& str) const {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case '"':  result += '"'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                default:   result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string SlotManager::extract_json_value(const std::string& json, const std::string& key) const {
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) {
        return "";
    }

    size_t colon_pos = json.find(':', key_pos + search_key.size());
    if (colon_pos == std::string::npos) {
        return "";
    }

    size_t value_start = json.find_first_not_of(" \t\r\n", colon_pos + 1);
    if (value_start == std::string::npos) {
        return "";
    }

    if (json[value_start] == '"') {
        size_t value_end = value_start + 1;
        while (value_end < json.size()) {
            if (json[value_end] == '\\' && value_end + 1 < json.size()) {
                value_end += 2;
            } else if (json[value_end] == '"') {
                break;
            } else {
                ++value_end;
            }
        }
        return unescape_json_string(json.substr(value_start + 1, value_end - value_start - 1));
    } else {
        size_t value_end = json.find_first_of(",}", value_start);
        if (value_end == std::string::npos) {
            return json.substr(value_start);
        }
        return json.substr(value_start, value_end - value_start);
    }
}

}
