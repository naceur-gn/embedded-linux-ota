#!/usr/bin/env python3
"""
OTA Server - Minimal HTTP server for OTA update distribution.

Usage:
    ./ota_server.py [--port PORT] [--releases-dir DIR]

API Endpoints:
    GET /api/v1/update?device_id=X&hardware_version=Y&current_version=Z
    GET /releases/<version>/image.bin
    GET /releases/<version>/metadata.json
    GET /api/v1/releases
"""

import os
import sys
import json
import re
import logging
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
from datetime import datetime, timezone


SEMVER_REGEX = r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$"


class OTAServer:
    """OTA Server managing releases and serving updates."""

    def __init__(self, releases_dir):
        self.releases_dir = os.path.abspath(releases_dir)
        self.releases = {}
        self.load_releases()

    def load_releases(self):
        """Load all valid releases from the releases directory."""
        self.releases.clear()

        if not os.path.exists(self.releases_dir):
            logging.warning(f"Releases directory not found: {self.releases_dir}")
            return

        for version_dir in os.listdir(self.releases_dir):
            version_path = os.path.join(self.releases_dir, version_dir)

            if not os.path.isdir(version_path):
                continue

            metadata_path = os.path.join(version_path, "metadata.json")
            image_path = os.path.join(version_path, "image.bin")

            if not os.path.exists(metadata_path):
                logging.warning(f"Release {version_dir}: metadata.json missing")
                continue

            if not os.path.exists(image_path):
                logging.warning(f"Release {version_dir}: image.bin missing")
                continue

            try:
                with open(metadata_path, "r") as f:
                    metadata = json.load(f)

                if not self.validate_metadata(metadata):
                    logging.warning(f"Release {version_dir}: invalid metadata")
                    continue

                if not self.validate_version(metadata.get("version", "")):
                    logging.warning(f"Release {version_dir}: invalid version format")
                    continue

                self.releases[metadata["version"]] = {
                    "metadata": metadata,
                    "path": version_path,
                    "image_path": image_path
                }

                logging.info(f"Loaded release: {metadata['version']}")

            except json.JSONDecodeError as e:
                logging.error(f"Release {version_dir}: invalid JSON: {e}")
            except Exception as e:
                logging.error(f"Release {version_dir}: error loading: {e}")

    def validate_metadata(self, metadata):
        """Validate release metadata structure."""
        required_fields = ["version", "hardware_version", "image", "sha256", "size"]
        for field in required_fields:
            if field not in metadata:
                return False
            if not metadata[field] and field != "signature":
                return False
        return True

    def validate_version(self, version):
        """Validate semantic version format."""
        return bool(re.match(SEMVER_REGEX, version))

    def find_update(self, hardware_version, current_version):
        """Find the best available update for a device."""
        candidates = []

        for version, release in self.releases.items():
            metadata = release["metadata"]

            if metadata["hardware_version"] != hardware_version:
                continue

            if not self.is_newer_version(version, current_version):
                continue

            candidates.append((version, metadata))

        if not candidates:
            return None

        candidates.sort(key=lambda x: self.version_tuple(x[0]), reverse=True)
        return candidates[0]

    def is_newer_version(self, version1, version2):
        """Check if version1 is newer than version2."""
        return self.version_tuple(version1) > self.version_tuple(version2)

    def version_tuple(self, version):
        """Convert version string to comparable tuple."""
        try:
            parts = version.split(".")
            return tuple(int(p) for p in parts)
        except (ValueError, AttributeError):
            return (0, 0, 0)

    def get_image_path(self, version):
        """Get the path to a release image."""
        if version in self.releases:
            return self.releases[version]["image_path"]
        return None

    def get_release_metadata(self, version):
        """Get release metadata."""
        if version in self.releases:
            return self.releases[version]["metadata"]
        return None

    def get_all_releases(self):
        """Get list of all releases."""
        return [
            {
                "version": v,
                "hardware_version": r["metadata"]["hardware_version"],
                "size": r["metadata"]["size"]
            }
            for v, r in sorted(self.releases.items())
        ]


