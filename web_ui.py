"""Web interface for Loki: live display, chat, plugins and TOML settings."""

from __future__ import annotations

import ast
import collections
import io
import ipaddress
import json
import logging
import os
import re
import secrets
import tempfile
import threading
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - _load reports the Python 3.11 requirement
    tomllib = None  # type: ignore[assignment]

_SECRET_NAMES = {"api_key", "password", "secret", "token"}
_BARE_KEY = re.compile(r"^[A-Za-z0-9_-]+$")
_TEMPLATE = Path(__file__).with_name("templates") / "index.html"
_PLUGINS_DIR = Path(__file__).with_name("plugins")
_MAX_BODY = 1024 * 1024


def _is_secret(path: tuple[str, ...]) -> bool:
    return any(part.lower() in _SECRET_NAMES for part in path)


def _flatten_settings(data: dict, prefix: tuple[str, ...] = ()):
    for key, value in data.items():
        path = prefix + (key,)
        if isinstance(value, dict):
            yield from _flatten_settings(value, path)
        elif not _is_secret(path):
            yield path, value


def _parse_value(value: str, current):
    if isinstance(current, bool):
        return value.lower() == "true"
    if isinstance(current, int) and not isinstance(current, bool):
        return int(value)
    if isinstance(current, float):
        return float(value)
    if isinstance(current, list):
        parsed = ast.literal_eval(value)
        if not isinstance(parsed, list):
            raise ValueError("expected a list")
        return parsed
    return value


def _toml_key(key: str) -> str:
    return key if _BARE_KEY.match(key) else _toml_value(key)


def _toml_value(value) -> str:
    if isinstance(value, bool):
        return str(value).lower()
    if isinstance(value, str):
        escaped = (
            value.replace("\\", "\\\\")
            .replace('"', '\\"')
            .replace("\n", "\\n")
            .replace("\r", "\\r")
            .replace("\t", "\\t")
        )
        return f'"{escaped}"'
    if isinstance(value, list):
        return "[" + ", ".join(_toml_value(item) for item in value) + "]"
    return str(value)


def _dump_toml(data: dict) -> str:
    lines: list[str] = []

    def write_table(table: dict, path: tuple[str, ...] = ()) -> None:
        if path:
            lines.append(f"[{'.'.join(_toml_key(p) for p in path)}]")
        for key, value in table.items():
            if not isinstance(value, dict):
                lines.append(f"{_toml_key(key)} = {_toml_value(value)}")
        for key, value in table.items():
            if isinstance(value, dict):
                if lines:
                    lines.append("")
                write_table(value, path + (key,))

    write_table(data)
    return "\n".join(lines) + "\n"


def _set_value(data: dict, path: tuple[str, ...], value) -> None:
    target = data
    for key in path[:-1]:
        target = target[key]
    target[path[-1]] = value


class _LogBuffer(logging.Handler):
    """Keep the most recent log records for the Logs page."""

    def __init__(self, size: int = 300):
        super().__init__()
        self.records: collections.deque = collections.deque(maxlen=size)
        self.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s"))

    def emit(self, record):  # noqa: D401
        try:
            self.records.append(self.format(record))
        except Exception:
            pass


def _chat_reply(message: str, state: dict | None) -> tuple[str, str | None]:
    """Return (reply, interaction kind) for a chat message."""
    text = message.lower()
    mood = (state or {}).get("mood", "curious")
    stage = (state or {}).get("stage", "dragon")
    level = (state or {}).get("level", 0)
    if any(w in text for w in ("feed", "hungry", "food", "eat")):
        return f"*munches happily* Thanks! Feeling {mood} at level {level}.", "feed"
    if any(w in text for w in ("play", "game", "fun")):
        return "Let's play! *chases its own tail*", "play"
    if any(w in text for w in ("sleep", "rest", "tired")):
        return "Zzz... resting my wings for a bit.", "rest"
    if any(w in text for w in ("pet", "love", "good", "care", "hug")):
        return "*purrs smoke rings* I like you too.", "care"
    if any(w in text for w in ("status", "how are", "mood", "level", "stage")):
        return f"I'm a {stage}, level {level}, and feeling {mood}.", "talk"
    if any(w in text for w in ("hello", "hi", "hey")):
        return f"Hey there! I'm Loki, currently feeling {mood}.", "talk"
    return f"I'm listening. (Feeling {mood}.) Try asking about my status, or tell me to play, feed or rest.", "talk"


