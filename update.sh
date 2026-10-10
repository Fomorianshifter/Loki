#!/bin/bash
# update.sh - Pull the latest Loki code and restart the running service.
#
# Loki runs as a persistent systemd service (loki.service) which loads the
# Python code into memory at startup, so a plain `git pull` is never enough
# on its own - the service must be restarted to pick up new code.

set -euo pipefail

# Directory the service actually runs from (matches WorkingDirectory in
# loki.service). Override with LOKI_DIR if you deployed elsewhere.
LOKI_DIR="${LOKI_DIR:-/opt/loki}"
SERVICE_NAME="${LOKI_SERVICE:-loki.service}"

echo "[LOKI] Updating Loki in $LOKI_DIR ..."

if [ ! -d "$LOKI_DIR/.git" ]; then
    echo "[LOKI] ERROR: $LOKI_DIR is not a git clone." >&2
    echo "[LOKI] The running service uses $LOKI_DIR - pull will not help unless" >&2
    echo "[LOKI] that directory itself is a clone of the repo. Re-clone or rsync it." >&2
    exit 1
fi

cd "$LOKI_DIR"

# Record whether requirements.txt changes so we know if deps need refreshing.
old_req_hash=""
new_req_hash=""
if [ -f requirements.txt ]; then
    old_req_hash=$(sha256sum requirements.txt | awk '{print $1}')
fi

echo "[LOKI] Pulling latest changes..."
git pull --ff-only

if [ -f requirements.txt ]; then
    new_req_hash=$(sha256sum requirements.txt | awk '{print $1}')
fi

# Refresh Python dependencies only when requirements.txt changed.
if [ -n "$new_req_hash" ] && [ "$old_req_hash" != "$new_req_hash" ]; then
    if [ -x .venv/bin/pip ]; then
        echo "[LOKI] requirements.txt changed - updating Python dependencies..."
        .venv/bin/pip install -r requirements.txt
    else
        echo "[LOKI] WARNING: requirements.txt changed but no venv found at $LOKI_DIR/.venv" >&2
        echo "[LOKI] Install dependencies manually before restarting." >&2
    fi
fi

# If the unit file itself changed, systemd needs to reload its definition.
if git diff --name-only HEAD@{1} HEAD 2>/dev/null | grep -q '^loki\.service$'; then
    if [ -f /etc/systemd/system/loki.service ]; then
        echo "[LOKI] loki.service changed - reinstalling unit and reloading systemd..."
        sudo cp loki.service /etc/systemd/system/loki.service
        sudo systemctl daemon-reload
    fi
fi

echo "[LOKI] Restarting $SERVICE_NAME ..."
sudo systemctl restart "$SERVICE_NAME"

# Show a brief status so the user can confirm the new version came up.
if sudo systemctl is-active --quiet "$SERVICE_NAME"; then
    echo "[LOKI] $SERVICE_NAME is running. Latest commit:"
    git log -1 --oneline
    echo "[LOKI] Done. Follow logs with: sudo journalctl -u $SERVICE_NAME -f"
else
    echo "[LOKI] ERROR: $SERVICE_NAME failed to start. Check logs:" >&2
    echo "[LOKI]   sudo journalctl -u $SERVICE_NAME -n 50" >&2
    exit 1
fi
