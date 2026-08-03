#!/usr/bin/env python3
"""
Robust main loop for Loki:
- safe display init with fallback
- plugin lifecycle management with exception handling
- shared_state aggregation for renderers/adapters
- graceful shutdown
"""

import importlib
import logging
import os
import time
import traceback
from pathlib import Path

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
logger = logging.getLogger("loki")

# Configurable values
PLUGINS_DIR = "plugins"
MAIN_LOOP_SLEEP = 0.1  # seconds


def discover_plugins():
    """
    Discover plugin modules in the plugins package by listing files.
    Returns dict name -> module instance (module object).
    """
    plugins = {}
    p = Path(PLUGINS_DIR)
    if not p.exists():
        logger.warning("Plugins directory not found: %s", PLUGINS_DIR)
        return plugins

    for py in p.glob("*.py"):
        name = py.stem
        if name.startswith("_"):
            continue
        try:
            mod = importlib.import_module(f"{PLUGINS_DIR}.{name}")
            plugins[name] = mod
        except Exception:
            logger.error("Failed to import plugin module %s:\n%s", name, traceback.format_exc())
    return plugins


def safe_call(method_name, plugin_name, plugin_obj, *args, **kwargs):
    """Call plugin method safely and log exceptions."""
    method = getattr(plugin_obj, method_name, None)
    if not callable(method):
        return None
    try:
        return method(*args, **kwargs)
    except Exception:
        logger.error("Plugin %s.%s failed:\n%s", plugin_name, method_name, traceback.format_exc())
        return None


class DisplayFallback:
    """
    Minimal fallback display object used when framebuffer cannot be opened.
    Renderers that check ``display.fb`` will get None and should fall back to
    terminal output or a no-op path.
    """

    def __init__(self):
        self.mode = "terminal"
        self.fb = None

    def write(self, *args, **kwargs):
        pass

    def close(self):
        pass


def init_display(config):
    """
    Try to initialise the display via display.init_display().
    On any failure return a DisplayFallback so the rest of the program can
    continue without a physical display attached.
    """
    # Resolve the framebuffer device path for diagnostic messages
    try:
        fb_dev = config.display().get("device", "/dev/fb1")
    except Exception:
        fb_dev = (config.get("device", "/dev/fb1") if isinstance(config, dict) else "/dev/fb1")

    try:
        import display as _display_module
        disp = _display_module.init_display(config)
        logger.info("Display initialised successfully (device=%s)", getattr(disp, "fb_dev", fb_dev))
        return disp
    except PermissionError:
        logger.warning(
            "Permission denied opening display device %s – check that your user is in the 'video' "
            "group or run with appropriate privileges. Falling back to terminal display.",
            fb_dev,
        )
        return DisplayFallback()
    except FileNotFoundError:
        logger.warning(
            "Display device %s not found – ensure the framebuffer driver is loaded "
            "(e.g. modprobe fbtft_device name=adafruit18 or equivalent). "
            "Falling back to terminal display.",
            fb_dev,
        )
        return DisplayFallback()
    except Exception:
        logger.error(
            "Unexpected error initialising display device %s:\n%s",
            fb_dev,
            traceback.format_exc(),
        )
        return DisplayFallback()


def main():
    from config import LokiConfig

    config_path = os.environ.get(
        "LOKI_CONFIG", os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.toml")
    )
    config = LokiConfig(config_path)

    # Discover and import plugin modules
    modules = discover_plugins()
    logger.info("Discovered plugin modules: %s", list(modules.keys()))

    # Instantiate plugin objects with per-plugin config
    plugin_configs = config.get("plugins", {})
    plugins = {}
    for name, module in modules.items():
        plugin_class = getattr(module, "Plugin", None)
        if not plugin_class:
            continue
        cfg = plugin_configs.get(name, {})
        try:
            instance = plugin_class(cfg)
            plugins[name] = instance
        except Exception:
            logger.exception("Failed to instantiate plugin %s", name)

    # Initialize display (safe) — outside the plugin loop
    display_obj = init_display(config)

    # Call on_start for each plugin (safe)
    for name, plugin in plugins.items():
        logger.info("Starting plugin %s", name)
        safe_call("on_start", name, plugin, None)

    # Main loop: aggregate plugin states and call on_tick
    shared_state = {}
    try:
        logger.info("Entering main loop")
        while True:
            shared_state.clear()
            for name, plugin in plugins.items():
                try:
                    st = getattr(plugin, "state", None)
                    if st is not None:
                        shared_state[name] = st
                except Exception:
                    logger.debug("Reading state from plugin %s failed", name)

            for name, plugin in plugins.items():
                safe_call("on_tick", name, plugin, shared_state)

            time.sleep(MAIN_LOOP_SLEEP)
    except KeyboardInterrupt:
        logger.info("Shutdown requested by user (KeyboardInterrupt)")
    except Exception:
        logger.error("Unhandled exception in main loop:\n%s", traceback.format_exc())
    finally:
        logger.info("Stopping plugins")
        for name, plugin in plugins.items():
            safe_call("on_stop", name, plugin)
        try:
            if hasattr(display_obj, "close"):
                display_obj.close()
        except Exception:
            logger.debug("Error closing display: %s", traceback.format_exc())
        logger.info("Shutdown complete")


if __name__ == "__main__":
    main()
