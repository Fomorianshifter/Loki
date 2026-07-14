from .base import Plugin

class ScanPlugin(Plugin):
    def on_scan(self, aps):
        print(f"[Scan] saw {len(aps)} APs")
