#include "boot/bootloader/simulated_bootloader.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace ota {

std::string bootloader_error_to_string(BootloaderError error) {
    switch (error) {
        case BootloaderError::NONE: return "NONE";
        case BootloaderError::BOOTLOADER_STATE_MISSING: return "BOOTLOADER_STATE_MISSING";
        case BootloaderError::BOOTLOADER_STATE_CORRUPTED: return "BOOTLOADER_STATE_CORRUPTED";
        case BootloaderError::INVALID_SLOT: return "INVALID_SLOT";
        case BootloaderError::INVALID_BOOT_STATE: return "INVALID_BOOT_STATE";
        case BootloaderError::TARGET_SLOT_NOT_BOOTABLE: return "TARGET_SLOT_NOT_BOOTABLE";
        case BootloaderError::PERSISTENCE_ERROR: return "PERSISTENCE_ERROR";
        case BootloaderError::BOOT_ATTEMPT_UPDATE_FAILED: return "BOOT_ATTEMPT_UPDATE_FAILED";
        case BootloaderError::SIMULATED_BOOT_FAILED: return "SIMULATED_BOOT_FAILED";
    }
    return "UNKNOWN";
}

SimulatedBootloader::SimulatedBootloader()
    : current_slot_(SlotId::SLOT_A),
      next_boot_slot_(SlotId::SLOT_A),
      next_boot_slot_set_(false),
      boot_attempts_a_(0),
      boot_attempts_b_(0),
      last_error_(BootloaderError::NONE) {
}

SimulatedBootloader::~SimulatedBootloader() {
}

void SimulatedBootloader::set_config(const SimulatedBootloaderConfig& config) {
    config_ = config;
}

bool SimulatedBootloader::initialize() {
    if (config_.state_dir.empty()) {
        config_ = get_default_config();
    }

    if (!ensure_directory_exists(config_.state_dir)) {
        last_error_ = BootloaderError::PERSISTENCE_ERROR;
        return false;
    }

    return load_state();
}

void SimulatedBootloader::set_state_dir(const std::string& state_dir, const std::string& state_file) {
    config_.state_dir = state_dir;
    config_.state_file = state_file;
}

SlotId SimulatedBootloader::get_current_slot() const {
    return current_slot_;
}

SlotId SimulatedBootloader::get_next_boot_slot() const {
    if (!next_boot_slot_set_) {
        return current_slot_;
    }
    return next_boot_slot_;
}

bool SimulatedBootloader::set_next_boot_slot(SlotId slot) {
    if (slot != SlotId::SLOT_A && slot != SlotId::SLOT_B) {
        last_error_ = BootloaderError::INVALID_SLOT;
        return false;
    }

    next_boot_slot_ = slot;
    next_boot_slot_set_ = true;
    last_error_ = BootloaderError::NONE;

    return persist_state();
}

bool SimulatedBootloader::clear_next_boot_slot() {
    next_boot_slot_set_ = false;
    last_error_ = BootloaderError::NONE;

    return persist_state();
}

bool SimulatedBootloader::has_pending_boot_slot() const {
    return next_boot_slot_set_;
}

int SimulatedBootloader::get_boot_attempts(SlotId slot) const {
    if (slot == SlotId::SLOT_A) {
        return boot_attempts_a_;
    } else if (slot == SlotId::SLOT_B) {
        return boot_attempts_b_;
    }
    return 0;
}

bool SimulatedBootloader::increment_boot_attempts(SlotId slot) {
    if (slot == SlotId::SLOT_A) {
        boot_attempts_a_++;
    } else if (slot == SlotId::SLOT_B) {
        boot_attempts_b_++;
    } else {
        last_error_ = BootloaderError::INVALID_SLOT;
        return false;
    }

    last_error_ = BootloaderError::NONE;
    return persist_state();
}

bool SimulatedBootloader::reset_boot_attempts(SlotId slot) {
    if (slot == SlotId::SLOT_A) {
        boot_attempts_a_ = 0;
    } else if (slot == SlotId::SLOT_B) {
        boot_attempts_b_ = 0;
    } else {
        last_error_ = BootloaderError::INVALID_SLOT;
        return false;
    }

    last_error_ = BootloaderError::NONE;
    return persist_state();
}

bool SimulatedBootloader::mark_boot_started(SlotId slot) {
    if (slot != SlotId::SLOT_A && slot != SlotId::SLOT_B) {
        last_error_ = BootloaderError::INVALID_SLOT;
        return false;
    }

    current_slot_ = slot;
    next_boot_slot_set_ = false;

    if (!increment_boot_attempts(slot)) {
        last_error_ = BootloaderError::BOOT_ATTEMPT_UPDATE_FAILED;
        return false;
    }

    last_error_ = BootloaderError::NONE;
    return true;
}

bool SimulatedBootloader::validate_slot(SlotId slot) const {
    return (slot == SlotId::SLOT_A || slot == SlotId::SLOT_B);
}

BootloaderState SimulatedBootloader::get_state() const {
    BootloaderState state;
    state.current_slot = current_slot_;
    state.next_boot_slot = next_boot_slot_set_ ? next_boot_slot_ : current_slot_;
    state.next_boot_slot_set = next_boot_slot_set_;
    state.boot_attempts_a = boot_attempts_a_;
    state.boot_attempts_b = boot_attempts_b_;
    return state;
}

