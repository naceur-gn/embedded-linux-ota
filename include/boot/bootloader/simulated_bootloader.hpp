#pragma once

#include "boot/bootloader/bootloader.hpp"
#include "logging/logger.h"
#include <string>
#include <map>

namespace ota {

struct SimulatedBootloaderConfig {
    std::string state_dir;
    std::string state_file;
};

class SimulatedBootloader : public Bootloader {
public:
    SimulatedBootloader();
    ~SimulatedBootloader() override;

    void set_config(const SimulatedBootloaderConfig& config);

    bool initialize() override;

    void set_state_dir(const std::string& state_dir, const std::string& state_file) override;

    SlotId get_current_slot() const override;

    SlotId get_next_boot_slot() const override;

    bool set_next_boot_slot(SlotId slot) override;

    bool clear_next_boot_slot() override;

    bool has_pending_boot_slot() const override;

    int get_boot_attempts(SlotId slot) const override;

    bool increment_boot_attempts(SlotId slot) override;

    bool reset_boot_attempts(SlotId slot) override;

    bool mark_boot_started(SlotId slot) override;

    bool validate_slot(SlotId slot) const override;

    BootloaderState get_state() const override;

    bool persist_state() override;

    bool load_state() override;

    BootloaderError get_last_error() const override;

    SimulatedBootloaderConfig get_default_config();

    static SimulatedBootloaderConfig get_test_config(const std::string& base_dir);

private:
    bool ensure_directory_exists(const std::string& path);

    std::string escape_json_string(const std::string& str) const;

    std::string unescape_json_string(const std::string& str) const;

    std::string extract_json_value(const std::string& json, const std::string& key) const;

    std::string state_to_json() const;

    bool state_from_json(const std::string& json);

    SimulatedBootloaderConfig config_;
    SlotId current_slot_;
    SlotId next_boot_slot_;
    bool next_boot_slot_set_;
    int boot_attempts_a_;
    int boot_attempts_b_;
    BootloaderError last_error_;
};

}
