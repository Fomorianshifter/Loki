#!/bin/bash
# Loki Master Startup Script

echo "[LOKI] Initializing wireless interface..."
sudo ip link set wlan1 down
sudo iw dev wlan1 set type monitor
sudo ip link set wlan1 up

echo "[LOKI] Starting C core engine..."
/opt/loki/core/loki &

echo "[LOKI] Starting Python UI and Dragon Animation loop..."
cd /opt/loki
source .venv/bin/activate
python3 main.py &

echo "[LOKI] Loki appliance active!"