bool SimulatedBootloader::persist_state() {
    std::string json = state_to_json();
    std::string tmp_file = config_.state_file + ".tmp";

    std::ofstream ofs(tmp_file);
    if (!ofs.is_open()) {
        last_error_ = BootloaderError::PERSISTENCE_ERROR;
        return false;
    }

    ofs << json;
    ofs.close();

    if (rename(tmp_file.c_str(), config_.state_file.c_str()) != 0) {
        last_error_ = BootloaderError::PERSISTENCE_ERROR;
        return false;
    }

    last_error_ = BootloaderError::NONE;
    return true;
}

bool SimulatedBootloader::load_state() {
    std::ifstream ifs(config_.state_file);
    if (!ifs.is_open()) {
        last_error_ = BootloaderError::NONE;
        return persist_state();
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string json = buffer.str();

    if (json.empty()) {
        last_error_ = BootloaderError::NONE;
        return persist_state();
    }

    return state_from_json(json);
}

BootloaderError SimulatedBootloader::get_last_error() const {
    return last_error_;
}

SimulatedBootloaderConfig SimulatedBootloader::get_default_config() {
    SimulatedBootloaderConfig config;
    config.state_dir = "/var/lib/ota/bootloader";
    config.state_file = "/var/lib/ota/bootloader/bootloader_state.json";
    return config;
}

SimulatedBootloaderConfig SimulatedBootloader::get_test_config(const std::string& base_dir) {
    SimulatedBootloaderConfig config;
    config.state_dir = base_dir + "/bootloader";
    config.state_file = base_dir + "/bootloader/bootloader_state.json";
    return config;
}

bool SimulatedBootloader::ensure_directory_exists(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    if (mkdir(path.c_str(), 0755) != 0) {
        return false;
    }

    return true;
}

std::string SimulatedBootloader::escape_json_string(const std::string& str) const {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

std::string SimulatedBootloader::unescape_json_string(const std::string& str) const {
    std::string result;
    bool escaped = false;
    for (char c : str) {
        if (escaped) {
            switch (c) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += c;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else {
            result += c;
        }
    }
    return result;
}

std::string SimulatedBootloader::extract_json_value(const std::string& json, const std::string& key) const {
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) {
        return "";
    }

    size_t colon_pos = json.find(':', key_pos + search_key.length());
    if (colon_pos == std::string::npos) {
        return "";
    }

    size_t value_start = json.find_first_not_of(" \t\n\r", colon_pos + 1);
    if (value_start == std::string::npos) {
        return "";
    }

    if (json[value_start] == '"') {
        size_t value_end = json.find('"', value_start + 1);
        if (value_end == std::string::npos) {
            return "";
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

std::string SimulatedBootloader::state_to_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"current_slot\": \"" << slot_id_to_string(current_slot_) << "\",\n";
    ss << "  \"next_boot_slot\": \"" << slot_id_to_string(next_boot_slot_) << "\",\n";
    ss << "  \"next_boot_slot_set\": " << (next_boot_slot_set_ ? "true" : "false") << ",\n";
    ss << "  \"boot_attempts_a\": " << boot_attempts_a_ << ",\n";
    ss << "  \"boot_attempts_b\": " << boot_attempts_b_ << "\n";
    ss << "}";
    return ss.str();
}

bool SimulatedBootloader::state_from_json(const std::string& json) {
    if (json.empty()) {
        return true;
    }

    std::string current_str = extract_json_value(json, "current_slot");
    if (!current_str.empty()) {
        SlotId parsed = string_to_slot_id(current_str);
        if (parsed != SlotId::SLOT_A && parsed != SlotId::SLOT_B) {
            last_error_ = BootloaderError::BOOTLOADER_STATE_CORRUPTED;
            return false;
        }
        current_slot_ = parsed;
    }

    std::string next_boot_str = extract_json_value(json, "next_boot_slot");
    if (!next_boot_str.empty()) {
        SlotId parsed = string_to_slot_id(next_boot_str);
        if (parsed != SlotId::SLOT_A && parsed != SlotId::SLOT_B) {
            last_error_ = BootloaderError::BOOTLOADER_STATE_CORRUPTED;
            return false;
        }
        next_boot_slot_ = parsed;
    }

    std::string next_set_str = extract_json_value(json, "next_boot_slot_set");
    if (!next_set_str.empty()) {
        next_boot_slot_set_ = (next_set_str == "true");
    }

    std::string attempts_a_str = extract_json_value(json, "boot_attempts_a");
    if (!attempts_a_str.empty()) {
        try {
            int val = std::stoi(attempts_a_str);
            boot_attempts_a_ = (val >= 0) ? val : 0;
        } catch (...) {
            last_error_ = BootloaderError::BOOTLOADER_STATE_CORRUPTED;
            return false;
        }
    }

    std::string attempts_b_str = extract_json_value(json, "boot_attempts_b");
    if (!attempts_b_str.empty()) {
        try {
            int val = std::stoi(attempts_b_str);
            boot_attempts_b_ = (val >= 0) ? val : 0;
        } catch (...) {
            last_error_ = BootloaderError::BOOTLOADER_STATE_CORRUPTED;
            return false;
        }
    }

    last_error_ = BootloaderError::NONE;
    return true;
}

}
