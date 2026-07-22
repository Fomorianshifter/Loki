from .base import Plugin
import requests

class BettercapPlugin(Plugin):
    def on_start(self, loki):
        print("[Bettercap] starting")

    def on_tick(self, state):
        try:
            r = requests.get("http://localhost:8083/api/wifi/networks")
            if r.status_code == 200:
                data = r.json()
                aps = data.get("networks", [])
                print(f"[Bettercap] {len(aps)} APs detected")
            else:
                print("[Bettercap] API error:", r.status_code)
        except Exception as e:
            print("[Bettercap] error:", e)
