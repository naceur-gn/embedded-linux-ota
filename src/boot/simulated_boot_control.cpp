#include "boot/simulated_boot_control.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace ota {

SimulatedBootControl::SimulatedBootControl()
    : current_slot_(SlotId::SLOT_A),
      next_slot_(SlotId::SLOT_A),
      next_slot_set_(false) {
    boot_attempts_[SlotId::SLOT_A] = 0;
    boot_attempts_[SlotId::SLOT_B] = 0;
}

SimulatedBootControl::~SimulatedBootControl() {
}

void SimulatedBootControl::set_config(const SimulatedBootConfig& config) {
    config_ = config;
}

void SimulatedBootControl::set_bootloader(std::shared_ptr<Bootloader> bootloader) {
    bootloader_ = bootloader;
}

std::shared_ptr<Bootloader> SimulatedBootControl::get_bootloader() const {
    return bootloader_;
}

bool SimulatedBootControl::initialize() {
    if (config_.boot_state_dir.empty()) {
        config_ = get_default_config();
    }

    if (!ensure_directory_exists(config_.boot_state_dir)) {
        return false;
    }

    if (bootloader_) {
        std::string bl_state_dir = config_.boot_state_dir + "/bootloader";
        std::string bl_state_file = config_.boot_state_dir + "/bootloader/bootloader_state.json";
        bootloader_->set_state_dir(bl_state_dir, bl_state_file);

        if (!bootloader_->initialize()) {
            return false;
        }

        BootloaderState bl_state = bootloader_->get_state();
        current_slot_ = bl_state.current_slot;
        next_slot_ = bl_state.next_boot_slot;
        next_slot_set_ = bl_state.next_boot_slot_set;
        boot_attempts_[SlotId::SLOT_A] = bl_state.boot_attempts_a;
        boot_attempts_[SlotId::SLOT_B] = bl_state.boot_attempts_b;

        return true;
    }

    return load_boot_state();
}

SlotId SimulatedBootControl::get_current_boot_slot() const {
    return current_slot_;
}

SlotId SimulatedBootControl::get_next_boot_slot() const {
    if (!next_slot_set_) {
        return current_slot_;
    }
    return next_slot_;
}

bool SimulatedBootControl::set_next_boot_slot(SlotId slot) {
    if (slot != SlotId::SLOT_A && slot != SlotId::SLOT_B) {
        return false;
    }

    if (slot == current_slot_) {
        return false;
    }

    next_slot_ = slot;
    next_slot_set_ = true;

    if (bootloader_) {
        if (!bootloader_->set_next_boot_slot(slot)) {
            return false;
        }
    }

    return persist_boot_state();
}

bool SimulatedBootControl::clear_next_boot_slot() {
    next_slot_set_ = false;

    if (bootloader_) {
        if (!bootloader_->clear_next_boot_slot()) {
            return false;
        }
    }

    return persist_boot_state();
}

int SimulatedBootControl::get_boot_attempt_count(SlotId slot) const {
    auto it = boot_attempts_.find(slot);
    if (it != boot_attempts_.end()) {
        return it->second;
    }
    return 0;
}

bool SimulatedBootControl::reset_boot_attempt_count(SlotId slot) {
    if (slot != SlotId::SLOT_A && slot != SlotId::SLOT_B) {
        return false;
    }

    boot_attempts_[slot] = 0;

    return persist_boot_state();
}

bool SimulatedBootControl::simulate_boot() {
    if (!next_slot_set_) {
        return false;
    }

    SlotId target = next_slot_;

    if (bootloader_) {
        if (!bootloader_->mark_boot_started(target)) {
            return false;
        }

        BootloaderState bl_state = bootloader_->get_state();
        current_slot_ = bl_state.current_slot;
        next_slot_set_ = bl_state.next_boot_slot_set;
        boot_attempts_[SlotId::SLOT_A] = bl_state.boot_attempts_a;
        boot_attempts_[SlotId::SLOT_B] = bl_state.boot_attempts_b;

        return persist_boot_state();
    }

    current_slot_ = target;
    next_slot_set_ = false;
    boot_attempts_[target]++;

    return persist_boot_state();
}

BootState SimulatedBootControl::get_boot_state() const {
    BootState state;
    state.current_slot = current_slot_;
    state.next_slot = next_slot_set_ ? next_slot_ : current_slot_;
    state.boot_attempts = boot_attempts_;
    return state;
}

