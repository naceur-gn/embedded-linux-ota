# OTA Client Communication Layer

## Overview

The OTA Client Communication Layer enables the embedded Linux device to communicate with the OTA server, discover available updates, and download update artifacts safely.

## Architecture

```
┌─────────────────────────────────────────────┐
│           EMBEDDED LINUX DEVICE             │
│                                             │
│  ┌───────────────────────────────────────┐  │
│  │              OTA CLIENT               │  │
│  │                                       │  │
│  │  Update Manager                       │  │
│  │       │                               │  │
│  │       ▼                               │  │
│  │  OTA Client API                       │  │
│  │       │                               │  │
│  │       ▼                               │  │
│  │  HTTP/HTTPS Transport                 │  │
│  │       │                               │  │
│  │       ▼                               │  │
│  │  Download Manager                     │  │
│  └───────────────┬───────────────────────┘  │
│                  │                          │
│                  ▼                          │
│        /var/lib/ota/downloads/              │
└──────────────────┼──────────────────────────┘
                   │
                  HTTP
                   │
                   ▼
          ┌──────────────────┐
          │    OTA SERVER    │
          │                  │
          │ Update API       │
          │ Image API        │
          └──────────────────┘
```

## Modules

### HTTP Client (`network/http_client.h/cpp`)

Handles HTTP/HTTPS communication with the OTA server.

Features:
- Configurable timeouts (connect, request, download)
- Retry mechanism with configurable count and delay
- Download size protection
- TLS/HTTPS support

### Response Parser (`client/response_parser.h/cpp`)

Parses and validates server responses.

Features:
- JSON response parsing
- Metadata validation
- Error reporting

### Download Manager (`download/download_manager.h/cpp`)

Manages safe download of update images.

Features:
- Temporary file storage
- SHA-256 calculation
- Download cleanup
- Size limit enforcement

### Update Manager (`client/update_manager.h/cpp`)

Orchestrates the update discovery and download process.

Features:
- Device configuration integration
- Version comparison
- Hardware compatibility checking
- Update check workflow

## Communication Flow

```
DEVICE
  │
  │ HTTPS
  ▼
OTA SERVER
  │
  │ metadata
  ▼
OTA CLIENT
  │
  │ validate response
  ▼
DOWNLOAD MANAGER
  │
  ▼
TEMPORARY STORAGE
```

## API Interaction

### Check for Update

```
GET /api/v1/update?device_id=X&hardware_version=Y&current_version=Z
```

Response:
```json
{
  "update_available": true,
  "version": "1.1.0",
  "hardware_version": "revA",
  "image": "/releases/1.1.0/image.bin",
  "size": 1024,
  "sha256": "abc123"
}
```

### Download Image

```
GET /releases/<version>/image.bin
```

Returns binary image data.

## Metadata Parsing

The client parses the following fields:
- `update_available`: Boolean indicating if an update is available
- `version`: Available version string
- `hardware_version`: Target hardware version
- `image`: Image download path
- `size`: Image size in bytes
- `sha256`: SHA-256 hash of the image

## Version Handling

Comparison rules:
- `available > installed` → Update available
- `available == installed` → No update
- `available < installed` → No downgrade

## Compatibility Checking

The client verifies hardware compatibility:
- Device `hardware_version` must match release `hardware_version`
- Incompatible updates are rejected before download

## Download Process

1. Create temporary file in download directory
2. Download image with size limit protection
3. Verify downloaded file size
4. Calculate SHA-256 hash
5. Return download result with file path and hash

## Temporary Storage

Downloads are stored in:
```
/var/lib/ota/downloads/<version>.img
```

Files are temporary and can be cleaned up after installation.

## Timeouts

Configurable timeouts:
- `connect_timeout`: Connection establishment (default: 10s)
- `request_timeout`: HTTP request completion (default: 30s)
- `download_timeout`: Full download completion (default: 300s)

## Retry Policy

Default retry settings:
- `retry_count`: 3 attempts
- `retry_delay`: 2 seconds between attempts

Retries are attempted for:
- Network errors
- Connection timeouts
- Server errors (5xx)

Retries are NOT attempted for:
- Client errors (4xx)
- Permanent failures

## Error Model

Error categories:
- `NETWORK_ERROR`: Connection failures
- `TIMEOUT`: Operation timed out
- `SERVER_ERROR`: HTTP 5xx errors
- `HTTP_ERROR`: HTTP 4xx errors
- `INVALID_RESPONSE`: Malformed server response
- `INCOMPATIBLE_DEVICE`: Hardware mismatch
- `NO_UPDATE`: Already up to date
- `DOWNLOAD_ERROR`: Download failed
- `DOWNLOAD_TOO_LARGE`: Size limit exceeded
- `DOWNLOAD_INCOMPLETE`: Partial download

## HTTPS Behavior

- TLS verification is enabled by default
- Custom CA certificates can be configured
- Self-signed certificates require explicit trust configuration

## Security Boundaries

The communication layer:
- Does NOT verify digital signatures (future task)
- Does NOT install updates (future task)
- Does NOT modify system state
- Only downloads and stores temporarily

## CLI Usage

```bash
# Check for updates
./build/ota-cli check -c configs/device.conf -u http://localhost:8080

# Download update
./build/ota-cli download -c configs/device.conf -u http://localhost:8080
```

## Testing

```bash
cd build
cmake --build .
./tests/ota_tests
```
