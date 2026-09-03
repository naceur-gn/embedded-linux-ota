# OTA Server Documentation

## Overview

The OTA Server is a minimal HTTP server for distributing software updates to embedded Linux devices. It provides release metadata and update images through a simple REST API.

## Architecture

```
                    OTA REPOSITORY
                         │
             ┌───────────┴───────────┐
             │                       │
         RELEASES                METADATA
             │                       │
             └───────────┬───────────┘
                         │
                         ▼
                    OTA SERVER
                         │
                 ┌───────┴────────┐
                 │                │
             UPDATE API       IMAGE API
                 │                │
                 └───────┬────────┘
                         │
                        HTTP
                         │
                         ▼
                   EMBEDDED DEVICE
```

## Repository Structure

```
ota-server/
├── releases/
│   ├── 1.0.0/
│   │   ├── image.bin
│   │   └── metadata.json
│   └── 1.1.0/
│       ├── image.bin
│       └── metadata.json
├── server/
│   └── ota_server.py
├── scripts/
│   └── create_release.py
├── tests/
│   ├── test_ota.py
│   └── test_api.py
└── README.md
```

## Release Metadata Format

```json
{
  "version": "1.1.0",
  "hardware_version": "revA",
  "release_type": "system",
  "image": "image.bin",
  "sha256": "abc123...",
  "signature": "",
  "size": 12345678,
  "timestamp": "2026-09-03T12:00:00+00:00",
  "min_version": "1.0.0"
}
```

### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| version | string | Yes | Semantic version (MAJOR.MINOR.PATCH) |
| hardware_version | string | Yes | Target hardware revision |
| release_type | string | Yes | Type of release (system, application) |
| image | string | Yes | Image filename |
| sha256 | string | Yes | SHA-256 hash of the image |
| signature | string | No | Digital signature (placeholder) |
| size | integer | Yes | Image size in bytes |
| timestamp | string | Yes | ISO 8601 timestamp |
| min_version | string | Yes | Minimum supported version |

## API Endpoints

### Update Discovery

```
GET /api/v1/update?device_id=X&hardware_version=Y&current_version=Z
```

**Parameters:**
- `device_id` (optional): Device identifier
- `hardware_version` (required): Device hardware version
- `current_version` (required): Device current software version

**Response (update available):**
```json
{
  "update_available": true,
  "version": "1.1.0",
  "hardware_version": "revA",
  "image": "/releases/1.1.0/image.bin",
  "size": 12345678,
  "sha256": "abc123...",
  "release_type": "system",
  "timestamp": "2026-09-03T12:00:00+00:00"
}
```

**Response (no update):**
```json
{
  "update_available": false,
  "current_version": "1.1.0",
  "hardware_version": "revA"
}
```

### List Releases

```
GET /api/v1/releases
```

**Response:**
```json
{
  "releases": [
    {
      "version": "1.0.0",
      "hardware_version": "revA",
      "size": 12345678
    },
    {
      "version": "1.1.0",
      "hardware_version": "revA",
      "size": 12345678
    }
  ]
}
```

### Download Image

```
GET /releases/<version>/image.bin
```

Returns the raw binary image data.

### Get Metadata

```
GET /releases/<version>/metadata.json
```

Returns the release metadata JSON.

## Version Policy

- Versions must follow semantic versioning: `MAJOR.MINOR.PATCH`
- Each component must be a non-negative integer
- Leading zeros are not allowed (e.g., `01.0.0` is invalid)
- The server rejects malformed versions

## Compatibility Policy

An update is considered compatible if:
1. The release `hardware_version` matches the device `hardware_version`
2. The release version is newer than the device `current_version`

The server does NOT perform downgrade logic - if the device has a newer version, no update is offered.

## SHA-256 Generation

SHA-256 hashes are calculated automatically when creating a release:
1. The image file is copied to the release directory
2. SHA-256 is computed from the image bytes
3. The hash is stored in `metadata.json`

The server never modifies the image after hash generation.

## HTTP Status Codes

| Code | Meaning |
|------|---------|
| 200 | Success |
| 400 | Bad request (missing parameters, invalid format) |
| 404 | Resource not found |
| 500 | Internal server error |

## Logging

The server logs:
- Update requests and responses
- Image downloads
- Release loading
- Errors and warnings

Log format:
```
YYYY-MM-DD HH:MM:SS [LEVEL] message
```

## Security Boundary

**Important:** The server provides information only. The device is responsible for:
- Verifying image integrity (SHA-256)
- Verifying digital signatures
- Checking compatibility

The server does NOT:
- Authenticate devices
- Enforce updates
- Modify images

## Usage

### Create a Release

```bash
cd ota-server
./scripts/create_release.py 1.1.0 /path/to/image.bin --hardware-version revA
```

### Start the Server

```bash
cd ota-server
./server/ota_server.py --port 8080 --releases-dir releases
```

### Query for Updates

```bash
curl "http://localhost:8080/api/v1/update?device_id=device-001&hardware_version=revA&current_version=1.0.0"
```

### Download an Image

```bash
curl -O http://localhost:8080/releases/1.1.0/image.bin
```

## Testing

```bash
cd ota-server
python -m pytest tests/ -v
```
