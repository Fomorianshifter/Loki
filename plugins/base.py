class Plugin:
    def __init__(self, config=None):
        self.config = config

    def on_start(self, loki):
        pass

    def on_tick(self, state):
        pass

    def on_stop(self):
        pass

# Provide alternate names the loader might expect
LokiPlugin = Plugin
