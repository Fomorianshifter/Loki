"""Tests for the local configuration web UI helpers."""

import json
import tempfile
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from pathlib import Path

from web_ui import ConfigWebUI, _dump_toml, _flatten_settings, _parse_value


class TestConfigWebUI(unittest.TestCase):
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

    def test_save_is_reparseable(self):
        with tempfile.TemporaryDirectory() as directory:
            config_path = Path(directory) / "config.toml"
            config_path.write_text("enabled = true\n")
            ui = ConfigWebUI(config_path)
            ui._save({"enabled": False})
            self.assertEqual(ui._load(), {"enabled": False})

    def test_dump_roundtrips_nested_and_special_values(self):
        import tomllib

        data = {"a": 1, "s": 'q"\\x', "t": {"on": True, "xs": [1, 2], "x-y": {"f": 1.5}}}
        self.assertEqual(tomllib.loads(_dump_toml(data)), data)

    def _serve(self, directory, plugins=None):
        config_path = Path(directory) / "config.toml"
        config_path.write_text('[main]\nname = "loki"\ntoken = "keep"\n[main.plugins.demo]\nenabled = true\n')
        ui = ConfigWebUI(config_path, port=0, plugins=plugins)
        ui.start()
        return ui, f"http://127.0.0.1:{ui.server.server_address[1]}"

    def _post(self, ui, base, path, body, token=True):
        headers = {"Content-Type": "application/json"}
        if token:
            headers["X-CSRF-Token"] = ui.csrf_token
        return urlopen(Request(base + path, json.dumps(body).encode(), headers))

    def test_serves_page_and_config(self):
        with tempfile.TemporaryDirectory() as directory:
            ui, base = self._serve(directory)
            try:
                page = urlopen(base + "/").read().decode()
                self.assertIn(ui.csrf_token, page)
                self.assertIn("Chat with Loki", page)
                cfg = json.loads(urlopen(base + "/api/config").read())
                keys = [f["key"] for s in cfg["sections"] for f in s["fields"]]
                self.assertIn("main.name", keys)
                self.assertNotIn("main.token", keys)
            finally:
                ui.stop()

    def test_post_requires_csrf_and_saves(self):
        with tempfile.TemporaryDirectory() as directory:
            ui, base = self._serve(directory)
            try:
                with self.assertRaises(HTTPError) as ctx:
                    self._post(ui, base, "/api/config", {"changes": {"main.name": "x"}}, token=False)
                self.assertEqual(ctx.exception.code, 403)
                self._post(ui, base, "/api/config", {"changes": {"main.name": "odin"}})
                data = ui._load()
                self.assertEqual(data["main"]["name"], "odin")
                self.assertEqual(data["main"]["token"], "keep")
                with self.assertRaises(HTTPError) as ctx:
                    self._post(ui, base, "/api/config", {"changes": {"main.nope": "1"}})
                self.assertEqual(ctx.exception.code, 400)
            finally:
                ui.stop()

    def test_plugin_toggle(self):
        with tempfile.TemporaryDirectory() as directory:
            ui, base = self._serve(directory)
            try:
                self._post(ui, base, "/api/plugins", {"name": "demo", "enabled": False})
                self.assertFalse(ui._load()["main"]["plugins"]["demo"]["enabled"])
            finally:
                ui.stop()

    def test_chat_and_frame(self):
        class Plugin:
            state = {"stage": "Egg", "level": 1, "xp": 5, "mood": "Happy"}
            kinds = []

            def interact(self, kind):
                self.kinds.append(kind)

            def latest_frame_png(self):
                return b"\x89PNGdata"

        with tempfile.TemporaryDirectory() as directory:
            ui, base = self._serve(directory, {"loki_animation": Plugin()})
            try:
                reply = json.loads(self._post(ui, base, "/api/chat", {"message": "please feed me"}).read())
                self.assertIn("Happy", reply["reply"])
                self.assertEqual(Plugin.kinds, ["feed"])
                self.assertTrue(reply["status"]["display"])
                self.assertEqual(urlopen(base + "/api/frame.png").read(), b"\x89PNGdata")
            finally:
                ui.stop()


if __name__ == "__main__":
    unittest.main()
