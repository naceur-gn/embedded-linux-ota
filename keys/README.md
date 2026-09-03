# OTA Signing Keys

## Overview

This directory contains signing keys for the OTA update system.

## Directory Structure

```
keys/
├── README.md                    # This file
├── development/
│   ├── public/
│   │   └── ota-signing.pub     # Development public key (can be committed)
│   └── private/
│       └── ota-signing.key     # Development private key (NEVER commit)
└── production/
    ├── public/
    │   └── ota-signing.pub     # Production public key
    └── private/
        └── ota-signing.key     # Production private key (NEVER commit)
```

## Security Rules

### NEVER commit private keys

Private keys must NEVER be:
- Committed to Git
- Stored on the device
- Sent through the OTA system
- Hard-coded in source code
- Logged in any output

### Key generation

Generate keys using the provided script:

```bash
./scripts/generate-signing-key
```

Or with OpenSSL directly:

```bash
# Generate ECDSA P-256 private key
openssl ecparam -genkey -name prime256v1 -noout -out keys/development/private/ota-signing.key

# Extract public key
openssl ec -in keys/development/private/ota-signing.key \
    -pubout -out keys/development/public/ota-signing.pub
```

### Device trusted key

The device must contain only the public key:

```
/etc/ota/trusted/
    ota-signing.pub
```

The public key should be:
- Owned by root or appropriate privileged account
- Not writable by the unprivileged OTA client user
- Validated when loaded

### Production key handling

Production private keys should be:
- Stored in a hardware security module (HSM) or secure key management system
- Never present on the OTA server filesystem
- Used only in a secure signing environment
- Rotated periodically

## Algorithm

This project uses ECDSA (Elliptic Curve Digital Signature Algorithm) with:
- Curve: P-256 (prime256v1, secp256r1)
- Digest: SHA-256
- Signature encoding: DER

### Why ECDSA over RSA?

1. **Smaller key sizes**: 256-bit ECDSA provides comparable security to 3072-bit RSA
2. **Better performance**: Faster signing and verification on embedded devices
3. **Modern standard**: Widely supported by OpenSSL and hardware security modules
4. **NIST approved**: P-256 is a NIST standard curve

## Testing

For development and testing, use the development keys.

For production, use a secure key management system and never expose the private key.
