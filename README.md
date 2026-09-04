# Embedded Linux OTA Update & Recovery System

A secure and reliable OTA (Over-The-Air) update system for embedded Linux devices.

## Overview

This system provides:
- Secure update delivery over HTTPS
- Cryptographic integrity verification (SHA-256)
- Cryptographic authenticity verification (digital signatures)
- Transaction management with atomic state persistence
- Concurrency protection via file locking
- Crash recovery detection
- A/B slot architecture for safe updates (filesystem-backed simulation)
- Slot validation and integrity checking
- Boot control abstraction layer (simulated backend)
- Persistent state management

### What Is Implemented (Tasks 1–11)

- Device foundation (configuration, state, logging, systemd service)
- OTA server (Python HTTP server, release management)
- OTA client communication (HTTP/HTTPS transport, download, update manager)
- SHA-256 integrity verification
- Digital signature verification (ECDSA P-256)
- Secure installation (staging, atomic finalization)
- Transaction management (state machine, persistence, history)
- A/B slot management (filesystem-backed slots)
- Boot control abstraction (simulated backend)
- Bootloader interface abstraction (simulated bootloader)

### What Is NOT Implemented Yet

- Real bootloader integration (U-Boot, GRUB, systemd-boot)
- Real reboot execution
- Health checks after boot
- Automatic rollback on failure
- Boot confirmation
- Real embedded storage (eMMC/NAND/SD partitions)

## Architecture

### CORRECT ARCHITECTURE — TASKS 1–11

```text
                         OTA UPDATE SERVER
                    ┌─────────────────────────┐
                    │                         │
                    │   Release Repository    │
                    │                         │
                    │   ├── image.bin         │
                    │   ├── metadata.json     │
                    │   └── signature         │
                    │                         │
                    │   Update API            │
                    │   Image Download API    │
                    │                         │
                    └────────────┬────────────┘
                                 │
                                 │ HTTPS
                                 │
                                 │
                                 ▼
┌───────────────────────────────────────────────────────────────┐
│                    EMBEDDED LINUX DEVICE                     │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │                      OTA CLIENT                          │  │
│  │                                                         │  │
│  │  ┌─────────────────────┐                                │  │
│  │  │   Update Manager    │                                │  │
│  │  └──────────┬──────────┘                                │  │
│  │             │                                           │  │
│  │             ▼                                           │  │
│  │  ┌─────────────────────┐                                │  │
│  │  │ Communication Layer │                                │  │
│  │  │      HTTPS/TLS      │                                │  │
│  │  └──────────┬──────────┘                                │  │
│  │             │                                           │  │
│  │             ▼                                           │  │
│  │  ┌─────────────────────┐                                │  │
│  │  │ Download Manager    │                                │  │
│  │  └──────────┬──────────┘                                │  │
│  │             │                                           │  │
│  │             ▼                                           │  │
│  │       Download Storage                                 │  │
│  │       /var/lib/ota/downloads/                          │  │
│  │             │                                           │  │
│  │             ▼                                           │  │
│  │  ┌─────────────────────┐                                │  │
│  │  │ SHA-256 Verification│                                │  │
│  │  │    Integrity Check  │                                │  │
│  │  └──────────┬──────────┘                                │  │
│  │             │                                           │  │
│  │             ▼                                           │  │
│  │  ┌─────────────────────┐                                │  │
│  │  │ Signature           │                                │  │
│  │  │ Verification        │                                │  │
│  │  │ Authenticity Check  │                                │  │
│  │  └──────────┬──────────┘                                │  │
│  │             │                                           │  │
│  │             ▼                                           │  │
│  │  ┌─────────────────────┐                                │  │
│  │  │ Update Installer    │                                │  │
│  │  └──────────┬──────────┘                                │  │
│  │             │                                           │  │
│  │             ▼                                           │  │
│  │  ┌─────────────────────┐                                │  │
│  │  │ Transaction / State │                                │  │
│  │  │ Manager             │                                │  │
│  │  └──────────┬──────────┘                                │  │
│  │             │                                           │  │
│  │             ▼                                           │  │
│  │  ┌─────────────────────────────────────────────────┐    │  │
│  │  │                 A/B SLOT MANAGER                │    │  │
│  │  │                                                 │    │  │
│  │  │       ┌─────────────┐   ┌─────────────┐         │    │  │
│  │  │       │   SLOT A    │   │   SLOT B    │         │    │  │
│  │  │       │             │   │             │         │    │  │
│  │  │       │   System A  │   │   System B  │         │    │  │
│  │  │       └─────────────┘   └─────────────┘         │    │  │
│  │  └───────────────────────┬─────────────────────────┘    │  │
│  │                          │                              │  │
│  │                          ▼                              │  │
│  │               ┌─────────────────────┐                  │  │
│  │               │    BOOT CONTROL     │                  │  │
│  │               │                     │                  │  │
│  │               │ Current Slot        │                  │  │
│  │               │ Next Boot Slot      │                  │  │
│  │               │ Boot Attempts       │                  │  │
│  │               └──────────┬──────────┘                  │  │
│  │                          │                              │  │
│  │                          ▼                              │  │
│  │               ┌─────────────────────┐                  │  │
│  │               │ BOOTLOADER INTERFACE │                  │  │
│  │               │                     │                  │  │
│  │               │ get_current_slot()  │                  │  │
│  │               │ set_next_boot_slot()│                  │  │
│  │               │ mark_boot_started() │                  │  │
│  │               │ boot_attempts       │                  │  │
│  │               └──────────┬──────────┘                  │  │
│  │                          │                              │  │
│  │                          ▼                              │  │
│  │               ┌─────────────────────┐                  │  │
│  │               │ SIMULATED BOOTLOADER │                  │  │
│  │               │                     │                  │  │
│  │               │ Filesystem-backed   │                  │  │
│  │               │ JSON persistence    │                  │  │
│  │               │ Atomic writes       │                  │  │
│  │               │                     │                  │  │
│  │               │ CURRENT IMPLEMENTATION                │  │
│  │               └─────────────────────┘                  │  │
│  │                                                         │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  Persistent Data                                               │
│  ├── /var/lib/ota/state/                                      │
│  ├── /var/lib/ota/downloads/                                 │
│  ├── /var/lib/ota/staging/                                   │
│  ├── /var/lib/ota/slots/                                     │
│  ├── /var/lib/ota/boot/                                      │
│  └── /var/lib/ota/bootloader/                                │
│                                                               │
└───────────────────────────────────────────────────────────────┘
```

