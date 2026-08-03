

from .base import Plugin
import time
import requests
from requests.exceptions import RequestException

class BettercapPlugin(Plugin):
    def __init__(self, config=None):
        super().__init__(config)
        self.api_host = "127.0.0.1"
        self.api_port = 8081
        self._last_error_log = 0
        self._error_backoff = 1
        self._max_backoff = 60

    def on_start(self, loki):
        try:
            cfg = self.config.plugin_config("bettercap") or {}
            host = cfg.get("host")
            port = cfg.get("port")
            if host:
                self.api_host = host
            if port:
                self.api_port = int(port)
        except Exception:
            pass
        print(f"[Bettercap] using API http://{self.api_host}:{self.api_port}/api/wifi/networks")

    def on_tick(self, state):
        url = f"http://{self.api_host}:{self.api_port}/api/wifi/networks"
        headers = {"Accept": "application/json"}
        try:
            r = requests.get(url, headers=headers, timeout=3)
            if r.status_code == 200:
                data = r.json()
                aps = data.get("networks", [])
                print(f"[Bettercap] {len(aps)} APs detected")
                self._error_backoff = 1
            elif r.status_code == 405:
                now = time.time()
                if now - self._last_error_log > 30:
                    print(f"[Bettercap] API returned 405 Method Not Allowed for {url}. Try OPTIONS or check API docs.")
                    self._last_error_log = now
                self._error_backoff = min(self._error_backoff * 2, self._max_backoff)
            else:
                now = time.time()
                if now - self._last_error_log > 10:
                    print(f"[Bettercap] API error: HTTP {r.status_code}")
                    self._last_error_log = now
        except RequestException as e:
            now = time.time()
            if now - self._last_error_log > min(self._error_backoff, self._max_backoff):
                print(f"[Bettercap] error: {e}")
                self._last_error_log = now
                self._error_backoff = min(self._error_backoff * 2, self._max_backoff)
