from .base import Plugin
# existing imports...

class SniffPlugin(Plugin):
    def on_start(self, loki):
        print("[Sniff] starting")

    def on_tick(self, state):
        # your sniff logic here
        pass

# Alias the descriptive class to the name the loader expects
Plugin = SniffPlugin
