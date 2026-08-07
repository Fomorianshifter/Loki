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
import time
import traceback
import os
from pathlib import Path
from web_ui import ConfigWebUI

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
logger = logging.getLogger("loki")

# Configurable values
PLUGINS_DIR = "plugins"
MAIN_LOOP_SLEEP = 0.0167  # seconds (60 FPS = ~16.67ms per frame for responsive UI)

def get_config_path():
    return Path(os.environ.get("LOKI_CONFIG_PATH", str(Path(__file__).with_name("config.toml"))))

def discover_plugins(custom_dir=None):
    # (Your existing code that sets up the standard plugin paths will be here)
    # Add this block to check if a custom directory was provided
    if custom_dir:
        # Code to add your custom_dir to the list of places it looks for plugins
        pass
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

def instantiate_plugins(modules, config=None):
    plugin_configs = (config or {}).get("plugins", {})
    instances = {}

    # modules is expected to be a dict mapping name -> module
    if isinstance(modules, dict):
        iterable = list(modules.items())
    else:
        # fallback: build (name, module) pairs for a list of modules
        iterable = []
    for m in modules:
            name = getattr(m, "PLUGIN_NAME", None) or getattr(m, "__name__", None)
    if not name:
            name = getattr(m, "__file__", "unknown").split("/")[-1].split(".")[0]
            iterable.append((name, m))

    for name, module in iterable:
        if name in {"base", "__init__"}:
            continue
        plugin_class = getattr(module, "Plugin", None)
        if not plugin_class:
            continue
        cfg = plugin_configs.get(name, {})
        if isinstance(cfg, dict) and cfg.get("enabled") is False:
            logger.info("Plugin %s disabled in config; skipping", name)
            continue
        if name == "loki_animation":
            cfg = {"plugin": cfg, "dragon": (config or {}).get("dragon", {})}
        try:
            instances[name] = plugin_class(cfg)
        except Exception:
            logger.exception("Failed to instantiate plugin %s", name)

    return instances

def safe_call(method_name, plugin_name, plugin_obj, *args, **kwargs):
    """
    Call plugin method safely and log exceptions.
    """
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
    Renderers should detect absence of fb and use terminal or no-op.
    """
    def __init__(self):
        self.mode = "terminal"
    def write(self, *args, **kwargs):
        # no-op or simple print
        pass
    def close(self):
        pass

def init_display(config):
    """
    Try to open framebuffer device from config or default. On failure, return fallback.
    """
    display_config = config.get("display", {}) if isinstance(config, dict) else {}
    fb_dev = display_config.get("device", "/dev/fb1")
    try:
        # Import lazily so the configuration UI and non-display plugins still
        # work on headless installations where Pillow is not installed.
        from display import init_display as create_display

        _display = create_display(config)
        fb = _display.fb

        logger.info("Opened framebuffer device %s", fb_dev)
        return fb
    except PermissionError:
        logger.warning("Permission denied opening framebuffer %s; falling back to terminal display", fb_dev)
        return DisplayFallback()
    except FileNotFoundError:
        logger.warning("Framebuffer device %s not found; falling back to terminal display", fb_dev)
        return DisplayFallback()
    except Exception:
        logger.error("Unexpected error opening framebuffer %s:\n%s", fb_dev, traceback.format_exc())
        return DisplayFallback()
def main():
    import tomllib
    import threading
    from web_ui import ConfigWebUI  # <-- CHANGE THIS LINE

    config_file = get_config_path()
    with config_file.open("rb") as config_stream:
        config = tomllib.load(config_stream)

    shared_state = {}
    web_ui = None
    web_config = config.get("ui", {}).get("web", {})
    if web_config.get("enabled", False):
        try:
            from web_ui import ConfigWebUI
            web_ui = ConfigWebUI(
                config_file,
                shared_state=shared_state,
                host=web_config.get("host", "0.0.0.0"),
                port=web_config.get("port", 8080),
            )
            if hasattr(web_ui, "start"):
                web_ui.start()
            logger.info("Web UI started on port %d", web_config.get("port", 8080))
        except Exception:
            logger.exception("Failed to start Web UI")

    # Discover and import plugin modules
    try:
        modules = discover_plugins()
    except Exception:
        logger.exception("Failed to discover plugins")
        modules = []
    # --- CUSTOM LOKI PATHS ---
    custom_plugins_dir = config.get("main", {}).get("custom_plugins", "/opt/loki/custom_plugins")
    wordlist_path = config.get("main", {}).get("wordlist", "/opt/loki/wordlist.txt")

    logger.info("Using plugin directory: %s", custom_plugins_dir)
    logger.info("Using wordlist for checks: %s", wordlist_path)

    # Discover and import plugin modules
    modules = discover_plugins(custom_dir=custom_plugins_dir)
    # Discover and import plugin modules
    try:
        modules = discover_plugins()
    except Exception:
        logger.exception("Failed to discover plugins")
        modules = []
    # make the plugin registry available to external scripts
    # (place this after the loader populates the plugins dict/list)
    # if the loader uses a dict called 'plugins' or 'plugin_registry', expose it
    try:
        plugin_registry = globals().get("plugin_registry") or globals().get("plugins") or {}
    except Exception:
        plugin_registry = {}

    logger.info("Discovered plugin modules: %s", list(modules.keys()))
    # open main.py in your editor (or use sed/awk to insert)
    # Instantiate plugin objects with per-plugin config
    plugins = instantiate_plugins(modules, config)

    # Initialize display (safe)
    display = init_display(config)

    # Call on_start for each plugin (safe)
    for name, plugin in plugins.items():
        logger.info("Starting plugin %s", name)
        safe_call("on_start", name, plugin, None)

    # Main loop: aggregate plugin states and call on_tick
    shared_state = {}
    try:
        logger.info("Entering main loop (60 FPS)")
        while True:
            # Build shared_state from plugin instances that expose .state
            shared_state.clear()
            for name, plugin in plugins.items():
                # If plugin has a 'state' attribute, include it
                try:
                    st = getattr(plugin, "state", None)
                    if st is not None:
                        shared_state[name] = st
                except Exception:
                    logger.debug("Reading state from plugin %s failed", name)

            # Call on_tick for each plugin with shared_state
            for name, plugin in plugins.items():
                safe_call("on_tick", name, plugin, shared_state)

            # Sleep a short while; keep this outside plugin calls
            time.sleep(MAIN_LOOP_SLEEP)
    except KeyboardInterrupt:
        logger.info("Shutdown requested by user (KeyboardInterrupt)")
    except Exception:
        logger.error("Unhandled exception in main loop:\n%s", traceback.format_exc())
    finally:
        # Call on_stop for each plugin
        logger.info("Stopping plugins")
        for name, plugin in plugins.items():
            safe_call("on_stop", name, plugin)
        # Close display if it has close method or is a file
        try:
            if hasattr(display, "close"):
                display.close()
        except Exception:
            logger.debug("Error closing display: %s", traceback.format_exc())
        if web_ui:
            web_ui.stop()
        logger.info("Shutdown complete")

if __name__ == "__main__":
    main()
