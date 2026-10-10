"""Tests for the local configuration web UI helpers."""

import io
import json
import tempfile
import unittest
from http import HTTPStatus
from unittest import mock
from urllib.request import urlopen
from pathlib import Path
from urllib.parse import urlencode

from web_ui import ConfigWebUI, _flatten_settings, _parse_value


class TestConfigWebUI(unittest.TestCase):
    def test_rejects_non_loopback_host(self):
        with self.assertRaises(ValueError):
            ConfigWebUI("config.toml", host="0.0.0.0")

    def test_empty_host_falls_back_to_loopback_default(self):
        ui = ConfigWebUI("config.toml", host="")
        self.assertEqual(ui.host, "127.0.0.1")
        self.assertEqual(str(ui.client_network), "127.0.0.0/8")

    def test_allows_pwnagotchi_style_usb_host(self):
        ui = ConfigWebUI("config.toml", host="10.0.0.2")
        self.assertEqual(str(ui.client_network), "10.0.0.0/24")

    def test_hides_secret_fields(self):
        fields = dict(
            _flatten_settings(
                {"plugins": {"wpa_sec": {"api_key": "do-not-display", "enabled": True}}}
            )
        )
        self.assertNotIn(("plugins", "wpa_sec", "api_key"), fields)
        self.assertTrue(fields[("plugins", "wpa_sec", "enabled")])

    def test_parses_existing_value_types(self):
        self.assertTrue(_parse_value("true", False))
        self.assertEqual(_parse_value("12", 1), 12)
        self.assertEqual(_parse_value("12.5", 1.0), 12.5)
        self.assertEqual(_parse_value("updated", "current"), "updated")
        self.assertEqual(_parse_value("[1, 2]", []), [1, 2])
        with self.assertRaises(ValueError):
            _parse_value("12abc", 1)
        with self.assertRaises(ValueError):
            _parse_value("12abc", 1.0)
        with self.assertRaises(ValueError):
            _parse_value("'not a list'", [])

    def test_post_rejects_invalid_csrf_without_roundtrip(self):
        ui = ConfigWebUI("config.toml")
        handler = ui._handler()
        form_body = b"csrf_token=invalid&enabled=false"
        request = type("Request", (), {})()
        request.path = "/config"
        request.headers = {"Content-Length": str(len(form_body))}
        request.rfile = io.BytesIO(form_body)
        request._is_allowed_client = lambda: True
        request.send_error = mock.Mock()

        with mock.patch.object(ui, "_load", side_effect=AssertionError("_load should not be called")):
            with mock.patch.object(
                ui, "_save", side_effect=AssertionError("_save should not be called")
            ):
                handler.do_POST(request)

        request.send_error.assert_called_once_with(HTTPStatus.FORBIDDEN)

    def test_post_valid_csrf_updates_config_without_roundtrip(self):
        ui = ConfigWebUI("config.toml")
        handler = ui._handler()
        form_body = urlencode(
            {
                "csrf_token": ui.csrf_token,
                "_snapshot": json.dumps({"enabled": "true"}),
                "enabled": "false",
            }
        ).encode("utf-8")
        request = type("Request", (), {})()
        request.path = "/config"
        request.headers = {"Content-Length": str(len(form_body))}
        request.rfile = io.BytesIO(form_body)
        request._is_allowed_client = lambda: True
        request.send_response = mock.Mock()
        request.send_header = mock.Mock()
        request.end_headers = mock.Mock()
        request.send_error = mock.Mock()

        with mock.patch.object(ui, "_load", return_value={"enabled": True}):
            with mock.patch.object(ui, "_save") as save_mock:
                handler.do_POST(request)

        request.send_error.assert_not_called()
        request.send_response.assert_called_once_with(HTTPStatus.SEE_OTHER)
        request.send_header.assert_called_once_with("Location", "/config")
        request.end_headers.assert_called_once()
        save_mock.assert_called_once_with({"enabled": False})

    def test_save_is_reparseable(self):
        with tempfile.TemporaryDirectory() as directory:
            config_path = Path(directory) / "config.toml"
            config_path.write_text("enabled = true\n")
            ui = ConfigWebUI(config_path)
            ui._save({"enabled": False})
            self.assertEqual(ui._load(), {"enabled": False})

    def test_save_escapes_newlines_in_strings(self):
        with tempfile.TemporaryDirectory() as directory:
            config_path = Path(directory) / "config.toml"
            config_path.write_text('value = "original"\n')
            ui = ConfigWebUI(config_path)
            value = "first line\nsecond line\rthird line\tend"
            ui._save({"value": value})
            self.assertEqual(ui._load(), {"value": value})

    def test_rejects_stale_form_values(self):
        ui = ConfigWebUI("config.toml")
        handler = ui._handler()
        form_body = urlencode(
            {
                "csrf_token": ui.csrf_token,
                "_snapshot": json.dumps({"enabled": "true"}),
                "enabled": "false",
            }
        ).encode("utf-8")
        request = type("Request", (), {})()
        request.path = "/config"
        request.headers = {"Content-Length": str(len(form_body))}
        request.rfile = io.BytesIO(form_body)
        request._is_allowed_client = lambda: True
        request.send_response = mock.Mock()
        request.send_header = mock.Mock()
        request.end_headers = mock.Mock()
        request.wfile = io.BytesIO()

        with mock.patch.object(ui, "_load", return_value={"enabled": False}):
            with mock.patch.object(ui, "_save") as save_mock:
                handler.do_POST(request)

        request.send_response.assert_called_once_with(HTTPStatus.CONFLICT)
        save_mock.assert_not_called()

    def test_serves_dashboard_and_configuration_on_loopback(self):
        with tempfile.TemporaryDirectory() as directory:
            config_path = Path(directory) / "config.toml"
            config_path.write_text("enabled = true\n")
            ui = ConfigWebUI(config_path, port=0)
            try:
                ui.start()
                port = ui.server.server_address[1]
                with urlopen(f"http://127.0.0.1:{port}/") as response:
                    dashboard = response.read().decode()
                with urlopen(f"http://127.0.0.1:{port}/config") as response:
                    config_page = response.read().decode()
                self.assertIn("LOKI // NODE_ACTIVE", dashboard)
                self.assertIn("/api/status", dashboard)
                self.assertIn('href="/config"', dashboard)
                self.assertIn("Loki configuration", config_page)
                self.assertIn("csrf_token", config_page)
            finally:
                ui.stop()

    def test_start_falls_back_to_loopback_when_address_unavailable(self):
        with tempfile.TemporaryDirectory() as directory:
            config_path = Path(directory) / "config.toml"
            config_path.write_text("enabled = true\n")
            # 10.0.0.2 is not assigned to any interface in the test
            # environment, so binding it would raise OSError (Errno 99).
            ui = ConfigWebUI(config_path, host="10.0.0.2", port=0)
            try:
                ui.start()
                self.assertEqual(ui.host, "127.0.0.1")
                self.assertEqual(str(ui.client_network), "127.0.0.0/8")
                port = ui.server.server_address[1]
                with urlopen(f"http://127.0.0.1:{port}/api/status") as response:
                    self.assertEqual(response.status, HTTPStatus.OK)
            finally:
                ui.stop()


if __name__ == "__main__":
    unittest.main()
