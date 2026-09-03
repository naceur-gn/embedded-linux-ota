# Embedded Linux OTA Update & Recovery System — Requirements Specification

**Version:** 1.0  
**Date:** 2026-09-03  
**Status:** Task 1 — Architecture & Specification

---

## 1. Project Objective

Build a secure and reliable OTA (Over-The-Air) update system capable of updating an Embedded Linux device while protecting the device from corrupted, unauthorized, interrupted, or failed updates.

The system must support the complete update lifecycle: check, download, verify integrity, verify authenticity, verify compatibility, install safely, reboot, detect new version, perform health check, confirm update, and rollback if the update fails.

**Priority order:**
1. Security
2. Reliability
3. Recovery
4. Correctness
5. Testability
6. Maintainability

---

## 2. Scope

### 2.1 In Scope

- OTA server serving signed update images and metadata
- OTA client running on the embedded Linux device
- HTTPS/TLS transport between server and client
- Cryptographic integrity verification (SHA-256)
- Cryptographic authenticity verification (digital signatures)
- Version and compatibility validation
- A/B partition update mechanism
- Safe installation to inactive slot
- Reboot and boot management
- Post-boot health checks
- Automatic rollback on failure
- Persistent state management across reboots
- State machine governing update lifecycle
- Automated test suite covering normal and failure scenarios

### 2.2 Explicitly Out of Scope

The following technologies are **not** to be introduced unless a future task explicitly requires them:

| Technology | Reason Excluded |
|---|---|
| AI / Machine Learning | Not required for OTA functionality |
| React | No web dashboard in initial scope |
| Kubernetes | No container orchestration needed |
| Docker | No containerization needed |
| Cloud infrastructure | Server is standalone, not cloud-managed |
| Microservices | Monolithic server is sufficient |
| ESP32 / STM32 | Target is Linux-based systems only |
| IoT platforms | Not an IoT fleet management system |
| Complicated databases | Flat files or SQLite suffice |
| Web dashboards | CLI-based management only initially |
| Yocto | May be reconsidered later for image generation; not required for initial development |

---

## 3. System Architecture

### 3.1 High-Level Architecture Diagram

```
                    ┌─────────────────────────┐
                    │       OTA SERVER        │
                    │                         │
                    │  Release Management     │
                    │  Metadata Store         │
                    │  Signed Image Storage   │
                    │  HTTPS Endpoint         │
                    └────────────┬────────────┘
                                 │
                                HTTPS
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │       OTA CLIENT        │
                    │                         │
                    │  Update Manager         │
                    │  Version Manager        │
                    │  Download Manager       │
                    │  Validator              │
                    │  Update Engine          │
                    │  State Manager          │
                    └────────────┬────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │      A/B SYSTEM         │
                    │                         │
                    │    SLOT A   │  SLOT B   │
                    │  (active)  │ (inactive) │
                    └────────────┬────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │   BOOT / RECOVERY       │
                    │                         │
                    │  Boot Slot Selection    │
                    │  Health Monitoring      │
                    │  Rollback Execution     │
                    └─────────────────────────┘
```

### 3.2 Component Descriptions

#### OTA Server

| Component | Responsibility |
|---|---|
| Release Management | Maintains version history, release notes, and compatibility metadata |
| Metadata Store | Stores release metadata: version, hardware compatibility, hashes, signatures |
| Signed Image Storage | Stores cryptographically signed update images |
| HTTPS Endpoint | Serves metadata and images over TLS |

#### OTA Client

| Component | Responsibility |
|---|---|
| Update Manager | Orchestrates the entire update lifecycle |
| Version Manager | Tracks current version, compares versions, checks compatibility |
| Download Manager | Handles HTTPS downloads with resume support and integrity checks |
| Validator | Verifies SHA-256 hash and digital signature of downloaded images |
| Update Engine | Writes update to inactive A/B slot |
| State Manager | Persists and manages update state across reboots |

#### A/B System

| Component | Responsibility |
|---|---|
| Slot A / Slot B | Two root filesystem partitions; one active, one inactive |
| Active Slot | Currently booted and running system |
| Inactive Slot | Target for new update installation |

#### Boot / Recovery

| Component | Responsibility |
|---|---|
| Boot Slot Selection | Selects which slot to boot based on state |
| Health Monitoring | Runs health checks after boot on new version |
| Rollback Execution | Switches back to previous slot if new version fails |

