# Boot Control Abstraction

## Purpose

The Boot Control abstraction layer provides an interface between the OTA system and the device bootloader. It allows the OTA system to specify which slot should be used on the next boot without directly modifying bootloader configuration.

## Why It Is Abstracted

The OTA system should not contain bootloader-specific code. Different devices may use:
- U-Boot
- GRUB
- systemd-boot
- Custom embedded boot mechanisms
- QEMU boot configuration

The abstraction allows the OTA logic to remain independent of the specific bootloader implementation.

## Architecture

```
OTA CLIENT
    │
    ▼
SLOT MANAGER
    │
    ▼
BOOT CONTROL API
    │
    ├── SimulatedBootControl
    ├── UBootBootControl      ← future
    └── GrubBootControl       ← future
```

## Current Simulated Backend

The simulated backend stores boot state in a JSON file:

```
/var/lib/ota/boot/
├── boot_state.json
```

Example:

```json
{
    "current_slot": "A",
    "next_slot_set": true,
    "next_slot": "B",
    "boot_attempts": {
        "A": 0,
        "B": 1
    }
}
```

## Current Boot Slot

The current boot slot represents the slot from which the system is currently running.

```
get_current_boot_slot() → A
```

This may differ from the active slot during update scenarios.

## Next Boot Slot

The next boot slot represents the slot that should be used on the next reboot.

```
get_next_boot_slot() → B
set_next_boot_slot(B)
clear_next_boot_slot()
```

## Boot Attempts

Boot attempt counters track how many times each slot has been booted.

```
get_boot_attempt_count(A) → 0
get_boot_attempt_count(B) → 1
reset_boot_attempt_count(B)
increment_boot_attempt(slot)
```

## Slot Validation

Before setting a next boot slot, the system validates:
- Slot exists (A or B)
- Slot is not empty
- Slot is not invalid
- Slot is not the currently active slot

## Persistence

Boot state is persisted using atomic writes:
1. Write to temporary file
2. Rename to final location

This ensures consistency across process restarts.

## Simulated Boot

The simulated boot operation allows testing boot/recovery logic without rebooting:

```
simulate_boot()
```

This operation:
1. Sets current slot to next slot
2. Clears next slot
3. Increments boot attempt counter

## Future U-Boot Integration

A future `UBootBootControl` implementation would:
- Read/write U-Boot environment variables
- Set `bootpart` or similar variables
- Use `fw_setenv`/`fw_getenv` commands

## Future GRUB Integration

A future `GrubBootControl` implementation would:
- Modify GRUB configuration files
- Update `grubenv` files
- Use `grub-reboot` or similar mechanisms

## Why Real Bootloader Modification Is Postponed

Real bootloader modification requires:
- Hardware-specific testing
- Recovery mechanisms
- Safety guarantees
- Bootloader access permissions

The simulated backend provides a safe development and testing environment.