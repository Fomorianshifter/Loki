import json
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from dragon.state import DragonConfig, DragonState, DragonStateStore


class DragonStateStoreTests(unittest.TestCase):
    def test_state_persists_across_loads(self):
        with tempfile.TemporaryDirectory() as td:
            state_path = Path(td) / "dragon_state.json"
            cfg = DragonConfig({"state_path": str(state_path), "persist": True})
            store = DragonStateStore(state_path, persist=True)

            state = store.load(cfg)
            result = state.interact("care")
            store.save(state)

            state2 = store.load(cfg)
            self.assertEqual(state2.xp, result["xp"])
            self.assertEqual(state2.interactions, 1)

    def test_no_persist_mode_does_not_write_state_file(self):
        with tempfile.TemporaryDirectory() as td:
            state_path = Path(td) / "dragon_state.json"
            cfg = DragonConfig({"state_path": str(state_path), "persist": False})
            store = DragonStateStore(state_path, persist=False)

            state = store.load(cfg)
            state.interact("talk")
            store.save(state)

            self.assertFalse(state_path.exists())

    def test_concurrent_saves_are_atomic(self):
        with tempfile.TemporaryDirectory() as td:
            state_path = Path(td) / "dragon_state.json"
            store = DragonStateStore(state_path, persist=True)
            states = [DragonState(xp=xp) for xp in range(20)]

            with ThreadPoolExecutor(max_workers=8) as executor:
                list(executor.map(store.save, states))

            saved = json.loads(state_path.read_text())
            self.assertIn(saved["xp"], range(20))
            self.assertEqual(list(Path(td).glob(".dragon_state.json.*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