bool SimulatedBootControl::persist_boot_state() {
    std::string json = boot_state_to_json();
    std::string tmp_file = config_.boot_state_file + ".tmp";

    std::ofstream ofs(tmp_file);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << json;
    ofs.close();

    if (rename(tmp_file.c_str(), config_.boot_state_file.c_str()) != 0) {
        return false;
    }

    return true;
}

bool SimulatedBootControl::load_boot_state() {
    std::ifstream ifs(config_.boot_state_file);
    if (!ifs.is_open()) {
        return persist_boot_state();
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string json = buffer.str();

    return boot_state_from_json(json);
}

bool SimulatedBootControl::validate_boot_target(SlotId slot, const SlotManager& slot_manager) const {
    if (slot != SlotId::SLOT_A && slot != SlotId::SLOT_B) {
        return false;
    }

    SlotInfo info = slot_manager.get_slot_info(slot);
    if (info.state == SlotState::EMPTY || info.state == SlotState::INVALID) {
        return false;
    }

    if (slot == current_slot_) {
        return false;
    }

    return true;
}

bool SimulatedBootControl::prepare_next_boot(SlotId slot, SlotManager& slot_manager) {
    if (!validate_boot_target(slot, slot_manager)) {
        return false;
    }

    return set_next_boot_slot(slot);
}

SimulatedBootConfig SimulatedBootControl::get_default_config() {
    SimulatedBootConfig config;
    config.boot_state_dir = "/var/lib/ota/boot";
    config.boot_state_file = "/var/lib/ota/boot/boot_state.json";
    return config;
}

SimulatedBootConfig SimulatedBootControl::get_test_config(const std::string& base_dir) {
    SimulatedBootConfig config;
    config.boot_state_dir = base_dir + "/boot";
    config.boot_state_file = base_dir + "/boot/boot_state.json";
    return config;
}

bool SimulatedBootControl::ensure_directory_exists(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    if (mkdir(path.c_str(), 0755) != 0) {
        return false;
    }

    return true;
}

std::string SimulatedBootControl::escape_json_string(const std::string& str) const {
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

std::string SimulatedBootControl::unescape_json_string(const std::string& str) const {
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

std::string SimulatedBootControl::extract_json_value(const std::string& json, const std::string& key) const {
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

std::string SimulatedBootControl::boot_state_to_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"current_slot\": \"" << slot_id_to_string(current_slot_) << "\",\n";
    ss << "  \"next_slot_set\": " << (next_slot_set_ ? "true" : "false") << ",\n";
    ss << "  \"next_slot\": \"" << slot_id_to_string(next_slot_) << "\",\n";
    ss << "  \"boot_attempts\": {\n";
    ss << "    \"A\": " << boot_attempts_.at(SlotId::SLOT_A) << ",\n";
    ss << "    \"B\": " << boot_attempts_.at(SlotId::SLOT_B) << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

bool SimulatedBootControl::boot_state_from_json(const std::string& json) {
    if (json.empty()) {
        return true;
    }

    std::string current_str = extract_json_value(json, "current_slot");
    if (!current_str.empty()) {
        current_slot_ = string_to_slot_id(current_str);
    }

    std::string next_set_str = extract_json_value(json, "next_slot_set");
    if (!next_set_str.empty()) {
        next_slot_set_ = (next_set_str == "true");
    }

    std::string next_str = extract_json_value(json, "next_slot");
    if (!next_str.empty()) {
        next_slot_ = string_to_slot_id(next_str);
    }

    size_t boot_attempts_start = json.find("\"boot_attempts\"");
    if (boot_attempts_start != std::string::npos) {
        size_t obj_start = json.find('{', boot_attempts_start);
        if (obj_start != std::string::npos) {
            size_t obj_end = json.find('}', obj_start);
            if (obj_end != std::string::npos) {
                std::string boot_attempts_json = json.substr(obj_start, obj_end - obj_start + 1);

                std::string attempts_a_str = extract_json_value(boot_attempts_json, "A");
                if (!attempts_a_str.empty()) {
                    try {
                        int val = std::stoi(attempts_a_str);
                        boot_attempts_[SlotId::SLOT_A] = (val >= 0) ? val : 0;
                    } catch (...) {
                        boot_attempts_[SlotId::SLOT_A] = 0;
                    }
                }

                std::string attempts_b_str = extract_json_value(boot_attempts_json, "B");
                if (!attempts_b_str.empty()) {
                    try {
                        int val = std::stoi(attempts_b_str);
                        boot_attempts_[SlotId::SLOT_B] = (val >= 0) ? val : 0;
                    } catch (...) {
                        boot_attempts_[SlotId::SLOT_B] = 0;
                    }
                }
            }
        }
    }

    return true;
}

}