### IMPORTANT COMMUNICATION DIRECTION

The diagram MUST clearly show that the **Embedded Linux device is the OTA client**.

The device initiates communication:

```text
Embedded Linux Device
        │
        │ HTTPS request
        ▼
    OTA Server
        │
        │ HTTPS response
        ▼
Embedded Linux Device
```

For example:

```text
DEVICE ────── check for update ──────► SERVER
DEVICE ◄───── update metadata ─────── SERVER

DEVICE ────── download request ──────► SERVER
DEVICE ◄──────── image.bin ─────────── SERVER
```

Do NOT draw the architecture as:

```text
SERVER ───── HTTP ─────► DEVICE
```

because that incorrectly suggests that the OTA server directly initiates the update.

The OTA client on the Embedded Linux device controls the OTA process.

---

## TASK 11 LIMITATION

The diagram MUST stop at the functionality that actually exists after Task 11.

Do NOT include these as implemented:

* real bootloader
* U-Boot integration
* GRUB integration
* real reboot
* first-boot detection
* health-check system
* boot confirmation
* automatic rollback

Those belong to later tasks.

Task 11 provides:

```text
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

CURRENT IMPLEMENTATION → Simulated Bootloader
FUTURE → Real U-Boot / GRUB integration

---

## IMPORTANT ARCHITECTURE DISTINCTION

Keep the two levels clear.

### Network architecture

```text
┌─────────────────────┐
│ Embedded Linux      │
│ OTA Client          │
└──────────┬──────────┘
           │
           │ HTTPS
           ▼
┌─────────────────────┐
│ OTA Server          │
│                     │
│ Release Repository  │
│ Update API          │
│ Image API           │
└─────────────────────┘
```

### Internal device architecture

```text
OTA Client
    │
    ├── Update Manager
    ├── Communication Layer
    ├── Download Manager
    ├── SHA-256 Verification
    ├── Signature Verification
    ├── Update Installer
    ├── Transaction / State Manager
    ├── A/B Slot Manager
    │       ├── Slot A
    │       └── Slot B
    │
    ├── Boot Control
    │       └── (uses Bootloader Interface)
    │
    └── Bootloader Interface
            └── Simulated Bootloader (CURRENT)
            └── Real U-Boot / GRUB (FUTURE)
```

Both diagrams may be included in the README if useful.

---

## README QUALITY REQUIREMENTS

Make the diagram:

* technically accurate
* clean
* professional
* easy to understand
* suitable for a GitHub embedded-systems project
* consistent with Tasks 1–10
* consistent with the actual source code

Do not add components merely to make the architecture look more advanced.

Do not introduce:

* Docker
* Kubernetes
* cloud infrastructure
* databases
* AI
* microservices
* unnecessary networking components

unless they actually exist in the implementation.

After updating the README, verify the diagram against the source code and Task 1–10 documentation.

Do not modify implementation code.

Do not start Task 11.

Stop after fixing and validating the README architecture diagram.

```
├── src/
│   ├── client/              # OTA client logic
│   ├── network/             # HTTP/HTTPS communication
│   ├── download/            # Download management
│   ├── device/              # Device configuration
│   ├── logging/             # Logging infrastructure
│   ├── validation/          # Integrity verification
│   ├── security/            # Digital signature verification
│   ├── installation/        # Update installation
│   ├── transaction/         # Transaction management
│   ├── slot/                # A/B slot management
│   ├── boot/                # Boot control abstraction
│   │   ├── simulated_boot_control.cpp
│   │   └── bootloader/      # Bootloader interface
│   │       └── simulated_bootloader.cpp
│   └── main.cpp             # Client entry point
├── include/
│   ├── client/
│   ├── network/
│   ├── download/
│   ├── device/
│   ├── logging/
│   ├── validation/
│   ├── security/
│   ├── installation/
│   ├── transaction/
│   ├── slot/
│   └── boot/
│       ├── boot_control.h
│       ├── simulated_boot_control.h
│       └── bootloader/      # Bootloader interface
│           ├── bootloader.hpp
│           └── simulated_bootloader.hpp
├── tests/                   # Unit and integration tests
├── ota-server/              # OTA server
├── configs/                 # Configuration files
├── scripts/                 # Setup and signing scripts
├── keys/                    # Signing keys (private keys protected)
├── systemd/                 # Service files
├── docs/                    # Documentation
└── CMakeLists.txt           # Build configuration
```

## Quick Start

### Build Client

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Test Client

```bash
./build/tests/ota_tests  # 368 tests
```

### Run Client

```bash
# Check for updates
./build/ota-cli check -c configs/device.conf -u http://localhost:8080

