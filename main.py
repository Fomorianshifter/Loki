import time
import loki
import toml

def load_config():
    return toml.load("config.toml")

class LokiState:
    def __init__(self):
        self.tick = 0

def load_plugins(config):
    enabled = config.get("plugins", {}).get("enabled", [])
    plugins = []

    for name in enabled:
        module = __import__(f"plugins.{name}", fromlist=["Plugin"])
        plugins.append(module.Plugin())

    return plugins

def main():
    config = load_config()
    state = LokiState()
    plugins = load_plugins(config)

    # Start plugins
    for p in plugins:
        if hasattr(p, "on_start"):
            p.on_start(loki)

    while True:
        state.tick += 1
        print("tick:", state.tick)

        aps = []      # placeholder until wifi_scan() is exposed
        frame = None  # placeholder until wifi_sniff() is exposed

        for p in plugins:
            if hasattr(p, "on_scan"):
                p.on_scan(aps)
            if hasattr(p, "on_sniff"):
                p.on_sniff(frame)
            if hasattr(p, "on_tick"):
                p.on_tick(state)

        time.sleep(1)