class OTARequestHandler(BaseHTTPRequestHandler):
    """HTTP request handler for OTA server."""

    server_instance = None

    def log_message(self, format, *args):
        """Override to use Python logging."""
        logging.info(f"{self.client_address[0]} - {format % args}")

    def do_GET(self):
        """Handle GET requests."""
        parsed = urlparse(self.path)
        path = parsed.path
        params = parse_qs(parsed.query)

        if path == "/api/v1/update":
            self.handle_update_request(params)
        elif path == "/api/v1/releases":
            self.handle_releases_list()
        elif path.startswith("/releases/") and path.endswith("/image.bin"):
            self.handle_image_download(path)
        elif path.startswith("/releases/") and path.endswith("/metadata.json"):
            self.handle_metadata_request(path)
        else:
            self.send_error(404, "Not Found")

    def handle_update_request(self, params):
        """Handle update discovery request."""
        device_id = params.get("device_id", ["unknown"])[0]
        hardware_version = params.get("hardware_version", [""])[0]
        current_version = params.get("current_version", ["0.0.0"])[0]

        logging.info(f"Update request: device={device_id} hw={hardware_version} ver={current_version}")

        if not hardware_version:
            self.send_json_response(400, {"error": "hardware_version required"})
            return

        if not self.server_instance.validate_version(current_version):
            self.send_json_response(400, {"error": "Invalid current_version format"})
            return

        result = self.server_instance.find_update(hardware_version, current_version)

        if result is None:
            logging.info(f"No update available for {device_id}")
            response = {
                "update_available": False,
                "current_version": current_version,
                "hardware_version": hardware_version
            }
        else:
            version, metadata = result
            logging.info(f"Update available: {version}")
            response = {
                "update_available": True,
                "version": metadata["version"],
                "hardware_version": metadata["hardware_version"],
                "image": f"/releases/{version}/image.bin",
                "size": metadata["size"],
                "sha256": metadata["sha256"],
                "release_type": metadata.get("release_type", "system"),
                "timestamp": metadata.get("timestamp", "")
            }

        self.send_json_response(200, response)

    def handle_releases_list(self):
        """Handle releases list request."""
        releases = self.server_instance.get_all_releases()
        self.send_json_response(200, {"releases": releases})

    def handle_image_download(self, path):
        """Handle image download request."""
        parts = path.strip("/").split("/")
        if len(parts) != 3 or parts[0] != "releases" or parts[2] != "image.bin":
            self.send_error(400, "Invalid path")
            return

        version = parts[1]

        if not self.server_instance.validate_version(version):
            logging.warning(f"Path traversal attempt: {path}")
            self.send_error(400, "Invalid version format")
            return

        image_path = self.server_instance.get_image_path(version)
        if image_path is None:
            self.send_error(404, "Release not found")
            return

        logging.info(f"Image download: {version}/image.bin")

        try:
            with open(image_path, "rb") as f:
                content = f.read()

            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        except Exception as e:
            logging.error(f"Error serving image: {e}")
            self.send_error(500, "Internal Server Error")

    def handle_metadata_request(self, path):
        """Handle metadata request."""
        parts = path.strip("/").split("/")
        if len(parts) != 3 or parts[0] != "releases" or parts[2] != "metadata.json":
            self.send_error(400, "Invalid path")
            return

        version = parts[1]

        if not self.server_instance.validate_version(version):
            logging.warning(f"Path traversal attempt: {path}")
            self.send_error(400, "Invalid version format")
            return

        metadata = self.server_instance.get_release_metadata(version)
        if metadata is None:
            self.send_error(404, "Release not found")
            return

        logging.info(f"Metadata request: {version}")
        self.send_json_response(200, metadata)

    def send_json_response(self, status_code, data):
        """Send JSON response."""
        response = json.dumps(data, indent=2)

        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.wfile.write(response.encode())


def run_server(port=8080, releases_dir="releases"):
    """Start the OTA server."""
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S"
    )

    server = OTAServer(releases_dir)
    OTARequestHandler.server_instance = server

    httpd = HTTPServer(("0.0.0.0", port), OTARequestHandler)

    logging.info(f"OTA Server starting on port {port}")
    logging.info(f"Releases directory: {os.path.abspath(releases_dir)}")
    logging.info(f"Loaded {len(server.releases)} releases")
    logging.info("Endpoints:")
    logging.info(f"  GET /api/v1/update?device_id=X&hardware_version=Y&current_version=Z")
    logging.info(f"  GET /api/v1/releases")
    logging.info(f"  GET /releases/<version>/image.bin")
    logging.info(f"  GET /releases/<version>/metadata.json")

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        logging.info("Server shutting down")
        httpd.shutdown()


def main():
    import argparse

    parser = argparse.ArgumentParser(description="OTA Server")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--releases-dir", default="releases", help="Releases directory")

    args = parser.parse_args()

    run_server(args.port, args.releases_dir)


if __name__ == "__main__":
    main()
