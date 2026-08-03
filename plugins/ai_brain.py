"""
AI Brain plugin for Loki.

Sends prompts to an OpenAI-compatible API (OpenAI, Ollama, local LLM servers)
and exposes results via the ``state`` dict that other plugins can read.

Configuration (config.toml, [plugins.ai_brain]):
  enabled     = true/false
  use_openai  = false          # set true and provide api_key to use OpenAI
  api_key     = ""             # OpenAI or compatible API key
  api_url     = "https://api.openai.com/v1/chat/completions"
  model       = "gpt-4o-mini"
  timeout     = 30             # HTTP request timeout in seconds
  auto_reply  = false
  debug       = false
"""

import logging
import threading
import time
import traceback

logger = logging.getLogger("loki.plugins.ai_brain")


class Plugin:
    def __init__(self, config=None):
        self.config = config or {}
        self.enabled = self.config.get("enabled", False)
        self.use_openai = self.config.get("use_openai", False)
        self.api_key = self.config.get("api_key", "")
        self.api_url = self.config.get(
            "api_url", "https://api.openai.com/v1/chat/completions"
        )
        self.model = self.config.get("model", "gpt-4o-mini")
        self.timeout = int(self.config.get("timeout", 30))
        self.auto_reply = self.config.get("auto_reply", False)
        self.debug = self.config.get("debug", False)

        self.state = {
            "last_prompt": None,
            "last_response": None,
            "error": None,
            "ready": False,
        }

        self._lock = threading.Lock()
        self._session = None  # requests.Session, lazily created

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _get_session(self):
        """Return a cached requests.Session, creating it on first call."""
        if self._session is None:
            try:
                import requests
                self._session = requests.Session()
                if self.api_key:
                    self._session.headers.update(
                        {"Authorization": f"******"}
                    )
                self._session.headers.update({"Content-Type": "application/json"})
            except ImportError:
                logger.error("[AIBrain] 'requests' library not installed; pip install requests")
                raise
        return self._session

    def _query(self, prompt):
        """
        Send *prompt* to the configured API and return the reply string.
        Raises on failure so callers can handle the error.
        """
        session = self._get_session()
        payload = {
            "model": self.model,
            "messages": [{"role": "user", "content": prompt}],
        }
        if self.debug:
            logger.debug("[AIBrain] POST %s  model=%s  prompt=%.80s…", self.api_url, self.model, prompt)

        response = session.post(self.api_url, json=payload, timeout=self.timeout)
        response.raise_for_status()

        data = response.json()
        choices = data.get("choices") or []
        if not choices:
            raise ValueError("API response contained no choices")
        content = choices[0].get("message", {}).get("content", "")
        return content.strip()

    # ------------------------------------------------------------------
    # Plugin lifecycle
    # ------------------------------------------------------------------

    def on_start(self, loki):
        if not self.enabled:
            logger.info("[AIBrain] Plugin disabled in config.")
            return

        if self.use_openai and not self.api_key:
            logger.warning(
                "[AIBrain] use_openai=true but api_key is empty; "
                "AI queries will fail until a key is provided."
            )

        try:
            import requests  # noqa: F401 – validate dependency is present
            with self._lock:
                self.state["ready"] = True
            logger.info(
                "[AIBrain] Started (use_openai=%s, model=%s, url=%s)",
                self.use_openai, self.model, self.api_url,
            )
        except ImportError:
            logger.error("[AIBrain] 'requests' is required; install it with: pip install requests")
            with self._lock:
                self.state["error"] = "requests not installed"

    def on_tick(self, shared_state):
        if not self.enabled:
            return
        if not self.state.get("ready"):
            return

        # auto_reply: pick up any prompt queued in shared_state by other plugins
        if self.auto_reply and shared_state:
            for plugin_name, pstate in shared_state.items():
                if not isinstance(pstate, dict):
                    continue
                prompt = pstate.get("ai_prompt")
                if not prompt:
                    continue
                logger.info("[AIBrain] Received prompt from plugin '%s'", plugin_name)
                self._handle_prompt(prompt)
                break  # process one prompt per tick

    def query(self, prompt):
        """
        Public method: send a prompt and return the response string.
        Thread-safe; can be called by other plugins.
        Returns None on error.
        """
        if not self.state.get("ready"):
            logger.warning("[AIBrain] query() called before plugin is ready")
            return None
        return self._handle_prompt(prompt)

    def _handle_prompt(self, prompt):
        try:
            reply = self._query(prompt)
            with self._lock:
                self.state["last_prompt"] = prompt
                self.state["last_response"] = reply
                self.state["error"] = None
            if self.debug:
                logger.debug("[AIBrain] Reply: %.120s…", reply)
            return reply
        except Exception:
            err = traceback.format_exc()
            logger.error("[AIBrain] Query failed:\n%s", err)
            with self._lock:
                self.state["last_prompt"] = prompt
                self.state["last_response"] = None
                self.state["error"] = err
            return None

    def on_stop(self):
        logger.info("[AIBrain] Stopping plugin")
        with self._lock:
            self.state["ready"] = False
        if self._session:
            try:
                self._session.close()
            except Exception:
                pass
            self._session = None