# Download update
./build/ota-cli download -c configs/device.conf -u http://localhost:8080

# Verify image integrity
./build/ota-cli verify -i /path/to/image.bin -e <expected-sha256>

# Verify release authenticity
./build/ota-cli verify-signature -r /path/to/release/ -k /etc/ota/trusted/ota-signing.pub

# Install validated update
./build/ota-cli install -i /path/to/image.bin -v <version> -e <expected-sha256>

# Check transaction status
./build/ota-cli status

# View transaction history
./build/ota-cli history

# Initialize A/B slot system
./build/ota-cli slots init

# View A/B slot status
./build/ota-cli slots

# View boot control status
./build/ota-cli boot status

# Set next boot slot
./build/ota-cli boot set B

# Clear pending boot selection
./build/ota-cli boot clear

# Simulate boot cycle
./build/ota-cli boot simulate

# View bootloader status
./build/ota-cli bootloader status

# Set next boot via bootloader
./build/ota-cli bootloader set-next B

# Clear pending boot via bootloader
./build/ota-cli bootloader clear-next

# Simulate boot via bootloader
./build/ota-cli bootloader simulate

# View boot attempt counts
./build/ota-cli bootloader attempts
```

### OTA Server

```bash
# Create a release
cd ota-server
./scripts/create_release.py 1.1.0 /path/to/image.bin

# Sign the release
./scripts/sign-release /path/to/release/

# Start server
./server/ota_server.py --port 8080

# Query for updates
curl "http://localhost:8080/api/v1/update?device_id=device-001&hardware_version=revA&current_version=1.0.0"

# Download image
curl -O http://localhost:8080/releases/1.1.0/image.bin
```

### Key Management

```bash
# Generate development signing keys
./scripts/generate-signing-key

# Install trusted public key on device
sudo mkdir -p /etc/ota/trusted
sudo cp keys/development/public/ota-signing.pub /etc/ota/trusted/
```

### Test Server

```bash
cd ota-server
python -m pytest tests/ -v
```

### Install Service

```bash
sudo scripts/setup-user.sh
sudo scripts/install.sh
sudo systemctl start ota-client
```

## Documentation

- [Requirements](docs/requirements.md) - Full system requirements
- [Device Foundation](docs/device-foundation.md) - Device architecture
- [OTA Server](ota-server/README.md) - Server documentation
- [OTA Client Communication](docs/ota-client-communication.md) - Communication layer
- [Integrity Verification](docs/integrity-verification.md) - Integrity verification system
- [Digital Signatures](docs/digital-signatures.md) - Digital signature verification
- [Update Installation](docs/update-installation.md) - Secure update installation
- [Transaction Management](docs/transaction-management.md) - OTA transaction lifecycle
- [A/B Slot Architecture](docs/ab-slot-architecture.md) - A/B slot management
- [Boot Control Abstraction](docs/boot-control.md) - Boot control abstraction layer
- [Bootloader Integration](docs/bootloader-integration.md) - Bootloader interface and simulated bootloader
- [Testing - Task 02](docs/testing/task-02.md) - Client test results
- [Testing - Task 03](docs/testing/task-03.md) - Server test results
- [Testing - Task 04](docs/testing/task-04.md) - Communication layer test results
- [Testing - Task 05](docs/testing/task-05.md) - Integrity verification test results
- [Testing - Task 06](docs/testing/task-06.md) - Digital signature test results
- [Testing - Task 07](docs/testing/task-07.md) - Installation test results
- [Testing - Task 08](docs/testing/task-08.md) - Transaction management test results
- [Testing - Task 09](docs/testing/task-09.md) - A/B slot architecture test results
- [Testing - Task 10](docs/testing/task-10.md) - Boot control test results
- [Testing - Task 11](docs/testing/task-11.md) - Bootloader interface test results
- [Difficulties](docs/difficulties.md) - Implementation challenges

## Technology Stack

| Component | Technology |
|-----------|------------|
| Client Language | C++17 |
| Build | CMake |
| Testing | Google Test |
| HTTP Library | libcurl |
| Crypto Library | OpenSSL |
| Server Language | Python 3 |
| Server Testing | pytest |
| Service | systemd |

## License

Proprietary