class ConfigWebUI:
    """Serve the Loki control panel (CSRF-protected for all state changes)."""

    def __init__(self, config_path, host: str = "127.0.0.1", port: int = 8080,
                 shared_state: dict | None = None, plugins: dict | None = None):
        self.config_path = Path(config_path)
        self.host = host
        self.port = port
        self.shared_state = shared_state if shared_state is not None else {}
        self.plugins = plugins if plugins is not None else {}
        self.client_network = ipaddress.ip_network("0.0.0.0/0")
        self.csrf_token = secrets.token_urlsafe(32)
        self.server: ThreadingHTTPServer | None = None
        self.thread: threading.Thread | None = None
        self.log_buffer = _LogBuffer()
        self._config_lock = threading.Lock()

    # -- configuration -------------------------------------------------
    def _load(self) -> dict:
        if tomllib is None:
            raise RuntimeError("The web UI requires Python 3.11 or later")
        with self.config_path.open("rb") as config_file:
            return tomllib.load(config_file)

    def _save(self, data: dict) -> None:
        fd, temporary_path = tempfile.mkstemp(
            dir=self.config_path.parent, prefix=f".{self.config_path.name}.", suffix=".tmp"
        )
        try:
            os.fchmod(fd, 0o600)
            with os.fdopen(fd, "w", encoding="utf-8") as temporary:
                temporary.write(_dump_toml(data))
            os.replace(temporary_path, self.config_path)
        except Exception:
            if os.path.exists(temporary_path):
                os.unlink(temporary_path)
            raise

    def _config_payload(self) -> dict:
        sections: dict[str, list] = {}
        for path, value in _flatten_settings(self._load()):
            section = ".".join(path[:-1]) or "(root)"
            if isinstance(value, bool):
                kind = "bool"
            elif isinstance(value, int):
                kind = "int"
            elif isinstance(value, float):
                kind = "float"
            elif isinstance(value, list):
                kind = "list"
            else:
                kind = "str"
            shown = json.dumps(value) if kind == "list" else value
            sections.setdefault(section, []).append(
                {"key": ".".join(path), "name": path[-1], "type": kind, "value": shown}
            )
        return {"sections": [{"name": n, "fields": f} for n, f in sections.items()]}

    def _apply_changes(self, changes: dict) -> int:
        with self._config_lock:
            data = self._load()
            known = {".".join(path): (path, current) for path, current in _flatten_settings(data)}
            for field, raw in changes.items():
                if field not in known:
                    raise ValueError(f"unknown setting: {field}")
                path, current = known[field]
                _set_value(data, path, _parse_value(str(raw), current))
            self._save(data)
        return len(changes)

    # -- plugins -------------------------------------------------------
    def _plugin_payload(self) -> list:
        config = self._load()
        configured = config.get("main", {}).get("plugins", {})
        names = {p.stem for p in _PLUGINS_DIR.glob("*.py") if not p.stem.startswith("_") and p.stem != "base"}
        names |= {k for k, v in configured.items() if isinstance(v, dict) and k not in {"loaders", "security"}}
        result = []
        for name in sorted(names):
            cfg = configured.get(name, {})
            cfg = cfg if isinstance(cfg, dict) else {}
            result.append({
                "name": name,
                "enabled": bool(cfg.get("enabled", True)),
                "running": name in self.plugins,
                "installed": (_PLUGINS_DIR / f"{name}.py").exists(),
                "settings": [
                    {"key": f"main.plugins.{name}.{k}", "name": k, "value": v}
                    for k, v in cfg.items()
                    if not _is_secret((k,)) and not isinstance(v, (dict, list))
                ],
            })
        return result

    def _set_plugin_enabled(self, name: str, enabled: bool) -> None:
        if not _BARE_KEY.match(name):
            raise ValueError("invalid plugin name")
        with self._config_lock:
            data = self._load()
            plugins = data.setdefault("main", {}).setdefault("plugins", {})
            if name in {"loaders", "security"}:
                raise ValueError("not a plugin")
            plugins.setdefault(name, {})["enabled"] = enabled
            self._save(data)

    # -- live state ----------------------------------------------------
    def _loki_state(self) -> dict | None:
        plugin = self.plugins.get("loki_animation")
        state = getattr(plugin, "state", None) if plugin else None
        if state is None:
            state = self.shared_state.get("loki_animation")
        return state

    def _frame_png(self) -> bytes | None:
        plugin = self.plugins.get("loki_animation")
        getter = getattr(plugin, "latest_frame_png", None)
        return getter() if callable(getter) else None

    def _status_payload(self) -> dict:
        state = self._loki_state() or {}
        return {
            "online": bool(state),
            "stage": state.get("stage", "Unknown"),
            "level": state.get("level", 0),
            "xp": state.get("xp", 0),
            "mood": state.get("mood", "Unknown"),
            "title": state.get("title", ""),
            "interactions": state.get("interactions", 0),
            "plugins": sorted(self.plugins),
            "display": bool(self._frame_png()),
        }

    def _chat(self, message: str) -> dict:
        reply, kind = _chat_reply(message, self._loki_state())
        plugin = self.plugins.get("loki_animation")
        if kind and plugin is not None and hasattr(plugin, "interact"):
            try:
                plugin.interact(kind)
            except Exception:
                logging.getLogger("loki").exception("chat interaction failed")
        return {"reply": reply, "interaction": kind, "status": self._status_payload()}

    def _page(self) -> str:
        page = _TEMPLATE.read_text(encoding="utf-8")
        return page.replace("__CSRF_TOKEN__", self.csrf_token)

    # -- http ----------------------------------------------------------
    def _handler(self):
        ui = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, fmt, *args):  # keep stdout quiet
                pass

            def _is_allowed_client(self) -> bool:
                try:
                    return ipaddress.ip_address(self.client_address[0]) in ui.client_network
                except (ValueError, AttributeError, TypeError, IndexError):
                    return False

            def _send(self, status, body: bytes, ctype: str):
                self.send_response(status)
                self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.end_headers()
                self.wfile.write(body)

            def _json(self, payload, status=HTTPStatus.OK):
                self._send(status, json.dumps(payload).encode("utf-8"), "application/json")

            def do_GET(self):
                if not self._is_allowed_client():
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return
                path = self.path.split("?", 1)[0]
                try:
                    if path == "/":
                        self._send(HTTPStatus.OK, ui._page().encode("utf-8"), "text/html; charset=utf-8")
                    elif path == "/api/status":
                        self._json(ui._status_payload())
                    elif path == "/api/config":
                        self._json(ui._config_payload())
                    elif path == "/api/plugins":
                        self._json({"plugins": ui._plugin_payload()})
                    elif path == "/api/logs":
                        self._json({"lines": list(ui.log_buffer.records)})
                    elif path == "/api/frame.png":
                        frame = ui._frame_png()
                        if frame is None:
                            self.send_error(HTTPStatus.NOT_FOUND)
                        else:
                            self._send(HTTPStatus.OK, frame, "image/png")
                    else:
                        self.send_error(HTTPStatus.NOT_FOUND)
                except (RuntimeError, OSError) as error:
                    self._json({"error": str(error)}, HTTPStatus.INTERNAL_SERVER_ERROR)

            def do_POST(self):
                if not self._is_allowed_client():
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return
                if self.headers.get("X-CSRF-Token", "") != ui.csrf_token:
                    self.send_error(HTTPStatus.FORBIDDEN)
                    return
                try:
                    length = int(self.headers.get("Content-Length", "0"))
                    if length > _MAX_BODY:
                        self.send_error(HTTPStatus.REQUEST_ENTITY_TOO_LARGE)
                        return
                    body = json.loads(self.rfile.read(length) or b"{}")
                    if not isinstance(body, dict):
                        raise ValueError("expected a JSON object")
                    path = self.path.split("?", 1)[0]
                    if path == "/api/config":
                        changes = body.get("changes")
                        if not isinstance(changes, dict):
                            raise ValueError("missing changes")
                        count = ui._apply_changes(changes)
                        self._json({"saved": count, "message": "Saved. Restart Loki to apply."})
                    elif path == "/api/plugins":
                        ui._set_plugin_enabled(str(body.get("name", "")), bool(body.get("enabled")))
                        self._json({"plugins": ui._plugin_payload()})
                    elif path == "/api/chat":
                        message = str(body.get("message", "")).strip()[:500]
                        if not message:
                            raise ValueError("empty message")
                        self._json(ui._chat(message))
                    else:
                        self.send_error(HTTPStatus.NOT_FOUND)
                except (ValueError, TypeError, SyntaxError, RuntimeError) as error:
                    self._json({"error": str(error)}, HTTPStatus.BAD_REQUEST)

        return Handler

    def start(self) -> None:
        logging.getLogger().addHandler(self.log_buffer)
        ThreadingHTTPServer.allow_reuse_address = True
        self.server = ThreadingHTTPServer((self.host, self.port), self._handler())
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def stop(self) -> None:
        logging.getLogger().removeHandler(self.log_buffer)
        if self.server:
            self.server.shutdown()
            self.server.server_close()
        if self.thread:
            self.thread.join(timeout=2)
