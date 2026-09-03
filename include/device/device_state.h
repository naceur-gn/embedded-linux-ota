#pragma once

#include <string>
#include <optional>

namespace ota {

enum class UpdateState {
    IDLE,
    CHECKING,
    DOWNLOADING,
    VERIFYING,
    INSTALLING,
    PENDING_REBOOT,
    REBOOTING,
    HEALTH_CHECK,
    SUCCESS,
    CONFIRMED,
    FAILURE,
    ROLLBACK,
    RECOVERY
};

struct PersistentState {
    std::string current_version;
    std::string active_slot;
    std::string pending_slot;
    std::string pending_version;
    UpdateState update_state;
    int boot_attempts;
    std::string rollback_reason;
};

std::string update_state_to_string(UpdateState state);

std::optional<PersistentState> load_state(const std::string& state_dir);

bool save_state(const PersistentState& state, const std::string& state_dir);

bool initialize_state(const std::string& state_dir, const std::string& version, const std::string& slot);

}
