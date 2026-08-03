# plugins/main.py
"""
Safe display plugin for Loki.
Opens /dev/fb1 lazily in on_start, falls back to terminal, and exposes Plugin.
"""

import time
import threading
import traceback

# Try to import the project's Plugin base class; fall back to a minimal base if missing.
try:
    from .base import Plugin as BasePlugin
except Exception:
    class BasePlugin:
        def __init__(self, config=None):
            self.config = config

class DisplayPlugin(BasePlugin):
    def __init__(self, config=None):
        super().__init__(config)
        self.fb = None
        self.fb_dev = "/dev/fb1"
        self._stop = False
        self._thread = None
        self._lock = threading.Lock()
        self._frame = None

        # allow config override if provided as dict-like
        try:
            cfg = getattr(self, "config", None)
            if isinstance(cfg, dict):
                dev = cfg.get("display_device")
                if dev:
                    self.fb_dev = dev
        except Exception:
            pass

    def on_start(self, loki):
        try:
            try:
                from display import init_display
                _display = init_display(self.config)
                self.fb = _display.fb

                print(f"[DisplayPlugin] opened framebuffer {self.fb_dev}")
            except PermissionError:
                self.fb = None
                print(f"[DisplayPlugin] permission denied for {self.fb_dev}; using terminal fallback")
            except FileNotFoundError:
                self.fb = None
                print(f"[DisplayPlugin] framebuffer {self.fb_dev} not found; using terminal fallback")
            except Exception as e:
                self.fb = None
                print(f"[DisplayPlugin] failed to open {self.fb_dev}: {e}")

            self._stop = False
            self._thread = threading.Thread(target=self._render_loop, daemon=True)
            self._thread.start()
        except Exception:
            print("[DisplayPlugin] on_start failed:\n" + traceback.format_exc())

    def _render_loop(self):
        tick = 0
        while not self._stop:
            tick += 1
            try:
                with self._lock:
                    if self.fb:
                        try:
                            # Diagnostic write; replace with proper framebuffer bytes for real use
                            self.fb.write(f"HB {tick}\n".encode("utf-8"))
                        except Exception:
                            try:
                                self.fb.close()
                            except Exception:
                                pass
                            self.fb = None
                            print("[DisplayPlugin] framebuffer write failed; falling back to terminal")
                        else:
                            # no framebuffer available, but we keep the loop alive
                            time.sleep(5.0)
            except Exception:
                print("[DisplayPlugin] render loop exception:\n" + traceback.format_exc())
                time.sleep(5.0)

    def on_tick(self, state):
        print("[DisplayPlugin] tick", flush=True)
        try:
            if not state:
                return
            with self._lock:
                snapshot = {}
                for k in ("exp", "level", "age", "title"):
                    for plugin_state in state.values():
                        if isinstance(plugin_state, dict) and k in plugin_state:
                            snapshot[k] = plugin_state[k]
                            break
                if snapshot:
                    self._frame = f"[Display] {snapshot}"
        except Exception:
            print("[DisplayPlugin] on_tick exception:\n" + traceback.format_exc())

    def on_stop(self):
        try:
            self._stop = True
            if self._thread and self._thread.is_alive():
                self._thread.join(timeout=2.0)
        except Exception:
            pass
        try:
            if self.fb:
                try:
                    self.fb.close()
                except Exception:
                    pass
                self.fb = None
        except Exception:
            pass
        print("[DisplayPlugin] stopped")

# Export the class expected by the plugin loader
Plugin = DisplayPlugin
