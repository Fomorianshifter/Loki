import tempfile
import threading
import time
import types
import unittest
import sys
from pathlib import Path
from unittest import mock

_REPO_ROOT = Path(__file__).resolve().parent.parent

if str(_REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(_REPO_ROOT))

import main
from dragon.state import DragonState
from plugins.loki_animation import LokiAnimationPlugin


class _DummyPlugin:
    def __init__(self, config=None):
        self.config = config or {}


class PluginLoaderTests(unittest.TestCase):
    def test_list_module_names_are_normalized(self):
        module = types.SimpleNamespace(Plugin=_DummyPlugin, __name__="plugins.ai_brain")
        named_module = types.SimpleNamespace(
            Plugin=_DummyPlugin, __name__="plugins.other", PLUGIN_NAME="custom_name"
        )

        instances = main.instantiate_plugins(
            [module, named_module],
            {"plugins": {"ai_brain": {"value": 1}, "custom_name": {"value": 2}}},
        )

        self.assertEqual(set(instances), {"ai_brain", "custom_name"})
        self.assertEqual(instances["ai_brain"].config, {"value": 1})
        self.assertEqual(instances["custom_name"].config, {"value": 2})

    def test_discover_plugins_loads_custom_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            plugin_path = Path(directory) / "custom_plugin.py"
            plugin_path.write_text(
                "PLUGIN_NAME = 'custom_named_plugin'\n"
                "class Plugin:\n"
                "    pass\n"
            )
            modules = main.discover_plugins(directory)

        self.assertIn("custom_named_plugin", modules)
        self.assertTrue(hasattr(modules["custom_named_plugin"], "Plugin"))

    def test_web_server_uses_configured_address(self):
        with tempfile.TemporaryDirectory() as directory:
            config_path = Path(directory) / "config.toml"
            config_path.write_text(
                '[ui.web]\nenabled = true\naddress = "10.0.0.3"\nport = 9000\n'
            )
            fake_ui = mock.Mock()
            with (
                mock.patch.object(main, "get_config_path", return_value=config_path),
                mock.patch("web_ui.ConfigWebUI", return_value=fake_ui) as web_ui_class,
                mock.patch.object(main, "discover_plugins", return_value={}),
                mock.patch.object(main, "init_display", return_value=mock.Mock()),
                mock.patch.object(main.time, "sleep", side_effect=KeyboardInterrupt),
            ):
                main.main()

        self.assertEqual(web_ui_class.call_args.kwargs["host"], "10.0.0.3")

    def test_interaction_saves_in_mutation_order(self):
        class BlockingStore:
            def __init__(self):
                self.first_save_started = threading.Event()
                self.release_first_save = threading.Event()
                self.snapshots = []

            def save(self, state):
                if not self.first_save_started.is_set():
                    self.first_save_started.set()
                    self.release_first_save.wait(timeout=2)
                self.snapshots.append(state.interactions)

        plugin = LokiAnimationPlugin({})
        plugin._state = DragonState(last_updated=time.time())
        store = BlockingStore()
        plugin._store = store
        first = threading.Thread(target=plugin.interact, args=("care",))
        second_started = threading.Event()

        def second_interaction():
            second_started.set()
            plugin.interact("talk")

        first.start()
        self.assertTrue(store.first_save_started.wait(timeout=2))
        second = threading.Thread(target=second_interaction)
        second.start()
        self.assertTrue(second_started.wait(timeout=2))
        try:
            time.sleep(0.05)
            self.assertEqual(plugin._state.interactions, 1)
        finally:
            store.release_first_save.set()
            first.join(timeout=2)
            second.join(timeout=2)

        self.assertEqual(store.snapshots, [1, 2])

    def test_loki_animation_plugin_starts_renders_and_stops_headlessly(self):
        rendered = threading.Event()
        plugin = LokiAnimationPlugin(
            {
                "plugin": {"enabled": True},
                "dragon": {
                    "enabled": True,
                    "persist": False,
                    "animation": {"width": 96, "height": 64, "fps": 30},
                },
            }
        )

        with (
            mock.patch("plugins.loki_animation._get_display", return_value=None),
            mock.patch(
                "dragon.animation.DragonAnimator.render",
                side_effect=lambda *_args: rendered.set(),
            ),
        ):
            plugin.on_start(None)
            try:
                self.assertTrue(rendered.wait(timeout=2))
                self.assertEqual(plugin.state["stage"], "egg")
                self.assertIsNotNone(plugin.interact("talk"))
                self.assertEqual(plugin.state["interactions"], 1)
            finally:
                thread = plugin._thread
                plugin.on_stop()

        self.assertIsNotNone(thread)
        self.assertFalse(thread.is_alive())


if __name__ == "__main__":
    unittest.main()
