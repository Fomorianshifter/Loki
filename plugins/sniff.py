from .base import Plugin

class SniffPlugin(Plugin):
    def on_start(self, loki):
        print("[Sniff] starting")

    def on_sniff(self, frame):
        # later: parse 802.11, detect handshakes
        pass
