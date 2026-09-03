# A/B System Slot Architecture

## Overview

The A/B slot architecture allows the device to maintain two independent system versions. This enables safe updates by installing new versions into the inactive slot while the active slot continues running.

## Architecture

```
┌──────────────────────────────┐
│        Embedded Linux        │
│                              │
│     ┌────────┐  ┌────────┐  │
│     │ SLOT A │  │ SLOT B │  │
│     │ v1.0.0 │  │ v1.1.0 │  │
│     │ ACTIVE │  │INACTIVE│  │
│     └────────┘  └────────┘  │
└──────────────────────────────┘
```

## Slot Model

### Slot IDs

- `SLOT_A` - First system slot
- `SLOT_B` - Second system slot

### Slot States

| State | Description |
|-------|-------------|
| `EMPTY` | Slot has no valid system |
| `ACTIVE` | Slot is currently running/selected |
| `INACTIVE` | Slot is not active, available for update |
| `PREPARED` | Slot has been prepared with new version |
| `BOOTABLE` | Slot is ready to be booted |
| `INVALID` | Slot contains invalid/corrupted data |

### Slot Metadata

Each slot maintains metadata:

```json
{
    "slot": "A",
    "version": "1.0.0",
    "hardware_version": "hw-v1",
    "state": "ACTIVE",
    "sha256": "abc123...",
    "installed_at": "2026-09-03T12:00:00Z"
}
```

## Slot Manager

The `SlotManager` component manages all slot-related operations:

```cpp
SlotManager sm;
sm.set_config(config);

sm.initialize_slots();

SlotId active = sm.get_active_slot();
SlotId inactive = sm.get_inactive_slot();

sm.prepare_inactive_slot("1.1.0", "hw-v1", "sha256...");
sm.switch_active_slot();
```

## Slot Selection Rules

1. Only one slot can be `ACTIVE` at a time
2. The other slot is `INACTIVE` or in another non-active state
3. Updates must be installed into the `INACTIVE` slot
4. The active slot is protected from modification

## Slot Protection

The Slot Manager prevents accidental modification of the active slot:

```cpp
// This will fail if target is the active slot
sm.prepare_inactive_slot("1.1.0", "hw-v1", "sha256...");
```

## Slot Integrity

Slot integrity can be validated by comparing stored SHA-256 with expected hash:

```cpp
bool valid = sm.validate_slot_integrity(SlotId::SLOT_A, expected_sha256);
```

## Slot Validation

Slots can be validated for consistency:

```cpp
bool valid = sm.validate_slot(SlotId::SLOT_A);
```

Validation checks:
- Slot ID matches expected
- State is not INVALID
- ACTIVE state matches global active slot
- Version format is valid

## File Layout

```
/var/lib/ota/slots/
├── global.json           # Active slot reference
├── slot-a/
│   ├── metadata.json     # Slot A metadata
│   └── system/           # Slot A system files
└── slot-b/
    ├── metadata.json     # Slot B metadata
    └── system/           # Slot B system files
```

## CLI Commands

### Initialize Slots

```bash
ota-cli slots init
```

Output:
```
Initializing A/B slot system...

Slot A:
  Version: 1.0.0
  State: ACTIVE

Slot B:
  Version: none
  State: INACTIVE

Active slot: A
Inactive slot: B
```

### View Slot Status

```bash
ota-cli slots
```

Output:
```
A/B Slot Status

Slot A
------
State: ACTIVE
Version: 1.0.0
Valid: YES

Slot B
------
State: INACTIVE
Version: 1.1.0
Valid: YES

Active slot: A
Inactive slot: B
```

## Integration with Transaction Manager

The Transaction Manager records slot information:

```json
{
    "transaction_id": "...",
    "state": "INSTALLING",
    "active_slot": "A",
    "target_slot": "B",
    "target_version": "1.1.0"
}
```

## Integration with Installer

The installer uses the Slot Manager to determine installation target:

```
OTA Transaction
      ↓
Slot Manager
      ↓
Get inactive slot
      ↓
Install into inactive slot
      ↓
Prepare slot for activation
```

## Filesystem Simulation

This implementation uses filesystem-backed slots for development and testing:

```
/var/lib/ota/slots/slot-a/
/var/lib/ota/slots/slot-b/
```

This abstraction allows the same code to work with:
- Filesystem directories (development)
- Block devices (production)
- QEMU disks (testing)
- Real embedded storage

## Safety Rules

1. **Never destroy the active slot** during update preparation
2. **Always install into the inactive slot**
3. **Validate slot integrity** before activation
4. **Persist state atomically** to prevent corruption
5. **Detect inconsistencies** between metadata and global state

## Future Enhancements

This architecture provides the foundation for:
- Bootloader integration
- Automatic rollback
- Health checks
- Boot confirmation
- Production partition flashing
