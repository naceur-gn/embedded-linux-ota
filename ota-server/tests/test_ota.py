#!/usr/bin/env python3
"""
Tests for OTA Release Creation and Server.

Run with: python -m pytest tests/ -v
or: python tests/test_ota.py
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

from ota_server import OTAServer
from create_release import create_release, validate_version, calculate_sha256


class TestVersionValidation(unittest.TestCase):
    """Test version format validation."""

    def test_valid_version_1_0_0(self):
        self.assertTrue(validate_version("1.0.0"))

    def test_valid_version_1_10_5(self):
        self.assertTrue(validate_version("1.10.5"))

    def test_valid_version_10_2_3(self):
        self.assertTrue(validate_version("10.2.3"))

    def test_invalid_version_single_number(self):
        self.assertFalse(validate_version("1"))

    def test_invalid_version_two_parts(self):
        self.assertFalse(validate_version("1.0"))

    def test_invalid_version_alpha(self):
        self.assertFalse(validate_version("abc"))

    def test_invalid_version_non_numeric(self):
        self.assertFalse(validate_version("1.x.0"))

    def test_invalid_version_four_parts(self):
        self.assertFalse(validate_version("1.0.0.1"))

    def test_invalid_version_empty(self):
        self.assertFalse(validate_version(""))

    def test_invalid_version_leading_zero(self):
        self.assertFalse(validate_version("01.0.0"))


class TestSHA256(unittest.TestCase):
    """Test SHA-256 calculation."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.test_file = os.path.join(self.test_dir, "test.bin")

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_sha256_generated(self):
        with open(self.test_file, "wb") as f:
            f.write(b"test data")

        sha256 = calculate_sha256(self.test_file)
        self.assertIsNotNone(sha256)
        self.assertEqual(len(sha256), 64)

    def test_sha256_matches_actual(self):
        test_data = b"hello world"
        with open(self.test_file, "wb") as f:
            f.write(test_data)

        expected = hashlib.sha256(test_data).hexdigest()
        actual = calculate_sha256(self.test_file)

        self.assertEqual(actual, expected)

    def test_sha256_changes_with_content(self):
        with open(self.test_file, "wb") as f:
            f.write(b"version 1")

        sha256_v1 = calculate_sha256(self.test_file)

        with open(self.test_file, "wb") as f:
            f.write(b"version 2")

        sha256_v2 = calculate_sha256(self.test_file)

        self.assertNotEqual(sha256_v1, sha256_v2)

    def test_sha256_large_file(self):
        with open(self.test_file, "wb") as f:
            f.write(os.urandom(1024 * 1024))

        sha256 = calculate_sha256(self.test_file)
        self.assertEqual(len(sha256), 64)


class TestReleaseCreation(unittest.TestCase):
    """Test release creation tool."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.releases_dir = os.path.join(self.test_dir, "releases")
        self.image_file = os.path.join(self.test_dir, "test_image.bin")

        with open(self.image_file, "wb") as f:
            f.write(b"test firmware image data")

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_valid_release_created(self):
        result = create_release("1.0.0", self.image_file, releases_dir=self.releases_dir)
        self.assertTrue(result)

        release_dir = os.path.join(self.releases_dir, "1.0.0")
        self.assertTrue(os.path.exists(release_dir))
        self.assertTrue(os.path.exists(os.path.join(release_dir, "image.bin")))
        self.assertTrue(os.path.exists(os.path.join(release_dir, "metadata.json")))

    def test_metadata_contains_sha256(self):
        create_release("1.0.0", self.image_file, releases_dir=self.releases_dir)

        metadata_path = os.path.join(self.releases_dir, "1.0.0", "metadata.json")
        with open(metadata_path, "r") as f:
            metadata = json.load(f)

        self.assertIn("sha256", metadata)
        self.assertEqual(len(metadata["sha256"]), 64)

    def test_metadata_sha256_matches_image(self):
        create_release("1.0.0", self.image_file, releases_dir=self.releases_dir)

        metadata_path = os.path.join(self.releases_dir, "1.0.0", "metadata.json")
        with open(metadata_path, "r") as f:
            metadata = json.load(f)

        image_path = os.path.join(self.releases_dir, "1.0.0", "image.bin")
        actual_sha256 = calculate_sha256(image_path)

        self.assertEqual(metadata["sha256"], actual_sha256)

    def test_invalid_version_rejected(self):
        result = create_release("1.0", self.image_file, releases_dir=self.releases_dir)
        self.assertFalse(result)

    def test_missing_image_rejected(self):
        result = create_release("1.0.0", "/nonexistent/image.bin", releases_dir=self.releases_dir)
        self.assertFalse(result)

    def test_empty_version_rejected(self):
        result = create_release("", self.image_file, releases_dir=self.releases_dir)
        self.assertFalse(result)

    def test_release_directory_structure(self):
        create_release("1.0.0", self.image_file, releases_dir=self.releases_dir)

        release_dir = os.path.join(self.releases_dir, "1.0.0")
        self.assertTrue(os.path.isdir(release_dir))

        metadata_path = os.path.join(release_dir, "metadata.json")
        with open(metadata_path, "r") as f:
            metadata = json.load(f)

        self.assertEqual(metadata["version"], "1.0.0")
        self.assertEqual(metadata["image"], "image.bin")
        self.assertIn("size", metadata)
        self.assertIn("timestamp", metadata)


class TestOTAServer(unittest.TestCase):
    """Test OTA Server functionality."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.releases_dir = os.path.join(self.test_dir, "releases")
        os.makedirs(self.releases_dir)

        self.image_file = os.path.join(self.test_dir, "test_image.bin")
        with open(self.image_file, "wb") as f:
            f.write(b"test firmware image data")

        create_release("1.0.0", self.image_file, "revA", self.releases_dir)
        create_release("1.1.0", self.image_file, "revA", self.releases_dir)
        create_release("1.2.0", self.image_file, "revB", self.releases_dir)

        self.server = OTAServer(self.releases_dir)

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_loads_valid_releases(self):
        self.assertEqual(len(self.server.releases), 3)

    def test_finds_update_available(self):
        result = self.server.find_update("revA", "1.0.0")
        self.assertIsNotNone(result)
        version, metadata = result
        self.assertEqual(version, "1.1.0")

    def test_no_update_current_version(self):
        result = self.server.find_update("revA", "1.1.0")
        self.assertIsNone(result)

    def test_no_update_newer_version(self):
        result = self.server.find_update("revA", "1.2.0")
        self.assertIsNone(result)

    def test_incompatible_hardware(self):
        result = self.server.find_update("revC", "1.0.0")
        self.assertIsNone(result)

    def test_get_image_path(self):
        path = self.server.get_image_path("1.0.0")
        self.assertIsNotNone(path)
        self.assertTrue(os.path.exists(path))

    def test_get_nonexistent_image(self):
        path = self.server.get_image_path("2.0.0")
        self.assertIsNone(path)

    def test_get_release_metadata(self):
        metadata = self.server.get_release_metadata("1.0.0")
        self.assertIsNotNone(metadata)
        self.assertEqual(metadata["version"], "1.0.0")

    def test_get_all_releases(self):
        releases = self.server.get_all_releases()
        self.assertEqual(len(releases), 3)

    def test_version_comparison(self):
        self.assertTrue(self.server.is_newer_version("1.1.0", "1.0.0"))
        self.assertTrue(self.server.is_newer_version("2.0.0", "1.9.9"))
        self.assertFalse(self.server.is_newer_version("1.0.0", "1.0.0"))
        self.assertFalse(self.server.is_newer_version("1.0.0", "1.1.0"))


