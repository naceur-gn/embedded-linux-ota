# Digital Signature & Authenticity Verification

## Overview

This document describes the cryptographic authenticity verification system for OTA updates. The system ensures that updates come from a trusted source and have not been tampered with.

## Security Principle: Integrity vs Authenticity

### SHA-256 (Integrity)

SHA-256 provides **integrity** verification. It answers:

> "Is the downloaded file identical to the expected file?"

**Use case:** Detecting accidental corruption during download.

### Digital Signature (Authenticity + Integrity)

Digital signatures provide **authenticity** and **integrity**. They answer:

> "Was this update signed by someone possessing the trusted private signing key?"

**Use case:** Detecting intentional tampering and verifying the source.

### Why Both Are Needed

A file can have a valid SHA-256 hash but still be malicious:

1. Attacker compromises the server
2. Attacker replaces the image with a malicious version
3. Attacker updates the metadata with the new SHA-256 hash
4. SHA-256 verification passes, but the update is malicious

Digital signatures prevent this because:
- Only the holder of the private key can create valid signatures
- The device only has the public key (cannot sign)
- Any modification to the signed data invalidates the signature

## Cryptographic Architecture

### Asymmetric Signing Model

```
OTA RELEASE SERVER
        │
        │ private signing key
        ▼
   Sign release
        │
        ▼
 signed artifact / manifest
        │
        │ HTTPS
        ▼
 EMBEDDED LINUX DEVICE
        │
        │ trusted public key
        ▼
 Verify signature
```

### Key Properties

1. **Private Key:** Only exists in the signing environment
2. **Public Key:** Deployed to devices, cannot create signatures
3. **Signature:** Proves the data was signed by the private key holder
4. **Verification:** Uses public key to confirm signature validity

## Cryptographic Algorithm

### Algorithm Selection

This project uses **ECDSA (Elliptic Curve Digital Signature Algorithm)** with:

| Parameter | Value |
|-----------|-------|
| Key Type | EC (Elliptic Curve) |
| Curve | P-256 (prime256v1, secp256r1) |
| Signature Algorithm | ECDSA |
| Digest Algorithm | SHA-256 |
| Signature Encoding | DER |

### Why ECDSA Over RSA?

1. **Smaller key sizes:** 256-bit ECDSA provides comparable security to 3072-bit RSA
2. **Better performance:** Faster signing and verification on embedded devices
3. **Modern standard:** Widely supported by OpenSSL and hardware security modules
4. **NIST approved:** P-256 is a NIST standard curve

### Key Size Comparison

| Algorithm | Security Level | Key Size | Signature Size |
|-----------|---------------|----------|----------------|
| RSA-2048 | 112-bit | 2048 bits | 256 bytes |
| RSA-3072 | 128-bit | 3072 bits | 384 bytes |
| ECDSA P-256 | 128-bit | 256 bits | 64 bytes |

## What Is Signed

### Canonical Release Manifest

The signed data is a canonical JSON representation of the release metadata:

```json
{
    "version": "1.1.0",
    "hardware_version": "revA",
    "release_type": "system",
    "image": "image.bin",
    "sha256": "abc123...",
    "size": 10485760,
    "timestamp": "2026-09-03T12:00:00Z",
    "min_version": "1.0.0"
}
```

### Canonicalization Rules

To ensure deterministic signing:

1. **Sorted keys:** All JSON keys are sorted alphabetically
2. **No whitespace:** No unnecessary whitespace
3. **No signature field:** The signature field is excluded from signing
4. **Consistent encoding:** UTF-8 encoding with no BOM

Example canonical format:
```
{"hardware_version":"revA","image":"image.bin","min_version":"1.0.0","release_type":"system","sha256":"abc123...","size":10485760,"timestamp":"2026-09-03T12:00:00Z","version":"1.1.0"}
```

### Why Canonicalization Matters

Without canonicalization, the same data could produce different signatures:
```json
{"version": "1.0.0"}
```
vs
```json
{
  "version": "1.0.0"
}
```

Canonicalization ensures consistent byte representation.

## Signing Workflow

### 1. Generate Key Pair

```bash
./scripts/generate-signing-key
```

This creates:
- `keys/development/private/ota-signing.key` (private)
- `keys/development/public/ota-signing.pub` (public)

### 2. Create Release

