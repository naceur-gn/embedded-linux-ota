# Device Foundation

This document describes the minimal Linux device environment that hosts the OTA client.

## Architecture

```
                 EMBEDDED LINUX DEVICE
                         │
             ┌───────────┴───────────┐
             │                       │
        /etc/ota/              /var/lib/ota/
        Configuration           Persistent State
             │                       │
             └───────────┬───────────┘
                         │
                         ▼
                  OTA Application
                         │
                 ┌───────┴───────┐
                 │               │
             Logging          systemd
                 │               │
                 ▼               ▼
            /var/log/ota/    ota-client.service
```

## Device Identity

The device is identified by four properties:

| Property | Example | Description |
|---|---|---|
| `device_id` | `device-001` | Unique identifier for this device |
| `hardware_version` | `revA` | Hardware revision |
| `software_version` | `1.0.0` | Current software version (semver) |
| `active_slot` | `A` | Currently active boot slot |

## Filesystem Layout

| Path | Purpose | Owner | Permissions |
|---|---|---|---|
| `/opt/ota/bin/` | OTA application binaries | root:root | 755 |
| `/etc/ota/` | Configuration files | root:root | 755 |
| `/etc/ota/device.conf` | Device configuration | root:root | 644 |
| `/var/lib/ota/` | Persistent state | ota:ota | 700 |
| `/var/lib/ota/state.conf` | Current state | ota:ota | 600 |
| `/var/lib/ota/downloads/` | Temporary download storage | ota:ota | 700 |
| `/var/log/ota/` | Log files | ota:ota | 750 |
| `/var/log/ota/ota.log` | Application log | ota:ota | 640 |

## Configuration

The device configuration is stored in `/etc/ota/device.conf` using a simple key=value format:

```ini
device_id=device-001
hardware_version=revA
software_version=1.0.0
active_slot=A
```

### Validation Rules

- `device_id`: Must not be empty
- `hardware_version`: Must not be empty
- `software_version`: Must follow semantic versioning (MAJOR.MINOR.PATCH)
- `active_slot`: Must be `A` or `B`

## Persistent State

The OTA state is stored in `/var/lib/ota/state.conf`:

```ini
current_version=1.0.0
active_slot=A
pending_slot=
pending_version=
update_state=IDLE
boot_attempts=0
rollback_reason=
```

### State Fields

| Field | Description |
|---|---|
| `current_version` | Version of the currently running system |
| `active_slot` | Which slot is currently booted |
| `pending_slot` | Slot being written to for pending update |
| `pending_version` | Version being installed |
| `update_state` | Current state in the update state machine |
| `boot_attempts` | Number of attempts to boot the new version |
| `rollback_reason` | Reason for rollback if one occurred |

## Logging

The logger writes to `/var/log/ota/ota.log` with format:

```
YYYY-MM-DD HH:MM:SS.mmm [LEVEL] component: message
```

Log levels: DEBUG, INFO, WARN, ERROR

## systemd Service

The OTA client runs as a systemd service:

```bash
sudo systemctl start ota-client
sudo systemctl stop ota-client
sudo systemctl status ota-client
```

Service properties:
- Runs as `ota` user
- Restarts on failure (5 second delay)
- Restricted filesystem access
- No new privileges

## Linux User

The `ota` system user:
- Has no login shell
- Has no home directory
- Owns `/var/lib/ota/` and `/var/log/ota/`
- Does NOT own `/etc/ota/` (configuration is read-only)

### Future Privileged Operations

The following operations will require elevated privileges (not implemented yet):
- Writing to A/B partition slots
- Rebooting the system
- Modifying bootloader configuration

## Build System

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Running

```bash
# Display device information
./build/ota-device-info -c configs/device.conf -s /tmp/ota_state

# Run the OTA client (foreground)
./build/ota-client -c configs/device.conf -s /tmp/ota_state
```

## Testing

```bash
cd build
cmake --build . --target ota_tests
./tests/ota_tests
```

## Security Principles

1. The OTA service runs as a non-root user
2. Configuration files are not world-writable
3. State files are not world-readable
4. The application does not execute downloaded files
5. Private signing keys are never stored on the device
