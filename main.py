import time
import loki
from plugins.sniff import SniffPlugin
from plugins.scan import ScanPlugin
from plugins.ai_brain import AIBrain

class LokiState:
    def __init__(self):
        self.tick = 0

def load_plugins():
    return [
        SniffPlugin(),
        ScanPlugin(),
        AIBrain(),
    ]

def main():
    state = LokiState()
    plugins = load_plugins()

    for p in plugins:
        p.on_start(loki)

    while True:
        state.tick += 1

        # TODO: call C wifi_scan() and wifi_sniff() once we expose them
        aps = []      # placeholder
        frame = None  # placeholder

        for p in plugins:
            p.on_scan(aps)
            p.on_sniff(frame)
            p.on_tick(state)

        time.sleep(1)

if __name__ == "__main__":
    main()
