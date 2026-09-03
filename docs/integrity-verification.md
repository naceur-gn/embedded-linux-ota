# Integrity Verification

## Overview

The integrity verification module ensures downloaded update images have not been corrupted or tampered with during transit. It uses SHA-256 cryptographic hashing to verify image authenticity against server-provided metadata.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                 INTEGRITY VERIFICATION              │
│                                                     │
│  ┌───────────────────────────────────────────────┐  │
│  │           IntegrityValidator                  │  │
│  │                                               │  │
│  │  validate_file()                              │  │
│  │       │                                       │  │
│  │       ├──► File existence check               │  │
│  │       │                                       │  │
│  │       ├──► Size validation                    │  │
│  │       │                                       │  │
│  │       ├──► Hash format validation             │  │
│  │       │                                       │  │
│  │       └──► SHA-256 calculation (EVP API)      │  │
│  └───────────────────────────────────────────────┘  │
│                                                     │
│  ValidationResult                                   │
│       ├── status: VALID | FILE_NOT_FOUND |          │
│       │           HASH_MISMATCH | SIZE_MISMATCH |   │
│       │           INVALID_HASH_FORMAT |             │
│       │           FILE_NOT_REGULAR                  │
│       ├── expected_hash                             │
│       ├── calculated_hash                           │
│       ├── expected_size                             │
│       └── actual_size                               │
└─────────────────────────────────────────────────────┘
```

## Key Components

### IntegrityValidator

The main validation class providing:

1. **File Validation**: Complete integrity check against expected hash and size
2. **SHA-256 Calculation**: Cryptographic hash computation using OpenSSL EVP API
3. **Hash Format Validation**: Ensures expected hash is a valid 64-character hex string
4. **Size Validation**: Verifies file size matches expected metadata
5. **Incremental Streaming**: Memory-efficient processing for large files

### Validation Statuses

| Status | Description |
|--------|-------------|
| VALID | File passes all integrity checks |
| FILE_NOT_FOUND | File does not exist at specified path |
| FILE_NOT_REGULAR | Path exists but is not a regular file |
| HASH_MISMATCH | Calculated hash differs from expected |
| SIZE_MISMATCH | File size differs from expected metadata |
| INVALID_HASH_FORMAT | Expected hash is not valid 64-char hex |

## API Reference

### IntegrityValidator

```cpp
namespace ota {

class IntegrityValidator {
public:
    // Validate file against expected hash and optional size
    ValidationResult validate_file(
        const std::string& file_path,
        const std::string& expected_hash,
        int64_t expected_size = -1
    );

    // Calculate SHA-256 hash of file
    std::string calculate_sha256(const std::string& file_path);

    // Check if hash format is valid (64-char hex)
    bool is_valid_sha256_format(const std::string& hash);

    // Verify file matches expected hash
    bool verify_sha256(const std::string& file_path,
                      const std::string& expected_hash);

    // Normalize hash (lowercase, trim whitespace)
    std::string normalize_hash(const std::string& hash);
};

} // namespace ota
```

### ValidationResult

```cpp
namespace ota {

struct ValidationResult {
    ValidationStatus status;
    std::string expected_hash;
    std::string calculated_hash;
    int64_t expected_size;
    int64_t actual_size;
    std::string error_message;

    bool is_valid() const;
};

} // namespace ota
```

## Usage Examples

### Basic File Validation

```cpp
#include "validation/integrity_validator.h"

ota::IntegrityValidator validator;

// Validate file against expected hash
auto result = validator.validate_file(
    "/var/lib/ota/downloads/update.bin",
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
);

if (result.is_valid()) {
    std::cout << "Image verified successfully\n";
} else {
    std::cerr << "Verification failed: " << result.error_message << "\n";
}
```

### With Size Validation

```cpp
// Include expected size from metadata
auto result = validator.validate_file(
    "/var/lib/ota/downloads/update.bin",
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    1048576  // 1MB expected size
);

if (result.status == ota::ValidationStatus::SIZE_MISMATCH) {
    std::cerr << "File size mismatch: expected " << result.expected_size
              << ", got " << result.actual_size << "\n";
}
```

### CLI Usage

```bash
# Verify image integrity
./ota-cli verify -i /path/to/image.bin -e e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

# Output:
# Image: verified
# Expected SHA-256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
# Calculated SHA-256: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
# Expected size: -1 bytes
# Actual size: 0 bytes
#
# Integrity: VALID
```

## Security Properties

### Threat Mitigation

1. **Data Corruption**: SHA-256 detects any bit-level corruption during download
2. **Tampering**: Cryptographic hash ensures image integrity against modification
3. **Replay Attacks**: Hash must match specific version's metadata
4. **Truncated Downloads**: Size validation catches incomplete transfers

### Hash Comparison

The validator performs case-insensitive hash comparison and handles whitespace in hashes, making it robust against common metadata formatting variations.

## Implementation Details

### SHA-256 Calculation

Uses OpenSSL EVP API for modern, secure hash computation:

```cpp
EVP_MD_CTX* ctx = EVP_MD_CTX_new();
EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);

// Process file in 4KB chunks
while (file.read(buffer, sizeof(buffer))) {
    EVP_DigestUpdate(ctx, buffer, file.gcount());
}

EVP_DigestFinal_ex(ctx, hash, &hash_length);
```

### Hash Normalization

- Converts to lowercase
- Trims leading/trailing whitespace
- Removes internal whitespace

This ensures hashes from different sources (server metadata, command line) are compared correctly.

## Testing

See [Task 05 Testing Report](testing/task-05.md) for comprehensive test results including:

- Unit tests for validation logic
- Attack simulation tests
- Edge case coverage

## Integration Points

### DownloadManager

The DownloadManager uses IntegrityValidator to verify downloads before saving:

```cpp
// After download completes
auto validation = validator.validate_file(temp_path, expected_hash, expected_size);
if (!validation.is_valid()) {
    cleanup(temp_path);
    return DownloadResult::error("Integrity check failed");
}
```

### UpdateManager

The UpdateManager coordinates verification with download operations:

```cpp
// During update flow
auto download_result = manager.download_update(version, progress_callback);
if (download_result.success) {
    auto validation = validator.validate_file(
        download_result.file_path,
        metadata.sha256,
        metadata.image_size
    );
    // Handle validation result...
}
```

## Limitations

1. **No Digital Signatures**: Current implementation verifies integrity but not authenticity (Task scope restriction)
2. **Single Hash Algorithm**: SHA-256 only; could be extended to support multiple algorithms
3. **No Certificate Pinning**: Server identity is not verified (uses HTTP in development)

## Future Enhancements

1. **Ed25519 Signatures**: Add cryptographic authenticity verification
2. **Multiple Hash Algorithms**: Support SHA-384, SHA-512 for flexibility
3. **Certificate Pinning**: HTTPS with pinned server certificates
4. **Delta Updates**: Verify incremental update integrity
