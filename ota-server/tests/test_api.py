#!/usr/bin/env python3
"""
API Tests for OTA Server HTTP endpoints.

These tests start the server and test the actual HTTP API.
"""

import os
import sys
import json
import hashlib
import shutil
import tempfile
import unittest
from http.client import HTTPConnection
from threading import Thread
from time import sleep

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "server"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))

from ota_server import OTAServer, OTARequestHandler, run_server
from create_release import create_release


class TestHTTPAPI(unittest.TestCase):
    """Test HTTP API endpoints."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.releases_dir = os.path.join(self.test_dir, "releases")
        os.makedirs(self.releases_dir)

        self.image_file = os.path.join(self.test_dir, "test_image.bin")
        self.test_data = b"test firmware image data for API test"
        with open(self.image_file, "wb") as f:
            f.write(self.test_data)

        create_release("1.0.0", self.image_file, "revA", self.releases_dir)
        create_release("1.1.0", self.image_file, "revA", self.releases_dir)
        create_release("1.2.0", self.image_file, "revB", self.releases_dir)

        self.port = 18081
        self.server_thread = None
        self.httpd = None

    def tearDown(self):
        if self.httpd:
            self.httpd.shutdown()
        if self.server_thread:
            self.server_thread.join(timeout=5)
        shutil.rmtree(self.test_dir)

    def start_server(self):
        """Start the OTA server in a background thread."""
        server = OTAServer(self.releases_dir)
        OTARequestHandler.server_instance = server

        self.httpd = HTTPServer(("127.0.0.1", self.port), OTARequestHandler)
        self.httpd.allow_reuse_address = True

        self.server_thread = Thread(target=self.httpd.serve_forever)
        self.server_thread.daemon = True
        self.server_thread.start()

        sleep(0.5)

    def make_request(self, method, path):
        """Make an HTTP request to the server."""
        conn = HTTPConnection("127.0.0.1", self.port, timeout=5)
        conn.request(method, path)
        response = conn.getresponse()
        body = response.read().decode()
        conn.close()
        return response.status, body

    def test_update_available(self):
        self.start_server()

        path = "/api/v1/update?device_id=device-001&hardware_version=revA&current_version=1.0.0"
        status, body = self.make_request("GET", path)

        self.assertEqual(status, 200)
        data = json.loads(body)
        self.assertTrue(data["update_available"])
        self.assertEqual(data["version"], "1.1.0")

    def test_no_update_current(self):
        self.start_server()

        path = "/api/v1/update?device_id=device-001&hardware_version=revA&current_version=1.1.0"
        status, body = self.make_request("GET", path)

        self.assertEqual(status, 200)
        data = json.loads(body)
        self.assertFalse(data["update_available"])

    def test_no_update_newer(self):
        self.start_server()

        path = "/api/v1/update?device_id=device-001&hardware_version=revA&current_version=2.0.0"
        status, body = self.make_request("GET", path)

        self.assertEqual(status, 200)
        data = json.loads(body)
        self.assertFalse(data["update_available"])

    def test_incompatible_hardware(self):
        self.start_server()

        path = "/api/v1/update?device_id=device-001&hardware_version=revC&current_version=1.0.0"
        status, body = self.make_request("GET", path)

        self.assertEqual(status, 200)
        data = json.loads(body)
        self.assertFalse(data["update_available"])

    def test_missing_hardware_version(self):
        self.start_server()

        path = "/api/v1/update?device_id=device-001&current_version=1.0.0"
        status, body = self.make_request("GET", path)

        self.assertEqual(status, 400)

    def test_invalid_current_version(self):
        self.start_server()

        path = "/api/v1/update?device_id=device-001&hardware_version=revA&current_version=1.0"
        status, body = self.make_request("GET", path)

        self.assertEqual(status, 400)

    def test_image_download(self):
        self.start_server()

        status, body = self.make_request("GET", "/releases/1.0.0/image.bin")

        self.assertEqual(status, 200)
        self.assertEqual(body.encode(), self.test_data)

    def test_missing_image(self):
        self.start_server()

        status, body = self.make_request("GET", "/releases/2.0.0/image.bin")

        self.assertEqual(status, 404)

    def test_metadata_download(self):
        self.start_server()

        status, body = self.make_request("GET", "/releases/1.0.0/metadata.json")

        self.assertEqual(status, 200)
        data = json.loads(body)
        self.assertEqual(data["version"], "1.0.0")

    def test_missing_metadata(self):
        self.start_server()

        status, body = self.make_request("GET", "/releases/2.0.0/metadata.json")

        self.assertEqual(status, 404)

    def test_releases_list(self):
        self.start_server()

        status, body = self.make_request("GET", "/api/v1/releases")

        self.assertEqual(status, 200)
        data = json.loads(body)
        self.assertIn("releases", data)
        self.assertEqual(len(data["releases"]), 3)

    def test_not_found(self):
        self.start_server()

        status, body = self.make_request("GET", "/nonexistent")

        self.assertEqual(status, 404)

    def test_path_traversal_rejected(self):
        self.start_server()

        status, body = self.make_request("GET", "/releases/../etc/passwd/image.bin")

        self.assertIn(status, [400, 404])

    def test_invalid_version_format(self):
        self.start_server()

        status, body = self.make_request("GET", "/releases/1.0/image.bin")

        self.assertEqual(status, 400)

    def test_image_bytes_match(self):
        self.start_server()

        status, body = self.make_request("GET", "/releases/1.0.0/image.bin")

        self.assertEqual(status, 200)
        self.assertEqual(body.encode(), self.test_data)


class HTTPServer:
    """Simple HTTP server wrapper for testing."""
    pass


from http.server import HTTPServer


if __name__ == "__main__":
    unittest.main()
