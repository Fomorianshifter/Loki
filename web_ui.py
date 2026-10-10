"""Local-only web interface for inspecting and editing Loki's TOML settings."""

from __future__ import annotations

import ast
import html
import ipaddress
import json
import logging
import os
import secrets
import tempfile
import threading
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - _load reports the Python 3.11 requirement
    tomllib = None  # type: ignore[assignment]

_SECRET_NAMES = {"api_key", "password", "secret", "token"}

_logger = logging.getLogger(__name__)


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


def _toml_value(value) -> str:
    if isinstance(value, bool):
        return str(value).lower()
    if isinstance(value, str):
        escaped_val = (
            value.replace("\\", "\\\\")
            .replace('"', '\\"')
            .replace("\n", "\\n")
            .replace("\r", "\\r")
            .replace("\t", "\\t")
        )
        return f'"{escaped_val}"'
    if isinstance(value, list):
        return "[" + ", ".join(_toml_value(item) for item in value) + "]"
    return str(value)


def _dump_toml(data: dict) -> str:
    lines = []

    def write_table(table: dict, path: tuple[str, ...] = ()) -> None:
        if path:
            lines.append(f"[{'.'.join(path)}]")
        for key, value in table.items():
            if not isinstance(value, dict):
                lines.append(f"{key} = {_toml_value(value)}")
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


