#pragma once

#include <string>
#include "slot/slot_manager.h"

namespace ota {

enum class BootloaderError {
    NONE,
    BOOTLOADER_STATE_MISSING,
    BOOTLOADER_STATE_CORRUPTED,
    INVALID_SLOT,
    INVALID_BOOT_STATE,
    TARGET_SLOT_NOT_BOOTABLE,
    PERSISTENCE_ERROR,
    BOOT_ATTEMPT_UPDATE_FAILED,
    SIMULATED_BOOT_FAILED
};

std::string bootloader_error_to_string(BootloaderError error);

struct BootloaderState {
    SlotId current_slot;
    SlotId next_boot_slot;
    bool next_boot_slot_set;
    int boot_attempts_a;
    int boot_attempts_b;
};

class Bootloader {
public:
    virtual ~Bootloader() = default;

    virtual bool initialize() = 0;

    virtual void set_state_dir(const std::string& state_dir, const std::string& state_file) = 0;

    virtual SlotId get_current_slot() const = 0;

    virtual SlotId get_next_boot_slot() const = 0;

    virtual bool set_next_boot_slot(SlotId slot) = 0;

    virtual bool clear_next_boot_slot() = 0;

    virtual bool has_pending_boot_slot() const = 0;

    virtual int get_boot_attempts(SlotId slot) const = 0;

    virtual bool increment_boot_attempts(SlotId slot) = 0;

    virtual bool reset_boot_attempts(SlotId slot) = 0;

    virtual bool mark_boot_started(SlotId slot) = 0;

    virtual bool validate_slot(SlotId slot) const = 0;

    virtual BootloaderState get_state() const = 0;

    virtual bool persist_state() = 0;

    virtual bool load_state() = 0;

    virtual BootloaderError get_last_error() const = 0;
};

}
