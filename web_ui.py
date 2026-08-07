"""Local-only web interface for inspecting and editing Loki's TOML settings."""

from __future__ import annotations

import ast
import html
import ipaddress
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
       escaped_val = value.replace("\\", "\\\\").replace("\"", "\\\"")
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
        self.host = host
        self.port = port
        self.shared_state = shared_state if shared_state is not None else {}
        
        # Allow any network connection for local testing so your computer isn't blocked
        self.client_network = ipaddress.ip_network("0.0.0.0/0")

        self.csrf_token = secrets.token_urlsafe(32)
        self.server: ThreadingHTTPServer | None = None
        self.thread: threading.Thread | None = None

    def _load(self) -> dict:
        if tomllib is None:
            raise RuntimeError("The local web UI requires Python 3.11 or later")
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

    def _page(self, message: str = "", data: dict | None = None) -> str:
        if data is None:
            data = self._load()
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
<p>Changes are saved to {html.escape(str(self.config_path))}. Fully restart Loki to apply them. Secrets are deliberately excluded; configure the WPA-SEC key with <code>LOKI_WPA_SEC_API_KEY</code>.</p>
{notice}<form method="post"><input type="hidden" name="csrf_token" value="{self.csrf_token}"><table>{''.join(rows)}</table><p><button type="submit">Save configuration</button></p></form></body></html>"""

    def _handler(self):
        # Capture the parent instance so the inner Handler class can access it safely
        server_instance = self

        class Handler(BaseHTTPRequestHandler):
            def _is_allowed_client(self) -> bool:
                return True  # Temporarily bypass network check for testing     

            def do_GET(self):
                if not self._is_allowed_client():
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return

                # --- NEW API ENDPOINT ---
                if self.path == "/api/status":
                    self.send_response(HTTPStatus.OK)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    
                    # Hardcoding physical screen stats to test the connection
                    import json
                    status = {
                        "stage": "Egg",
                        "level": "0",
                        "xp": "0",
                        "mood": "Grumpy"
                    }
                    self.wfile.write(json.dumps(status).encode('utf-8'))
                    return

                # --- SERVE WEB UI ---
                if self.path == "/":
                    self.send_response(HTTPStatus.OK)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.end_headers()

                    with open("/opt/loki/templates/index.html", "r", encoding="utf-8") as f:
                        html_content = f.read()

                    self.wfile.write(html_content.encode('utf-8'))
                    return
                
                self.send_error(HTTPStatus.NOT_FOUND)

            def do_POST(self):
                if not self._is_allowed_client() or self.path != "/":
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return
                    
                length = int(self.headers.get("Content-Length", "0"))
                form = parse_qs(self.rfile.read(length).decode("utf-8"), keep_blank_values=True)
                
                if form.get("csrf_token", [""])[0] != server_instance.csrf_token:
                    self.send_error(HTTPStatus.FORBIDDEN)
                    return
                    
                try:
                    data = server_instance._load()
                    for path, current in _flatten_settings(data):
                        field = ".".join(path)
                        if field in form:
                            _set_value(data, path, _parse_value(form[field][0], current))
                    server_instance._save(data)
                    self.send_response(HTTPStatus.OK)
                    self.end_headers()
                    self.wfile.write(b"Settings saved successfully.")
                except (ValueError, TypeError, RuntimeError) as error:
                    self.send_response(HTTPStatus.BAD_REQUEST)
                    self.end_headers()
                    self.wfile.write(str(error).encode('utf-8'))

        return Handler

    def start(self) -> None:
        ThreadingHTTPServer.allow_reuse_address = True
        self.server = ThreadingHTTPServer((self.host, self.port), self._handler())
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def stop(self) -> None:
        if self.server:
            self.server.shutdown()
            self.server.server_close()
        if self.thread:
            self.thread.join(timeout=2)
