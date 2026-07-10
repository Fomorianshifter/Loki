#!/usr/bin/env python3
"""
server.py - Loki Dragon UI Web Server
======================================
Simple Flask server that serves the Loki web UI on port 5000.
Designed to run on Orange Pi Zero 2W alongside the C firmware.

Usage:
    pip install flask
    python3 server.py

Then open:
    http://<orange-pi-ip>:5000
    http://localhost:5000
"""

import os
import json
import time
from flask import Flask, send_from_directory, jsonify, request

# ---------------------------------------------------------------------------
# App setup
# ---------------------------------------------------------------------------
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
app = Flask(__name__, static_folder=BASE_DIR)

# Track the server start time for uptime reporting
SERVER_START = time.time()

# Simulated system state (in production, read from shared memory / IPC)
system_state = {
    "core":    "ok",
    "tft":     "ok",
    "sdcard":  "ok",
    "flash":   "ok",
    "eeprom":  "ok",
    "flipper": "idle",
    "gpio":    "ok",
    "spi":     "ok",
    "i2c":     "ok",
    "pwm":     "ok",
}

# ---------------------------------------------------------------------------
# Static file routes
# ---------------------------------------------------------------------------

@app.route("/")
def index():
    """Serve the main HTML page."""
    return send_from_directory(BASE_DIR, "index.html")


@app.route("/<path:filename>")
def static_files(filename):
    """Serve static assets (CSS, JS, images, etc.)."""
    return send_from_directory(BASE_DIR, filename)


# ---------------------------------------------------------------------------
# REST API  (placeholder – wire to real C firmware via IPC / shared files)
# ---------------------------------------------------------------------------

@app.route("/api/status", methods=["GET"])
def api_status():
    """Return current system status for all subsystems."""
    uptime_seconds = int(time.time() - SERVER_START)
    return jsonify({
        "uptime": uptime_seconds,
        "subsystems": system_state,
    })


@app.route("/api/status/<subsystem>", methods=["GET"])
def api_subsystem_status(subsystem):
    """Return status for a single subsystem."""
    if subsystem not in system_state:
        return jsonify({"error": f"Unknown subsystem: {subsystem}"}), 404
    return jsonify({subsystem: system_state[subsystem]})


@app.route("/api/test/<subsystem>", methods=["POST"])
def api_run_test(subsystem):
    """
    Trigger a hardware test for the given subsystem.
    In production, this would invoke the compiled C binary or write
    a command to a named pipe / shared file that the C daemon reads.
    """
    valid = {"tft", "eeprom", "flash", "flipper", "gpio", "all"}
    if subsystem not in valid:
        return jsonify({"error": f"Unknown test: {subsystem}"}), 400

    # Placeholder: update status to 'running'
    if subsystem == "all":
        for key in system_state:
            system_state[key] = "ok"
        result = {"status": "ok", "message": "All tests passed (simulated)"}
    else:
        # Simulate a brief test
        system_state[subsystem] = "ok"
        result = {"status": "ok", "message": f"{subsystem} test passed (simulated)"}

    return jsonify(result)


@app.route("/api/mood", methods=["GET"])
def api_get_mood():
    """Return the current dragon mood (read from a state file if integrated)."""
    mood_file = os.path.join(BASE_DIR, ".dragon_mood")
    mood = "neutral"
    if os.path.exists(mood_file):
        with open(mood_file, "r") as f:
            mood = f.read().strip()
    return jsonify({"mood": mood})


@app.route("/api/mood", methods=["POST"])
def api_set_mood():
    """
    Set the dragon mood.  Body: {"mood": "happy"|"mean"|"neutral"}
    Writes to a state file that the UI can poll.
    """
    data = request.get_json(silent=True) or {}
    mood = data.get("mood", "neutral")
    if mood not in ("happy", "mean", "neutral"):
        return jsonify({"error": "Invalid mood. Use happy, mean, or neutral."}), 400

    mood_file = os.path.join(BASE_DIR, ".dragon_mood")
    with open(mood_file, "w") as f:
        f.write(mood)

    return jsonify({"mood": mood, "message": f"Mood set to {mood}"})


# ---------------------------------------------------------------------------
# Entrypoint
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    host = "0.0.0.0"
    port = 5000
    print("=" * 55)
    print("  🔥 Loki Dragon UI - Web Server")
    print("  Orange Pi Zero 2W Interactive System")
    print("=" * 55)
    print(f"  Serving UI at:  http://{host}:{port}")
    print(f"  Local access:   http://localhost:{port}")
    print(f"  Static root:    {BASE_DIR}")
    print("=" * 55)
    print("  Press Ctrl+C to stop")
    print()
    app.run(host=host, port=port, debug=False)