class ConfigWebUI:
    """Serve a CSRF-protected editor on loopback or Loki's USB network."""

    def __init__(self, config_path: str | Path, host: str = "127.0.0.1", port: int = 8080, shared_state: dict | None = None):
        self.config_path = Path(config_path)
        if not host or not host.strip():
            # Empty address in config.toml means "use the loopback default"
            # rather than failing web UI startup outright.
            host = "127.0.0.1"
        self.host = host
        self.port = port
        self.shared_state = shared_state if shared_state is not None else {}

        if host == "127.0.0.1":
            # Strictly local-only access.
            self.client_network = ipaddress.ip_network("127.0.0.0/8")
        else:
            # Pwnagotchi-style USB network: Loki is 10.0.0.2 and the
            # connected computer is 10.0.0.1.  Anything else is rejected so
            # the editor is never exposed to a public interface.
            try:
                address = ipaddress.ip_address(host)
            except ValueError:
                raise ValueError(f"Unsupported web UI host: {host!r}")
            usb_network = ipaddress.ip_network("10.0.0.0/24")
            if address not in usb_network:
                raise ValueError(
                    "Web UI host must be 127.0.0.1 or an address on the "
                    "10.0.0.0/24 USB network"
                )
            self.client_network = usb_network

        self.csrf_token = secrets.token_urlsafe(32)
        self.server: ThreadingHTTPServer | None = None
        self.thread: threading.Thread | None = None

    def _load(self) -> dict:
        if tomllib is None:
            raise RuntimeError("The local web UI requires Python 3.11 or later")
        with self.config_path.open("rb") as config_file:
            return tomllib.load(config_file)

    def _status_snapshot(self) -> dict:
        """Build the JSON payload for ``GET /api/status`` from shared state."""
        dragon = self.shared_state.get("loki_animation")
        if isinstance(dragon, dict):
            return {
                "stage": dragon.get("stage", "unknown"),
                "level": dragon.get("level", 0),
                "xp": dragon.get("xp", 0),
                "mood": dragon.get("mood", "unknown"),
            }
        return {"stage": "unknown", "level": 0, "xp": 0, "mood": "unknown"}

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

    def _page(self, message: str = "", data: dict | None = None) -> str:
        if data is None:
            data = self._load()
        snapshot = json.dumps(
            {".".join(path): _toml_value(value) for path, value in _flatten_settings(data)}
        )
        rows = []
        for path, value in _flatten_settings(data):
            field = ".".join(path)
            escaped_field = html.escape(field, quote=True)
            if isinstance(value, bool):
                control = (
                    f'<select name="{escaped_field}"><option value="true"'
                    f'{" selected" if value else ""}>true</option><option value="false"'
                    f'{" selected" if not value else ""}>false</option></select>'
                )
            else:
                control = f'<input name="{escaped_field}" value="{html.escape(str(value), quote=True)}">'
            rows.append(f"<tr><th>{escaped_field}</th><td>{control}</td></tr>")
        notice = f"<p class=\"notice\">{html.escape(message)}</p>" if message else ""
        return f"""<!doctype html>
<html><head><meta charset="utf-8"><title>Loki configuration</title>
<style>body{{font-family:sans-serif;max-width:900px;margin:2rem auto}}table{{border-collapse:collapse;width:100%}}th,td{{padding:.5rem;border-bottom:1px solid #ddd;text-align:left}}input{{width:100%;box-sizing:border-box}}.notice{{color:#075}}</style>
</head><body><h1>Loki configuration</h1>
<p><a href="/">Return to dashboard</a></p>
<p>Changes are saved to {html.escape(str(self.config_path))}. Fully restart Loki to apply them. Secrets are deliberately excluded; configure the WPA-SEC key with <code>LOKI_WPA_SEC_API_KEY</code>.</p>
{notice}<form method="post"><input type="hidden" name="csrf_token" value="{self.csrf_token}"><input type="hidden" name="_snapshot" value="{html.escape(snapshot, quote=True)}"><table>{''.join(rows)}</table><p><button type="submit">Save configuration</button></p></form></body></html>"""

    def _handler(self):
        # Capture the parent instance so the inner Handler class can access it safely
        server_instance = self

        class Handler(BaseHTTPRequestHandler):
            def _is_allowed_client(self) -> bool:
                try:
                    client_address = ipaddress.ip_address(self.client_address[0])
                except ValueError:
                    return False
                return client_address in server_instance.client_network

            def do_GET(self):
                if not self._is_allowed_client():
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return

                if self.path == "/api/status":
                    import json

                    self.send_response(HTTPStatus.OK)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    self.wfile.write(
                        json.dumps(server_instance._status_snapshot()).encode("utf-8")
                    )
                    return

                if self.path == "/":
                    self.send_response(HTTPStatus.OK)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.end_headers()
                    template_path = Path(__file__).parent / "templates" / "index.html"
                    self.wfile.write(template_path.read_bytes())
                    return

                if self.path == "/config":
                    self.send_response(HTTPStatus.OK)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.end_headers()
                    self.wfile.write(server_instance._page().encode("utf-8"))
                    return

                self.send_error(HTTPStatus.NOT_FOUND)

            def do_POST(self):
                if not self._is_allowed_client() or self.path != "/config":
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return

                length = int(self.headers.get("Content-Length", "0"))
                form = parse_qs(self.rfile.read(length).decode("utf-8"), keep_blank_values=True)

                if form.get("csrf_token", [""])[0] != server_instance.csrf_token:
                    self.send_error(HTTPStatus.FORBIDDEN)
                    return

                try:
                    data = server_instance._load()
                    snapshot = json.loads(form.get("_snapshot", [""])[0])
                    if not isinstance(snapshot, dict):
                        raise ValueError("invalid configuration snapshot")
                    stale_fields = []
                    for path, current in _flatten_settings(data):
                        field = ".".join(path)
                        if field in form and snapshot.get(field) != _toml_value(current):
                            stale_fields.append(field)
                    if stale_fields:
                        self.send_response(HTTPStatus.CONFLICT)
                        self.end_headers()
                        self.wfile.write(
                            b"Configuration changed since this form was loaded; reload and try again."
                        )
                        return
                    for path, current in _flatten_settings(data):
                        field = ".".join(path)
                        if field in form:
                            _set_value(data, path, _parse_value(form[field][0], current))
                    server_instance._save(data)
                    self.send_response(HTTPStatus.SEE_OTHER)
                    self.send_header("Location", "/config")
                    self.end_headers()
                except (ValueError, TypeError, RuntimeError) as error:
                    self.send_response(HTTPStatus.BAD_REQUEST)
                    self.end_headers()
                    self.wfile.write(str(error).encode('utf-8'))

        return Handler

    def start(self) -> None:
        ThreadingHTTPServer.allow_reuse_address = True
        try:
            self.server = ThreadingHTTPServer((self.host, self.port), self._handler())
        except OSError:
            if self.host == "127.0.0.1":
                raise
            # The configured USB-network address is not assigned to any local
            # interface (usb0 down or not configured yet).  Fall back to
            # loopback so the UI is still reachable on the device itself.
            self.client_network = ipaddress.ip_network("127.0.0.0/8")
            _logger.warning(
                "Web UI address %s unavailable; falling back to 127.0.0.1:%d",
                self.host,
                self.port,
            )
            self.host = "127.0.0.1"
            self.server = ThreadingHTTPServer((self.host, self.port), self._handler())
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def stop(self) -> None:
        if self.server:
            self.server.shutdown()
            self.server.server_close()
        if self.thread:
            self.thread.join(timeout=2)
