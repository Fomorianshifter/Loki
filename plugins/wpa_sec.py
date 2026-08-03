import time
import threading
import requests
import logging
import os

logger = logging.getLogger("loki.plugins.wpa_sec")


class Plugin:
    def __init__(self, config=None):
        self.config = config or {}
        self.enabled = self.config.get("enabled", False)

        # API key from config.toml
        self.api_key = self.config.get("api_key", "")

        # Behavior toggles
        self.auto_fetch = self.config.get("auto_fetch", False)
        self.auto_upload = self.config.get("auto_upload", False)
        self.interval = self.config.get("interval", 30)

        self.running = True
        self.thread = None

    def on_start(self, loki):
        if not self.enabled:
            logger.info("[WPA-SEC] Plugin disabled in config.")
            return

        if not self.api_key:
            logger.warning("[WPA-SEC] No API key provided.")
            return

        logger.info("[WPA-SEC] Starting WPA-SEC plugin...")
        logger.info("[WPA-SEC] Auto-fetch: %s, Auto-upload: %s", self.auto_fetch, self.auto_upload)

        # Background thread for auto-fetching
        if self.auto_fetch:
            self.thread = threading.Thread(target=self.auto_loop, daemon=True)
            self.thread.start()

    def auto_loop(self):
        while self.running:
            try:
                logger.info("[WPA-SEC] Auto-fetching handshakes...")
                self.fetch_handshakes()
            except Exception:
                logger.exception("[WPA-SEC] Error during auto-fetch")
            time.sleep(self.interval)

    def fetch_handshakes(self):
        """Fetch cracked or pending handshakes from WPA-SEC."""
        if not self.api_key:
            logger.warning("[WPA-SEC] No API key configured; skipping fetch.")
            return

        url = f"https://wpa-sec.stanev.org/?api={self.api_key}&download=1"
        logger.info("[WPA-SEC] Requesting handshake list...")
        try:
            response = requests.get(url, timeout=10)
            if response.status_code == 200:
                text = response.text or ""
                logger.info("[WPA-SEC] Handshake response length: %d", len(text))
                # Response may contain sensitive hash data; do not log its content
                # Optionally parse or save the response here
                # If auto_upload is enabled, implement safe upload triggers here
            else:
                logger.error("[WPA-SEC] Error fetching handshakes: HTTP %s", response.status_code)
        except Exception:
            logger.exception("[WPA-SEC] Request failed")

    def upload_handshake(self, file_path, max_size=5 * 1024 * 1024, retries=3, backoff=2):
        """Upload a handshake file to WPA-SEC with safety checks."""
        if not self.api_key:
            logger.warning("[WPA-SEC] Cannot upload: No API key.")
            return False

        if not os.path.isfile(file_path):
            logger.warning("[WPA-SEC] File not found: %s", file_path)
            return False

        size = os.path.getsize(file_path)
        if size > max_size:
            logger.warning("[WPA-SEC] File too large (%d bytes), skipping upload: %s", size, file_path)
            return False

        url = f"https://wpa-sec.stanev.org/?api={self.api_key}"
        logger.info("[WPA-SEC] Uploading handshake: %s (size=%d)", file_path, size)

        for attempt in range(1, retries + 1):
            try:
                with open(file_path, "rb") as f:
                    files = {"file": f}
                    response = requests.post(url, files=files, timeout=15)
                if response.status_code == 200:
                    logger.info("[WPA-SEC] Upload successful.")
                    return True
                else:
                    logger.error("[WPA-SEC] Upload failed: HTTP %s", response.status_code)
            except Exception:
                logger.exception("[WPA-SEC] Upload error on attempt %d", attempt)
            time.sleep(backoff ** attempt)

        logger.error("[WPA-SEC] Upload failed after %d attempts", retries)
        return False

    def on_tick(self, state):
        # Optional: show tick activity or react to shared state
        pass

    def on_stop(self):
        logger.info("[WPA-SEC] Stopping plugin...")
        self.running = False
        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=2)
