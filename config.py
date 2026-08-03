import toml

class LokiConfig:
    def __init__(self, path):
        self.data = toml.load(path)

    def get(self, key, default=None):
        return self.data.get(key, default)

    def system(self):
        return self.data.get("system", {})

    def brain(self):
        return self.data.get("brain", {})

    def display(self):
        return self.data.get("display", {})

    def enabled_plugins(self):
        # Expecting: [plugins] enabled = ["bettercap", "sniff", "scan", "ai_brain"]
        plugins = self.data.get("plugins", {})
        return plugins.get("enabled", [])

