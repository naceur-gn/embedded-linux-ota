# OTA Transaction Management

## Overview

The transaction management system makes OTA updates atomic and reliable. Every OTA update is treated as a transaction with a clear lifecycle, persistent state, and crash recovery.

## State Machine

```
IDLE
  |
  v
CHECKING
  |
  +-- no update --> IDLE
  |
  v
UPDATE_AVAILABLE
  |
  +-- cancel --> IDLE
  |
  v
DOWNLOADING
  |
  v
DOWNLOADED
  |
  v
VERIFYING
  |
  v
VERIFIED
  |
  v
INSTALLING
  |
  v
INSTALLED
  |
  v
IDLE
```

Failure can occur at any active state:

```
CHECKING          --> FAILED
UPDATE_AVAILABLE  --> FAILED
DOWNLOADING       --> FAILED
DOWNLOADED        --> FAILED
VERIFYING         --> FAILED
VERIFIED          --> FAILED
INSTALLING        --> FAILED
```

## Valid State Transitions

| From | To |
|---|---|
| IDLE | CHECKING |
| CHECKING | UPDATE_AVAILABLE, IDLE, FAILED |
| UPDATE_AVAILABLE | DOWNLOADING, IDLE, FAILED |
| DOWNLOADING | DOWNLOADED, FAILED |
| DOWNLOADED | VERIFYING, FAILED |
| VERIFYING | VERIFIED, FAILED |
| VERIFIED | INSTALLING, FAILED |
| INSTALLING | INSTALLED, FAILED |
| INSTALLED | IDLE |
| FAILED | IDLE |

Invalid transitions are rejected with an error log.

## Components

### TransactionStateMachine

Encapsulates the state machine logic.

```cpp
TransactionStateMachine sm;
sm.can_transition(from, to);
sm.transition_to(new_state, transaction_id);
sm.get_current_state();
sm.reset();
sm.is_active();
sm.is_terminal();
sm.is_failure();
```

### TransactionManager

Manages the full transaction lifecycle with persistence, concurrency, and history.

```cpp
TransactionManager tm;
tm.set_config(config);
tm.acquire_lock();
tm.create_transaction(target, source, hw, sha256);
tm.update_state(state, error_code, error_msg);
tm.complete_transaction();
tm.record_failure(error_code, error_msg);
tm.release_lock();
```

## Transaction ID

Every transaction gets a unique UUID-style ID generated from `/dev/urandom`:

```
550e8400-e29b-41d4-a716-446655440000
```

The ID appears in logs, persistent state, and history for debugging.

## Persistent State

Transaction state is stored as JSON:

```json
{
    "transaction_id": "550e8400-...",
    "state": "INSTALLING",
    "target_version": "1.1.0",
    "source_version": "1.0.0",
    "hardware_version": "hw-v1",
    "download_path": "/var/lib/ota/downloads/update.bin",
    "installation_target": "/var/lib/ota/install-target",
    "sha256": "abc123...",
    "error_code": "",
    "error_message": "",
    "started_at": "2026-09-03T12:00:00Z",
    "updated_at": "2026-09-03T12:05:00Z",
    "owner_pid": 12345
}
```

### Atomic Writes

State persistence uses atomic rename:

1. Write to `transaction.json.tmp`
2. Flush to disk
3. Rename to `transaction.json`

This prevents corruption from interrupted writes.

## Concurrency Protection

File locking via `fcntl()` prevents concurrent OTA transactions:

```cpp
tm.acquire_lock();  // Blocks if another transaction is active
// ... perform transaction ...
tm.release_lock();
```

Lock is automatically released on process exit or crash.

## Crash Recovery

On restart, the client detects incomplete transactions:

```cpp
if (tm.has_incomplete_transaction()) {
    std::string msg = tm.detect_incomplete_state();
    // Report: "Previous OTA transaction detected. State: INSTALLING. Recovery required."
}
```

**Critical rule**: `INSTALLING` state after restart does NOT mean installation succeeded. It means the process was interrupted during installation.

## Transaction History

Completed transactions are recorded in `/var/lib/ota/state/history/`:

```json
{
    "transaction_id": "550e8400-...",
    "version": "1.1.0",
    "result": "SUCCESS",
    "started_at": "2026-09-03T12:00:00Z",
    "completed_at": "2026-09-03T12:05:00Z",
    "sha256": "abc123..."
}
```

History is bounded (default: 10 entries, configurable).

## CLI Commands

### Status

```bash
ota-cli status
```

Output:

```
OTA Transaction Status

Transaction ID : 550e8400-...
State          : INSTALLING
Target Version : 1.1.0
Source Version : 1.0.0
Hardware       : hw-v1
Started        : 2026-09-03T12:00:00Z
Updated        : 2026-09-03T12:05:00Z
```

### History

```bash
ota-cli history
```

Output:

```
OTA Transaction History

1. 1.1.0   SUCCESS (2026-09-03T12:05:00Z)
2. 1.0.5   SUCCESS (2026-09-02T10:00:00Z)
3. 1.0.4   FAILED (2026-09-01T08:00:00Z)
```

## File Layout

```
/var/lib/ota/
    ota.lock                    # Concurrency lock file
    state/
        transaction.json        # Current transaction state
        transaction.json.tmp    # Atomic write staging
        history/
            <uuid>.json         # Historical transaction records
```

## Error Codes

| Code | Description |
|---|---|
| NETWORK_ERROR | Download failed |
| SIGNATURE_INVALID | Digital signature verification failed |
| HASH_MISMATCH | SHA-256 integrity check failed |
| INSTALL_FAILED | Installation to target failed |
| ABORTED | Transaction aborted by user |

## Relationship to Future Features

This transaction system provides the foundation for:

- **A/B slot management**: Track which slot is being updated
- **Boot confirmation**: Confirm successful boot after installation
- **Health checks**: Monitor device health after update
- **Rollback**: Revert to previous version if update fails
- **Recovery**: Handle interrupted updates gracefully
