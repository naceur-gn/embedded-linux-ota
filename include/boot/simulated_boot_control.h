#pragma once

#include "boot/boot_control.h"
#include "boot/bootloader/bootloader.hpp"
#include "boot/bootloader/simulated_bootloader.hpp"
#include "logging/logger.h"
#include <string>
#include <map>
#include <memory>

namespace ota {

struct SimulatedBootConfig {
    std::string boot_state_dir;
    std::string boot_state_file;
};

class SimulatedBootControl : public BootControl {
public:
    SimulatedBootControl();
    ~SimulatedBootControl() override;

    void set_config(const SimulatedBootConfig& config);

    void set_bootloader(std::shared_ptr<Bootloader> bootloader);

    std::shared_ptr<Bootloader> get_bootloader() const;

    bool initialize() override;

    SlotId get_current_boot_slot() const override;

    SlotId get_next_boot_slot() const override;

    bool set_next_boot_slot(SlotId slot) override;

    bool clear_next_boot_slot() override;

    int get_boot_attempt_count(SlotId slot) const override;

    bool reset_boot_attempt_count(SlotId slot) override;

    bool simulate_boot() override;

    BootState get_boot_state() const override;

    bool persist_boot_state() override;

    bool load_boot_state() override;

    bool validate_boot_target(SlotId slot, const SlotManager& slot_manager) const override;

    bool prepare_next_boot(SlotId slot, SlotManager& slot_manager) override;

    SimulatedBootConfig get_default_config();

    static SimulatedBootConfig get_test_config(const std::string& base_dir);

private:
    bool ensure_directory_exists(const std::string& path);

    std::string escape_json_string(const std::string& str) const;

    std::string unescape_json_string(const std::string& str) const;

    std::string extract_json_value(const std::string& json, const std::string& key) const;

    std::string boot_state_to_json() const;

    bool boot_state_from_json(const std::string& json);

    SimulatedBootConfig config_;
    SlotId current_slot_;
    SlotId next_slot_;
    std::map<SlotId, int> boot_attempts_;
    bool next_slot_set_;
    std::shared_ptr<Bootloader> bootloader_;
};

}