```bash
./ota-server/scripts/create_release.py 1.1.0 /path/to/image.bin
```

### 3. Sign Release

```bash
./scripts/sign-release /path/to/release/
```

This:
1. Validates metadata
2. Verifies image integrity
3. Creates canonical metadata
4. Signs with private key
5. Saves signature to `metadata.sig`
6. Updates `metadata.json` with signature

### 4. Deploy Release

Copy the signed release to the OTA server.

## Verification Workflow

### Device Verification Flow

```
Downloaded Update
       │
       ▼
SHA-256 Verification
       │
       │ valid
       ▼
Load Trusted Public Key
       │
       ▼
Load Signature
       │
       ▼
Reconstruct Canonical Data
       │
       ▼
Verify Digital Signature
       │
       │ valid
       ▼
AUTHENTIC UPDATE
```

### CLI Verification

```bash
# Verify integrity (SHA-256 only)
./ota-cli verify -i /path/to/image.bin -e <expected-hash>

# Verify authenticity (digital signature)
./ota-cli verify-signature -r /path/to/release/ -k /path/to/trusted.pub
```

### Output Examples

Success:
```
Authenticity: SIGNATURE_VALID
Signed data hash: abc123...
```

Failure:
```
Authenticity: SIGNATURE_INVALID
Reason: Signature verification failed
```

## Trusted Public Key

### Device Configuration

The trusted public key is installed at:
```
/etc/ota/trusted/ota-signing.pub
```

### Security Properties

1. **Read-only:** Not writable by the OTA client user
2. **Root-owned:** Owned by root or appropriate privileged account
3. **Validated:** Checked when loaded
4. **Never downloaded:** Never fetched from the OTA server

### Trust Anchor

This public key establishes the device's trust anchor. Only updates signed by the corresponding private key are accepted.

## Private Key Protection

### Development Keys

For development and testing:
- Stored in `keys/development/private/`
- Protected by file permissions (600)
- Never committed to Git

### Production Keys

For production deployments:
- **NEVER** stored on the OTA server filesystem
- Used only in a secure signing environment
- May be stored in a Hardware Security Module (HSM)
- Rotated periodically

### Security Rules

The private key must NEVER be:
- Committed to Git
- Stored on the device
- Sent through the OTA system
- Hard-coded in source code
- Logged in any output

## Failure Behavior

If signature verification fails:

```
REJECT UPDATE
```

The client must:
1. Not install the image
2. Not activate a slot
3. Not modify the active system
4. Log the failure
5. Return a non-zero error code
6. Preserve the currently running version
7. Clean temporary artifacts

### Log Example

```
ERROR [signature-verifier] Digital signature verification failed
ERROR [ota-client] Update 1.1.0 rejected: signature invalid
```

## Threat Model

### Mitigated Threats

1. **Tampered updates:** Any modification invalidates signature
2. **Wrong signing key:** Signatures from untrusted keys are rejected
3. **Missing signatures:** Updates without signatures are rejected
4. **Replay attacks:** Old signatures for new versions are rejected
5. **Server compromise:** Attacker cannot sign without private key

### Not Mitigated (Out of Scope)

1. Compromised private key (requires key rotation)
2. Compromised device (requires secure boot)
3. Man-in-the-middle (requires HTTPS)
4. Downgrade attacks (requires version enforcement)

## Testing

See [Task 06 Testing Report](testing/task-06.md) for comprehensive test results.

### Test Categories

1. **Unit Tests:** Signature verification logic
2. **Attack Tests:** Various attack vectors
3. **Integration Tests:** End-to-end verification flow
4. **Regression Tests:** Ensure previous functionality works

## Implementation Files

### Source Code

- `include/security/signature_verifier.h` - Header file
- `src/security/signature_verifier.cpp` - Implementation

### Scripts

- `scripts/generate-signing-key` - Key generation
- `scripts/sign-release` - Release signing

### Keys

- `keys/README.md` - Key management documentation
- `keys/development/public/ota-signing.pub` - Development public key

## Future Enhancements

1. **Hardware Security Modules (HSM):** Secure key storage
2. **Key rotation:** Automated key rotation process
3. **Certificate chains:** Support for certificate-based trust
4. **Multiple algorithms:** Support for Ed25519, RSA-PSS
5. **Secure boot:** Hardware root of trust
