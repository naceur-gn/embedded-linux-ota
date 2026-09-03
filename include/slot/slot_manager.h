#pragma once

#include <string>
#include <map>
#include <functional>

namespace ota {

enum class SlotId {
    SLOT_A,
    SLOT_B
};

enum class SlotState {
    EMPTY,
    ACTIVE,
    INACTIVE,
    PREPARED,
    BOOTABLE,
    INVALID
};

std::string slot_id_to_string(SlotId slot);

SlotId string_to_slot_id(const std::string& str);

std::string slot_state_to_string(SlotState state);

SlotState string_to_slot_state(const std::string& str);

struct SlotInfo {
    SlotId slot_id;
    std::string version;
    std::string hardware_version;
    SlotState state;
    std::string sha256;
    std::string installed_at;
    bool is_valid() const;
};

struct SlotConfig {
    std::string slots_dir;
    std::string state_file;
    SlotId default_active_slot;
    std::string default_version;
    std::string default_hardware_version;
};

class SlotManager {
public:
    SlotManager();

    void set_config(const SlotConfig& config);

    bool initialize_slots();

    SlotId get_active_slot() const;

    SlotId get_inactive_slot() const;

    SlotInfo get_slot_info(SlotId slot) const;

    bool set_slot_state(SlotId slot, SlotState state);

    bool set_slot_version(SlotId slot, const std::string& version);

    bool set_slot_sha256(SlotId slot, const std::string& sha256);

    bool set_slot_hardware_version(SlotId slot, const std::string& hw_version);

    std::string get_slot_version(SlotId slot) const;

    bool is_slot_valid(SlotId slot) const;

    bool is_slot_active(SlotId slot) const;

    bool is_slot_empty(SlotId slot) const;

    bool validate_slot(SlotId slot) const;

    bool validate_slot_integrity(SlotId slot, const std::string& expected_sha256) const;

    bool prepare_inactive_slot(const std::string& version,
                              const std::string& hardware_version,
                              const std::string& sha256);

    bool switch_active_slot();

    SlotInfo get_slot_a_info() const;

    SlotInfo get_slot_b_info() const;

    bool persist_slot_state();

    bool load_slot_state();

    SlotConfig get_default_config();

    static SlotConfig get_test_config(const std::string& base_dir);

private:
    std::string get_slot_dir(SlotId slot) const;

    std::string get_slot_metadata_file(SlotId slot) const;

    bool ensure_slot_directory(SlotId slot);

    bool write_slot_metadata(SlotId slot, const SlotInfo& info);

    bool read_slot_metadata(SlotId slot, SlotInfo& info) const;

    bool write_global_state();

    bool read_global_state();

    bool ensure_directory_exists(const std::string& path);

    std::string escape_json_string(const std::string& str) const;

    std::string unescape_json_string(const std::string& str) const;

    std::string extract_json_value(const std::string& json, const std::string& key) const;

    SlotConfig config_;
    SlotInfo slot_a_;
    SlotInfo slot_b_;
    SlotId active_slot_;
};

}
