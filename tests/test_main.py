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


if __name__ == "__main__":
    unittest.main()
