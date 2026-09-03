#pragma once

#include <string>
#include <map>
#include "slot/slot_manager.h"

namespace ota {

struct BootState {
    SlotId current_slot;
    SlotId next_slot;
    std::map<SlotId, int> boot_attempts;
};

class BootControl {
public:
    virtual ~BootControl() = default;

    virtual bool initialize() = 0;

    virtual SlotId get_current_boot_slot() const = 0;

    virtual SlotId get_next_boot_slot() const = 0;

    virtual bool set_next_boot_slot(SlotId slot) = 0;

    virtual bool clear_next_boot_slot() = 0;

    virtual int get_boot_attempt_count(SlotId slot) const = 0;

    virtual bool reset_boot_attempt_count(SlotId slot) = 0;

    virtual bool simulate_boot() = 0;

    virtual BootState get_boot_state() const = 0;

    virtual bool persist_boot_state() = 0;

    virtual bool load_boot_state() = 0;

    virtual bool validate_boot_target(SlotId slot, const SlotManager& slot_manager) const = 0;

    virtual bool prepare_next_boot(SlotId slot, SlotManager& slot_manager) = 0;
};

}