class TestMetadataValidation(unittest.TestCase):
    """Test metadata validation."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.server = OTAServer(self.test_dir)

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_valid_metadata(self):
        metadata = {
            "version": "1.0.0",
            "hardware_version": "revA",
            "image": "image.bin",
            "sha256": "abc123",
            "size": 1024
        }
        self.assertTrue(self.server.validate_metadata(metadata))

    def test_missing_version(self):
        metadata = {
            "hardware_version": "revA",
            "image": "image.bin",
            "sha256": "abc123",
            "size": 1024
        }
        self.assertFalse(self.server.validate_metadata(metadata))

    def test_missing_sha256(self):
        metadata = {
            "version": "1.0.0",
            "hardware_version": "revA",
            "image": "image.bin",
            "size": 1024
        }
        self.assertFalse(self.server.validate_metadata(metadata))

    def test_missing_image(self):
        metadata = {
            "version": "1.0.0",
            "hardware_version": "revA",
            "sha256": "abc123",
            "size": 1024
        }
        self.assertFalse(self.server.validate_metadata(metadata))

    def test_empty_sha256(self):
        metadata = {
            "version": "1.0.0",
            "hardware_version": "revA",
            "image": "image.bin",
            "sha256": "",
            "size": 1024
        }
        self.assertFalse(self.server.validate_metadata(metadata))


class TestServerIntegration(unittest.TestCase):
    """Integration test for the OTA server."""

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.releases_dir = os.path.join(self.test_dir, "releases")
        os.makedirs(self.releases_dir)

        self.image_file = os.path.join(self.test_dir, "test_image.bin")
        self.test_data = b"test firmware image data for integration test"
        with open(self.image_file, "wb") as f:
            f.write(self.test_data)

        create_release("1.0.0", self.image_file, "revA", self.releases_dir)
        create_release("1.1.0", self.image_file, "revA", self.releases_dir)

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_complete_flow(self):
        server = OTAServer(self.releases_dir)

        result = server.find_update("revA", "1.0.0")
        self.assertIsNotNone(result)

        version, metadata = result
        self.assertEqual(version, "1.1.0")

        image_path = server.get_image_path(version)
        self.assertIsNotNone(image_path)
        self.assertTrue(os.path.exists(image_path))

        with open(image_path, "rb") as f:
            downloaded = f.read()

        self.assertEqual(downloaded, self.test_data)

        expected_sha256 = hashlib.sha256(self.test_data).hexdigest()
        self.assertEqual(metadata["sha256"], expected_sha256)

        actual_sha256 = hashlib.sha256(downloaded).hexdigest()
        self.assertEqual(actual_sha256, metadata["sha256"])


if __name__ == "__main__":
    unittest.main()
