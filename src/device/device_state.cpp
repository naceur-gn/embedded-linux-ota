#include "device/device_state.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace ota {

std::string update_state_to_string(UpdateState state) {
    switch (state) {
        case UpdateState::IDLE:           return "IDLE";
        case UpdateState::CHECKING:       return "CHECKING";
        case UpdateState::DOWNLOADING:    return "DOWNLOADING";
        case UpdateState::VERIFYING:      return "VERIFYING";
        case UpdateState::INSTALLING:     return "INSTALLING";
        case UpdateState::PENDING_REBOOT: return "PENDING_REBOOT";
        case UpdateState::REBOOTING:      return "REBOOTING";
        case UpdateState::HEALTH_CHECK:   return "HEALTH_CHECK";
        case UpdateState::SUCCESS:        return "SUCCESS";
        case UpdateState::CONFIRMED:      return "CONFIRMED";
        case UpdateState::FAILURE:        return "FAILURE";
        case UpdateState::ROLLBACK:       return "ROLLBACK";
        case UpdateState::RECOVERY:       return "RECOVERY";
        default:                          return "UNKNOWN";
    }
}

static UpdateState string_to_update_state(const std::string& str) {
    if (str == "IDLE")           return UpdateState::IDLE;
    if (str == "CHECKING")       return UpdateState::CHECKING;
    if (str == "DOWNLOADING")    return UpdateState::DOWNLOADING;
    if (str == "VERIFYING")      return UpdateState::VERIFYING;
    if (str == "INSTALLING")     return UpdateState::INSTALLING;
    if (str == "PENDING_REBOOT") return UpdateState::PENDING_REBOOT;
    if (str == "REBOOTING")      return UpdateState::REBOOTING;
    if (str == "HEALTH_CHECK")   return UpdateState::HEALTH_CHECK;
    if (str == "SUCCESS")        return UpdateState::SUCCESS;
    if (str == "CONFIRMED")      return UpdateState::CONFIRMED;
    if (str == "FAILURE")        return UpdateState::FAILURE;
    if (str == "ROLLBACK")       return UpdateState::ROLLBACK;
    if (str == "RECOVERY")       return UpdateState::RECOVERY;
    return UpdateState::IDLE;
}

static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::optional<PersistentState> load_state(const std::string& state_dir) {
    std::string state_file = state_dir + "/state.conf";
    std::ifstream file(state_file);
    if (!file.is_open()) {
        return std::nullopt;
    }

    PersistentState state{};
    state.update_state = UpdateState::IDLE;
    state.boot_attempts = 0;

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

        if (key == "current_version") {
            state.current_version = value;
        } else if (key == "active_slot") {
            state.active_slot = value;
        } else if (key == "pending_slot") {
            state.pending_slot = value;
        } else if (key == "pending_version") {
            state.pending_version = value;
        } else if (key == "update_state") {
            state.update_state = string_to_update_state(value);
        } else if (key == "boot_attempts") {
            try {
                state.boot_attempts = std::stoi(value);
            } catch (...) {
                state.boot_attempts = 0;
            }
        } else if (key == "rollback_reason") {
            state.rollback_reason = value;
        }
    }

    return state;
}

bool save_state(const PersistentState& state, const std::string& state_dir) {
    std::string state_file = state_dir + "/state.conf";

    std::ofstream file(state_file);
    if (!file.is_open()) {
        return false;
    }

    file << "current_version=" << state.current_version << "\n";
    file << "active_slot=" << state.active_slot << "\n";
    file << "pending_slot=" << state.pending_slot << "\n";
    file << "pending_version=" << state.pending_version << "\n";
    file << "update_state=" << update_state_to_string(state.update_state) << "\n";
    file << "boot_attempts=" << state.boot_attempts << "\n";
    file << "rollback_reason=" << state.rollback_reason << "\n";

    return file.good();
}

bool initialize_state(const std::string& state_dir, const std::string& version, const std::string& slot) {
    std::error_code ec;
    std::filesystem::create_directories(state_dir, ec);
    if (ec) {
        return false;
    }

    PersistentState state;
    state.current_version = version;
    state.active_slot = slot;
    state.pending_slot = "";
    state.pending_version = "";
    state.update_state = UpdateState::IDLE;
    state.boot_attempts = 0;
    state.rollback_reason = "";

    return save_state(state, state_dir);
}

}
