# Bootloader Integration

## Overview

This document describes the bootloader abstraction layer introduced in Task 11. The bootloader interface creates a clean boundary between the OTA boot-control logic and the bootloader that will eventually control real embedded Linux booting.

## Architecture

```text
OTA Client
    │
    ▼
A/B Slot Manager
    │
    ▼
Boot Control API
    │
    ▼
Bootloader Interface
    │
    ▼
Simulated Bootloader
```

## Component Responsibilities

### Slot Manager

The Slot Manager owns:
- Slot A/B information
- Slot validity
- Slot state (EMPTY, ACTIVE, INACTIVE, PREPARED, BOOTABLE, INVALID)
- Installed version per slot
- Target slot validation

### Boot Control

The Boot Control layer owns:
- Current boot slot
- Requested next boot slot
- Boot attempts
- Boot decision persistence
- Validation of boot targets against slot states

### Bootloader Interface

The Bootloader interface defines the conceptual operations that a real bootloader would perform:
- Reading the boot decision at startup
- Selecting which slot to boot
- Starting the selected system
- Tracking boot attempts
- Persisting boot state

### Simulated Bootloader

The Simulated Bootloader implements the Bootloader interface using filesystem-backed persistent state:
- All state is stored in JSON files
- Atomic writes ensure consistency
- No interaction with real bootloader components

## Interface Design

### BootloaderError

```cpp
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
```

### BootloaderState

```cpp
struct BootloaderState {
    SlotId current_slot;
    SlotId next_boot_slot;
    bool next_boot_slot_set;
    int boot_attempts_a;
    int boot_attempts_b;
};
```

### Bootloader Interface Methods

| Method | Description |
|--------|-------------|
| `initialize()` | Initialize the bootloader and load persistent state |
| `set_state_dir(state_dir, state_file)` | Configure persistence paths |
| `get_current_slot()` | Get the currently active slot |
| `get_next_boot_slot()` | Get the next boot slot (or current if none set) |
| `set_next_boot_slot(slot)` | Set the next boot slot |
| `clear_next_boot_slot()` | Clear the pending next boot selection |
| `has_pending_boot_slot()` | Check if a next boot slot is pending |
| `get_boot_attempts(slot)` | Get boot attempt count for a slot |
| `increment_boot_attempts(slot)` | Increment boot attempt counter |
| `reset_boot_attempts(slot)` | Reset boot attempt counter to zero |
| `mark_boot_started(slot)` | Mark boot as started (sets current slot, clears pending, increments attempts) |
| `validate_slot(slot)` | Validate that a slot identifier is valid |
| `get_state()` | Get the full bootloader state |
| `persist_state()` | Persist state to filesystem |
| `load_state()` | Load state from filesystem |
| `get_last_error()` | Get the last error code |

## Simulated Bootloader

### Boot Decision Sequence

```text
              BOOT START
                  │
                  ▼
        Read persistent boot state
                  │
                  ▼
        Is a next boot slot set?
             /          \
           YES           NO
            │             │
            ▼             ▼
     Use target slot  Use current slot
            │
            ▼
    Increment attempts
            │
            ▼
       Boot simulated
```

### Simulated Boot Behavior

When `mark_boot_started(slot)` is called:

1. The target slot becomes the current slot
2. The pending next boot selection is consumed
3. The boot attempt counter for the target slot is incremented
4. The state is persisted atomically

### Persistent State

All bootloader state is stored in JSON format:

```json
{
  "current_slot": "A",
  "next_boot_slot": "B",
  "next_boot_slot_set": true,
  "boot_attempts_a": 0,
  "boot_attempts_b": 1
}
```

State is persisted using atomic writes (write to `.tmp`, then rename).

Default storage location:
- State directory: `/var/lib/ota/bootloader/`
- State file: `/var/lib/ota/bootloader/bootloader_state.json`

## Integration with BootControl

The `SimulatedBootControl` class integrates with the `Bootloader` interface:

```cpp
class SimulatedBootControl : public BootControl {
    void set_bootloader(std::shared_ptr<Bootloader> bootloader);
    std::shared_ptr<Bootloader> get_bootloader() const;
    // ...
};
```

When a bootloader is set:
1. BootControl initializes the bootloader with appropriate config
2. BootControl reads state from the bootloader on initialization
3. BootControl delegates boot operations to the bootloader
4. BootControl maintains its own state for backward compatibility

## Boot Attempt Tracking

Boot attempts are persisted per slot:

```text
slot_a_attempts = 0
slot_b_attempts = 1
```

The bootloader makes it possible to determine:
- How many times has this slot attempted to boot?

Note: Maximum retry policy and rollback are not yet implemented. Those belong to later tasks.

## Current Limitations

1. **Simulated only**: The bootloader is entirely simulated using filesystem state
2. **No real reboot**: No actual system reboot is performed
3. **No real bootloader interaction**: No U-Boot, GRUB, or systemd-boot interaction
4. **No rollback**: Automatic rollback on boot failure is not implemented
5. **No health checks**: Post-boot health verification is not implemented
6. **No boot confirmation**: No mechanism to confirm successful boot

## Future U-Boot/GRUB Integration Concept

For real embedded Linux systems, the Bootloader interface would be implemented by:

### U-Boot Integration

```cpp
class UBootBootloader : public Bootloader {
    // Implementation using U-Boot environment variables
    // boot_part=A or boot_part=B
    // Uses fw_setenv/fw_getenv commands
};
```

### GRUB Integration

```cpp
class GRUBBootloader : public Bootloader {
    // Implementation using GRUB default entry
    // Modifies /boot/grub/grub.cfg or uses grubby
};
```

### systemd-boot Integration

```cpp
class SystemdBootBootloader : public Bootloader {
    // Implementation using bootctl
    // Manages bootloader entries in /boot/loader/entries/
};
```

## Security and Safety

The bootloader abstraction:
- Never requires root privileges for tests
- Never modifies the host bootloader
- Never modifies `/boot`
- Never modifies GRUB configuration
- Never executes arbitrary shell commands
- Validates slot identifiers
- Validates persistent state
- Rejects invalid target slots
- Uses atomic state updates

The simulator is safe to run repeatedly on a development machine.