### 3.3 Update Lifecycle Flow

```
START
  │
  ▼
Check for update ──────(no update)──────► END
  │
  ▼ (update available)
Download update
  │
  ▼
Verify integrity (SHA-256)
  │
  ▼
Verify authenticity (digital signature)
  │
  ▼
Verify compatibility (version, hardware)
  │
  ▼
Install to inactive slot
  │
  ▼
Set pending reboot state
  │
  ▼
Reboot
  │
  ▼
Boot new version
  │
  ▼
Health check
  │
  ├──(PASS)──► Confirm/commit update ──► END (success)
  │
  └──(FAIL)──► Rollback to previous slot ──► Reboot ──► END (recovered)
```

---

## 4. Security Requirements

### 4.1 Transport Security

| ID | Requirement |
|---|---|
| SEC-01 | All communication between OTA client and OTA server MUST use HTTPS (TLS 1.2 or higher) |
| SEC-02 | The client MUST validate the server's TLS certificate |
| SEC-03 | Plain HTTP MUST NOT be used for any update-related communication |

### 4.2 Integrity Verification

| ID | Requirement |
|---|---|
| SEC-04 | Every update image MUST include a SHA-256 hash in its metadata |
| SEC-05 | The client MUST compute SHA-256 of the downloaded image and compare it to the metadata hash |
| SEC-06 | The client MUST reject the update if the hash does not match |
| SEC-07 | The system MUST detect corrupted downloads, modified files, and incomplete files |
| SEC-08 | Hash verification MUST occur before any installation attempt |

### 4.3 Authenticity Verification

| ID | Requirement |
|---|---|
| SEC-09 | Every update image MUST be digitally signed by the release authority |
| SEC-10 | The client MUST verify the digital signature using an embedded public key or certificate |
| SEC-11 | The client MUST NEVER trust an update merely because it came from the OTA server |
| SEC-12 | The client MUST reject updates with invalid, missing, or expired signatures |
| SEC-13 | The signing key MUST NOT be present on the client device |

### 4.4 Version Validation

| ID | Requirement |
|---|---|
| SEC-14 | The client MUST verify the update is intended for the correct hardware/device type |
| SEC-15 | The client MUST verify the software version is appropriate (not downgrade unless explicitly allowed) |
| SEC-16 | The metadata MUST include minimum supported version constraints |
| SEC-17 | The client MUST reject incompatible updates before downloading the full image when possible |

---

## 5. Reliability Requirements

### 5.1 Safe Installation

| ID | Requirement |
|---|---|
| REL-01 | The current working system MUST NEVER be destroyed before the new system has been validated |
| REL-02 | Updates MUST be installed to the inactive A/B slot only |
| REL-03 | The active slot MUST remain untouched and bootable during installation |
| REL-04 | A failed update MUST NOT permanently brick the device |

### 5.2 A/B Update Mechanism

| ID | Requirement |
|---|---|
| REL-05 | The system MUST maintain two bootable slots (A and B) |
| REL-06 | One slot is always active and running; the other is inactive and available for updates |
| REL-07 | Writing to the inactive slot MUST NOT affect the active slot |
| REL-08 | After reboot, the system MUST boot from the slot containing the new version |
| REL-09 | If the new version fails health checks, the system MUST automatically revert to the previous slot |

### 5.3 Power Loss Protection

| ID | Requirement |
|---|---|
| REL-10 | Power loss during download MUST NOT affect the current running system |
| REL-11 | Power loss during installation to inactive slot MUST leave at least one slot bootable |
| REL-12 | Power loss after reboot into new version MUST either complete health check or trigger rollback |
| REL-13 | The system MUST recover gracefully from power loss at any point in the update cycle |

### 5.4 State Persistence

| ID | Requirement |
|---|---|
| REL-14 | Update state MUST persist across reboots |
| REL-15 | The state MUST accurately reflect the current stage of the update process |
| REL-16 | On boot, the system MUST check persistent state and take appropriate action (continue, rollback, or proceed normally) |

---

## 6. Persistent State

The system MUST maintain the following persistent state:

| Field | Type | Description |
|---|---|---|
| `current_version` | string | Version of the currently running system |
| `active_slot` | enum (A/B) | Which slot is currently active and booted |
| `pending_slot` | enum (A/B/null) | Slot being written to for pending update |
| `pending_version` | string/null | Version being installed |
| `update_state` | enum | Current state in the update state machine |
| `boot_attempts` | integer | Number of attempts to boot and confirm the new version |
| `rollback_reason` | string/null | Reason for rollback if one occurred |

The exact storage mechanism (file, partition, etc.) will be defined in a later implementation task.

---

## 7. Update State Machine

### 7.1 States

| State | Description |
|---|---|
| `IDLE` | No update in progress; system is running normally |
| `CHECKING` | Client is querying server for available updates |
| `DOWNLOADING` | Update image is being downloaded |
| `VERIFYING` | Downloaded image is being verified (integrity + authenticity + compatibility) |
| `INSTALLING` | Verified image is being written to inactive slot |
| `PENDING_REBOOT` | Installation complete; waiting for reboot |
| `REBOOTING` | System is rebooting into new version |
| `HEALTH_CHECK` | New version has booted; health checks are running |
| `SUCCESS` | Health check passed; update is good |
| `CONFIRMED` | Update committed; new version is now the current version |
| `FAILURE` | Health check failed or error occurred |
| `ROLLBACK` | System is reverting to previous slot |
| `RECOVERY` | System has rolled back and is in a stable state |

### 7.2 Valid Transitions

```
IDLE ──────────────────► CHECKING
CHECKING ──────────────► DOWNLOADING (update available)
CHECKING ──────────────► IDLE (no update available)
DOWNLOADING ───────────► VERIFYING (download complete)
DOWNLOADING ───────────► IDLE (download failed)
VERIFYING ─────────────► INSTALLING (verification passed)
VERIFYING ─────────────► IDLE (verification failed)
INSTALLING ────────────► PENDING_REBOOT (installation complete)
INSTALLING ────────────► FAILURE (installation failed)
PENDING_REBOOT ────────► REBOOTING
REBOOTING ─────────────► HEALTH_CHECK
HEALTH_CHECK ──────────► SUCCESS (health check passed)
HEALTH_CHECK ──────────► FAILURE (health check failed)
SUCCESS ───────────────► CONFIRMED
CONFIRMED ─────────────► IDLE
FAILURE ───────────────► ROLLBACK
ROLLBACK ──────────────► RECOVERY
RECOVERY ──────────────► IDLE
```

### 7.3 Transition Rules

- Invalid transitions MUST be rejected
- The state machine MUST be the single source of truth for what actions are permitted
- Each transition MUST be logged with a timestamp
- The state MUST be persisted to non-volatile storage before any destructive action

---

## 8. Failure Scenarios

The system MUST handle the following failure scenarios:

### 8.1 Scenario Matrix

| # | Scenario | Expected Behavior |
|---|---|---|
| F-01 | Normal update (happy path) | Update downloads, verifies, installs, reboots, passes health check, commits |
| F-02 | Corrupted update image | Hash mismatch detected during verification; update rejected; stays on current version |
| F-03 | Invalid digital signature | Signature verification fails; update rejected; stays on current version |
| F-04 | Interrupted download | Download resumes or restarts; no effect on current system |
| F-05 | Network failure during download | Download pauses/retries; no effect on current system |
| F-06 | Incompatible version | Compatibility check fails before download; update rejected |
| F-07 | Installation failure (disk error, etc.) | Installation aborted; active slot untouched; system remains on current version |
| F-08 | Power loss during update to inactive slot | On recovery, at least one slot is bootable; state machine determines correct action |
| F-09 | New version fails to boot | Bootloader/health monitor detects failure; automatic rollback to previous slot |
| F-10 | New version boots but fails health checks | Health check fails; automatic rollback triggered |
| F-11 | Automatic rollback | System reverts to previous slot; previous version resumes normal operation |
| F-12 | Successful confirmation | New version confirmed as healthy; becomes the current version; previous slot available for next update |

### 8.2 Recovery Guarantees

- The device MUST be recoverable from any single failure
- The device MUST NOT be bricked by any combination of a single failure
- Rollback MUST restore the device to a known-good state
- After rollback, the device MUST be capable of attempting another update

---

## 9. Technology Choices

### 9.1 Initial Stack

