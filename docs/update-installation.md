# Secure OTA Update Installation

## Overview

This document describes the secure update installation layer for OTA updates. The installation system takes validated updates and safely installs them into a designated target location without modifying the active system.

## Architecture

```
                 OTA CLIENT
                     │
                     ▼
             Update Validator
                     │
              ┌──────┴──────┐
              │             │
          SHA-256        Signature
          VALID           VALID
              │             │
              └──────┬──────┘
                     ▼
             INSTALL MANAGER
                     │
                     ▼
             Prepare Target
                     │
                     ▼
              Install Image
                     │
                     ▼
             Verify Installation
                     │
                     ▼
              INSTALL SUCCESS
```

## Security Boundary

The installer only operates on updates that have been explicitly validated:

```
Downloaded Artifact
        │
        ▼
Integrity Verification
        │
        ▼
Authenticity Verification
        │
        ▼
Installation Authorization
        │
        ▼
Installer
```

The installer will NOT:
- Accept arbitrary files without validation
- Modify the active system
- Skip verification steps
- Mark incomplete installations as successful

## Directory Structure

```
/var/lib/ota/
├── downloads/          # Downloaded artifacts
├── staging/            # Temporary staging area
│   └── <version>/      # Version-specific staging
│       └── image.bin
├── install-target/     # Final installation target
│   └── <version>/      # Version-specific target
│       └── image.bin
└── state/              # Persistent state
    └── installation_<version>.json
```

## Installation Workflow

### 1. Receive Validated Update

The installer receives an `InstallInfo` struct containing:
- Version string
- Image path
- Expected SHA-256 hash
- Expected file size

### 2. Validate Prerequisites

Before installation:
- Validate version string (no path traversal)
- Check image file exists and is regular file
- Verify sufficient disk space
- Check permissions

### 3. Create Staging Directory

Create a temporary staging directory:
```
/var/lib/ota/staging/<version>/
```

### 4. Copy to Staging

Copy the validated image to the staging directory.

### 5. Verify Staged Artifact

Calculate SHA-256 of staged file and compare with expected hash.

### 6. Finalize Installation

Move the staged artifact to the installation target:
```
/var/lib/ota/install-target/<version>/image.bin
```

### 7. Synchronize Filesystem

Flush data to disk to ensure persistence.

### 8. Post-Installation Verification

Recalculate SHA-256 of installed artifact and verify.

### 9. Record Installation State

Save installation state to persistent storage.

## Security Controls

### Path Traversal Prevention

The installer rejects version strings containing:
- `..` (relative path traversal)
- `/` at the beginning (absolute paths)
- `//` (double slashes)

```cpp
bool validate_path(const std::string& path);
bool is_traversal_attempt(const std::string& path);
```

### Symlink Attack Handling

The installer checks for symbolic links and handles them safely.

### Disk Space Validation

Before installation, the installer checks available disk space:
- Required space = image size × 2 (for staging + target)
- Additional margin = configurable minimum free space

### Atomic Installation

The installation uses atomic operations where possible:
1. Stage in temporary directory
2. Verify staged artifact
3. Atomic move to final location

### Post-Installation Verification

After installation, the installed artifact is verified:
- File exists
- SHA-256 matches expected value
- Correct permissions

## Installation States

```cpp
enum class InstallStatus {
    NOT_READY,           // Initial state
    READY,               // Ready to install
    INSTALLING,          // Installation in progress
    INSTALLED,           // Installation successful
    VERIFICATION_FAILED, // Post-install verification failed
    INSTALLATION_FAILED, // Installation failed
    INSUFFICIENT_SPACE,  // Not enough disk space
    PERMISSION_DENIED,   // Insufficient permissions
    PATH_TRAVERSAL_DETECTED, // Malicious path detected
    INVALID_ARTIFACT,    // Invalid update artifact
    STAGING_FAILED       // Staging operation failed
};
```

## Installation State Persistence

Installation state is saved to:
```
/var/lib/ota/state/installation_<version>.json
```

