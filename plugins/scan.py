from .base import Plugin
# existing imports...

class ScanPlugin(Plugin):
    def on_start(self, loki):
        print("[Scan] starting")

    def on_tick(self, state):
        # your scan logic here
        pass

Plugin = ScanPlugin