| Component | Technology | Rationale |
|---|---|---|
| Operating System | Linux | Target platform for embedded devices |
| Programming Language | C/C++ | Direct hardware access, small footprint, no runtime dependencies |
| Init System | systemd | Standard on modern Linux; provides service management and journaling |
| Cryptography | OpenSSL | Industry-standard; provides SHA-256, TLS, and digital signature verification |
| Hash Algorithm | SHA-256 | Sufficient for integrity checking; widely supported |
| Transport | HTTPS (TLS 1.2+) | Encrypted and authenticated communication |
| Storage | Filesystem | Simple, sufficient for A/B slots and state |
| Update Mechanism | A/B partitions | Proven approach used by Android, ChromeOS, and many embedded systems |
| Testing | Automated test suite | Required for correctness and regression prevention |

### 9.2 Dependency Justification Policy

For every dependency introduced in future tasks, the following must be documented:

1. **Why it is required** — What specific problem does it solve?
2. **What problem it solves** — Which requirement does it fulfill?
3. **Why a simpler alternative is insufficient** — Why can we not solve this with existing dependencies?

---

## 10. Non-Goals

The following are explicitly **not** goals of this project:

| Non-Goal | Reason |
|---|---|
| AI-powered update scheduling | Unnecessary complexity |
| Web-based management dashboard | CLI-based management is sufficient initially |
| Fleet management for thousands of devices | Single-device update system is the focus |
| Container-based deployments | Not applicable to embedded Linux |
| Cloud-hosted infrastructure | Server is self-contained |
| Support for non-Linux platforms | Target is exclusively embedded Linux |
| Real-time update streaming | Complete image download before installation |
| Delta/differential updates | Full image updates are simpler and more reliable initially |
| Multi-device orchestration | Each device manages its own updates independently |

---

## 11. Future Optional Components

These components are **not** part of the initial implementation but may be considered in later tasks:

| Component | Condition for Consideration |
|---|---|
| Yocto-based image generation | If realistic embedded Linux image building becomes necessary |
| Delta updates | If bandwidth or storage constraints demand it |
| Encrypted update images | If intellectual property protection becomes a requirement |
| Secure boot integration | If hardware root of trust is available |
| Hardware-backed key storage | If TPM or secure element is available |
| Rollback counters / wear leveling | If flash memory endurance becomes a concern |
| Multi-partition redundancy | If higher availability is required |
| Update scheduling / maintenance windows | If operational requirements demand it |

---

## 12. Acceptance Criteria

Task 1 is complete when this document answers all of the following:

| # | Question | Answered By |
|---|---|---|
| 1 | What exactly are we building? | Section 1 — Project Objective |
| 2 | What exactly is being updated? | Section 3.2 — A/B System (root filesystem partitions) |
| 3 | How is an update downloaded? | Section 3.3 — Update Lifecycle; Section 9 — HTTPS |
| 4 | How is integrity verified? | Section 4.2 — SHA-256 hash verification |
| 5 | How is authenticity verified? | Section 4.3 — Digital signature verification |
| 6 | How is the update installed safely? | Section 5.1 — Write to inactive slot only |
| 7 | How does A/B updating work? | Section 5.2 — Two slots; one active, one inactive |
| 8 | What happens after reboot? | Section 3.3 — Boot new version → health check |
| 9 | How do we know the new version is healthy? | Section 5.2 — Health checks; automatic rollback on failure |
| 10 | How does rollback work? | Section 5.2 — Revert to previous slot; reboot |
| 11 | What happens if power is lost? | Section 5.3 — At least one slot always bootable |
| 12 | What happens if the image is malicious or corrupted? | Section 4.2–4.3 — Hash and signature verification reject it |
| 13 | What technologies are necessary? | Section 9.1 — Initial Stack |
| 14 | What technologies are intentionally excluded? | Section 2.2 + Section 10 |
| 15 | How will we test the system? | Section 8 — Failure scenario matrix; automated test suite |

---

## 13. Appendix: Directory Structure (Future Reference)

The final repository will eventually follow this structure. **Do NOT implement this in Task 1.**

```
src/
include/
tests/
server/
configs/
scripts/
docs/
CMakeLists.txt
README.md
```

---

*This document is the authoritative specification for the Embedded Linux OTA Update & Recovery System. All future implementation tasks MUST conform to this specification unless an explicit amendment is made.*
