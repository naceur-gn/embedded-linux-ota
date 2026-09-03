#!/usr/bin/env python3
"""
Release Creation Tool for OTA Server

Usage:
    ./create_release.sh <version> <image_path> [--hardware-version <hw>]

Examples:
    ./create_release.sh 1.1.0 /path/to/image.bin
    ./create_release.sh 1.1.0 /path/to/image.bin --hardware-version revA
"""

import os
import sys
import json
import hashlib
import re
import shutil
import argparse
from datetime import datetime, timezone


SEMVER_REGEX = r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$"


def validate_version(version):
    """Validate semantic version format."""
    return bool(re.match(SEMVER_REGEX, version))


def calculate_sha256(file_path):
    """Calculate SHA-256 hash of a file."""
    sha256_hash = hashlib.sha256()
    with open(file_path, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()


def get_file_size(file_path):
    """Get file size in bytes."""
    return os.path.getsize(file_path)


def create_release(version, image_path, hardware_version="revA", releases_dir="releases"):
    """Create a new release in the repository."""
    errors = []

    if not validate_version(version):
        errors.append(f"Invalid version format: {version} (expected MAJOR.MINOR.PATCH)")

    if not os.path.exists(image_path):
        errors.append(f"Image file not found: {image_path}")

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return False

    release_dir = os.path.join(releases_dir, version)

    if os.path.exists(release_dir):
        print(f"WARNING: Release {version} already exists, updating", file=sys.stderr)
        shutil.rmtree(release_dir)

    os.makedirs(release_dir, exist_ok=True)

    dest_image = os.path.join(release_dir, "image.bin")
    shutil.copy2(image_path, dest_image)

    sha256 = calculate_sha256(dest_image)
    image_size = get_file_size(dest_image)

    metadata = {
        "version": version,
        "hardware_version": hardware_version,
        "release_type": "system",
        "image": "image.bin",
        "sha256": sha256,
        "signature": "",
        "size": image_size,
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "min_version": "1.0.0"
    }

    metadata_path = os.path.join(release_dir, "metadata.json")
    with open(metadata_path, "w") as f:
        json.dump(metadata, f, indent=2)

    print(f"Release {version} created successfully:")
    print(f"  Directory: {release_dir}")
    print(f"  Image: {dest_image}")
    print(f"  Size: {image_size} bytes")
    print(f"  SHA-256: {sha256}")

    return True


def main():
    parser = argparse.ArgumentParser(description="Create an OTA release")
    parser.add_argument("version", help="Release version (semver: MAJOR.MINOR.PATCH)")
    parser.add_argument("image", help="Path to the update image file")
    parser.add_argument("--hardware-version", default="revA", help="Target hardware version")
    parser.add_argument("--releases-dir", default="releases", help="Releases directory path")

    args = parser.parse_args()

    success = create_release(
        args.version,
        args.image,
        args.hardware_version,
        args.releases_dir
    )

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
