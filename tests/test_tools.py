import importlib.util
import json
import os
from pathlib import Path
import struct
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from unittest.mock import patch
import zlib
import gzip
import re

ROOT = Path(__file__).resolve().parents[1]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


packager = load("packager", ROOT / "tools/package_firmware.py")
fleet = load("fleet", ROOT / "fleet/server.py")


def image(board="esp32c3", app="test-app", version="2.0.1"):
    raw = bytearray(64)
    raw[0:2] = b"\xe9\x03"
    struct.pack_into("<H", raw, 12, 5)
    return bytes(raw) + struct.pack("<12s32s48s32s", packager.IDENTITY_MAGIC,
                                    board.encode(), app.encode(), version.encode()) + bytes(range(256))


class PackageTests(unittest.TestCase):
    def test_compressed_admin_page_matches_source(self):
        text = (ROOT / "src/DeviceAdminPage.h").read_text(encoding="utf-8")
        source = re.search(r'R"EDC\((.*)\)EDC";', text, re.S)[1].encode()
        header = (ROOT / "src/DeviceAdminPageGzip.h").read_text()
        compressed = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", header))
        self.assertEqual(gzip.decompress(compressed), source)

    def test_identity_and_roundtrip(self):
        data = packager.package(image())
        self.assertEqual(packager.inspect_package(data)["version"], "2.0.1")
        self.assertEqual(len(data), len(image()) + 256)

    def test_corruption_truncation_and_trailing_data(self):
        data = packager.package(image())
        for invalid in [data[:100], data[:-1], data + b"x", data[:300] + bytes([data[300] ^ 1]) + data[301:]]:
            with self.subTest(length=len(invalid)), self.assertRaises(ValueError):
                packager.inspect_package(invalid)

    def test_relabelled_package_does_not_pass_inspector(self):
        data = bytearray(packager.package(image()))
        data[88:120] = b"fake\0".ljust(32, b"\0")
        struct.pack_into("<I", data, 252, zlib.crc32(data[:252]))
        with self.assertRaises(ValueError):
            packager.inspect_package(data)

    def test_missing_ambiguous_and_wrong_chip(self):
        for invalid in [b"no identity", image() + image(app="another-app"), image(board="esp32")]:
            with self.subTest(length=len(invalid)), self.assertRaises(ValueError):
                packager.package(invalid)


class FakeDevice(BaseHTTPRequestHandler):
    response = {"version": "1.2", "online": False, "family": "expected", "csrf_token": "private-token", "password": "never-expose"}
    code = 200

    def do_GET(self):
        self.send_response(self.code)
        self.end_headers()
        self.wfile.write(json.dumps(self.response).encode())

    def log_message(self, *args):
        pass


class FleetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), FakeDevice)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown(); cls.server.server_close(); cls.thread.join()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name)
        self.device = {"id": "test", "name": "Test", "adapter": "json", "role": "Sensor",
                       "url": f"http://127.0.0.1:{self.server.server_port}",
                       "data_current_field": "online", "expected": {"family": "expected"}}
        self.config = self.path / "devices.json"
        self.config.write_text(json.dumps({"devices": [self.device]}))
        self.fleet = fleet.Fleet(self.config, self.path / "state.json")
        FakeDevice.code = 200

    def tearDown(self):
        self.temp.cleanup()

    def test_reachable_does_not_imply_current_and_secrets_are_removed(self):
        result = self.fleet.snapshot()
        device = result["devices"][0]
        self.assertTrue(device["reachable"])
        self.assertFalse(device["data_current"])
        self.assertTrue(device["last_seen"])
        self.assertNotIn("private-token", json.dumps(result))
        self.assertNotIn("never-expose", json.dumps(result))

    def test_identity_mismatch_not_recorded_as_valid_contact(self):
        self.device["expected"]["family"] = "other"
        result = self.fleet.probe(self.device)
        self.assertTrue(result["reachable"])
        self.assertIsNone(result["last_seen"])
        self.assertIn("inattesa", result["error"])

    def test_last_contact_survives_failure_and_restart(self):
        first = self.fleet.snapshot()["devices"][0]
        second = fleet.Fleet(self.config, self.path / "state.json")
        FakeDevice.code = 401
        result = second.probe(self.device)
        self.assertEqual(result["last_seen"], first["last_seen"])
        self.assertEqual(result["firmware_version"], "1.2")
        self.assertIn("Autenticazione", result["error"])

    def test_unknown_device_is_not_marked_offline(self):
        self.device["url"] = None
        self.assertIsNone(self.fleet.probe(self.device)["reachable"])

    def test_protected_http_server_is_reachable_without_sensor_claim(self):
        self.device["adapter"] = "http"
        for code in (401, 403):
            FakeDevice.code = code
            result = self.fleet.probe(self.device)
            self.assertTrue(result["reachable"])
            self.assertTrue(result["last_seen"])
            self.assertIsNone(result["data_current"])
            self.assertIsNone(result["error"])

    def test_public_and_credential_urls_are_refused(self):
        for url in ["https://127.0.0.1", "http://user:password@127.0.0.1", "http://8.8.8.8", "http://127.0.0.1/side-effect"]:
            with self.subTest(url=url), self.assertRaises(ValueError):
                fleet.validate_url(url)

    def test_core_credentials_stay_on_the_server(self):
        self.device.update(adapter="core", user_env="TEST_DEVICE_USER", password_env="TEST_DEVICE_PASSWORD")
        with patch.dict(os.environ, {"TEST_DEVICE_USER": "reader", "TEST_DEVICE_PASSWORD": "private-password"}):
            result = self.fleet.probe(self.device)
        self.assertNotIn("private-password", json.dumps(result))
        self.assertNotIn("csrf_token", result)

    def test_telemetry_is_allowlisted_and_reports_freshness(self):
        self.device.update(adapter="core", user_env="TEST_DEVICE_USER", password_env="TEST_DEVICE_PASSWORD",
                           telemetry_path="/api/status", telemetry_keys=["pv_w", "load_w", "fresh"], telemetry_current_field="fresh")
        data = dict(FakeDevice.response, pv_w=2450, load_w=710, fresh=True)
        with patch.object(FakeDevice, "response", data), patch.dict(os.environ, {"TEST_DEVICE_USER": "reader", "TEST_DEVICE_PASSWORD": "private-password"}):
            result = self.fleet.probe(self.device)
        self.assertEqual(result["telemetry"], {"pv_w": 2450, "load_w": 710, "fresh": True})
        self.assertTrue(result["data_current"])
        self.assertNotIn("csrf_token", result["telemetry"])

    def test_historical_error_does_not_make_recovered_device_unhealthy(self):
        self.device.update(adapter="core", user_env="TEST_DEVICE_USER", password_env="TEST_DEVICE_PASSWORD")
        data = dict(FakeDevice.response, last_error="Connessione Wi-Fi non riuscita")
        with patch.object(FakeDevice, "response", data), patch.dict(os.environ, {"TEST_DEVICE_USER": "reader", "TEST_DEVICE_PASSWORD": "private-password"}):
            result = self.fleet.probe(self.device)
        self.assertTrue(result["reachable"])
        self.assertIsNone(result["error"])
        self.assertEqual(result["last_error"], data["last_error"])


if __name__ == "__main__":
    unittest.main()