Example:
```json
{
    "version": "1.1.0",
    "status": "installed",
    "sha256": "abc123...",
    "installed_at": "1234567890",
    "target": "/var/lib/ota/install-target/1.1.0"
}
```

## Crash Safety

If the installation is interrupted:

1. **During staging**: Staging directory contains incomplete data
2. **During copy**: Staging has partial file, target untouched
3. **During verification**: Installation not marked as successful
4. **After verification**: Installation complete

On next execution:
- Incomplete staging can be cleaned up
- No false success state is reported
- Active system remains unchanged

## Cleanup

### After Successful Installation
- Remove staging directory
- Preserve installation target
- Preserve installation state

### After Failed Installation
- Remove staging directory
- Preserve diagnostic information
- Preserve active system
- Record failure reason

## Permissions

The OTA service runs using the dedicated OTA user:
- Downloads: OTA user
- Verification: OTA user
- Staging: OTA user
- Installation target: OTA user (with appropriate permissions)

### Directory Permissions
```
staging/          0755  OTA user
install-target/   0755  OTA user
state/            0755  OTA user
```

## CLI Usage

### Install Command

```bash
ota-cli install -i <image-path> -v <version> -e <expected-sha256>
```

Options:
- `-i, --image PATH` - Image file to install
- `-v, --version VER` - Version string
- `-e, --expected-hash HEX` - Expected SHA-256 hash
- `-s, --state-dir PATH` - State directory (default: /var/lib/ota)

### Example

```bash
# Validate update first
ota-cli verify -i /path/to/image.bin -e <expected-hash>

# Install validated update
ota-cli install -i /path/to/image.bin -v 1.1.0 -e <expected-hash>
```

## API Reference

### InstallManager

```cpp
class InstallManager {
public:
    void set_config(const InstallConfig& config);

    InstallResult install(const InstallInfo& info,
                         InstallProgressCallback progress = nullptr);

    bool check_disk_space(int64_t required_bytes);

    bool validate_path(const std::string& path);

    bool is_safe_path(const std::string& base_dir, const std::string& path);

    std::string calculate_sha256(const std::string& file_path);

    bool sync_to_disk(const std::string& path);

    InstallationState load_installation_state(const std::string& version);

    bool save_installation_state(const InstallationState& state);

    bool cleanup_staging(const std::string& version);

    bool cleanup_install_target(const std::string& version);
};
```

### InstallConfig

```cpp
struct InstallConfig {
    std::string staging_dir;       // Default: /var/lib/ota/staging
    std::string install_target;    // Default: /var/lib/ota/install-target
    std::string state_dir;         // Default: /var/lib/ota/state
    int64_t min_free_space_mb;     // Default: 100 MB
};
```

### InstallInfo

```cpp
struct InstallInfo {
    std::string version;           // Version string
    std::string image_path;        // Path to image file
    std::string expected_sha256;   // Expected SHA-256 hash
    int64_t expected_size;         // Expected file size
};
```

### InstallResult

```cpp
struct InstallResult {
    InstallStatus status;          // Installation status
    std::string installed_path;    // Path to installed artifact
    std::string calculated_sha256; // Calculated SHA-256
    std::string error_message;     // Error message if failed

    bool is_success() const;       // Check if installation succeeded
};
```

## Testing

See [Task 07 Testing Report](testing/task-07.md) for comprehensive test results.

### Test Categories

1. **Unit Tests**: Installation logic
2. **Attack Tests**: Security attack vectors
3. **Integration Tests**: End-to-end installation flow
4. **Regression Tests**: Ensure previous functionality works

## Implementation Files

### Source Code

- `include/installation/installer.h` - Header file
- `src/installation/installer.cpp` - Implementation

### Tests

- `tests/test_installer.cpp` - Unit tests
- `tests/test_install_attack.cpp` - Attack simulation tests

## Future Enhancements

1. **A/B Installation**: Support for dual-partition updates
2. **Rollback Support**: Automatic rollback on failure
3. **Delta Updates**: Incremental update installation
4. **Encrypted Updates**: Support for encrypted update images
5. **Secure Boot Integration**: Hardware root of trust
