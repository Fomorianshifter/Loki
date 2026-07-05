# Deployment Guide

This document describes a practical way to deploy Loki binaries to a target Linux SBC (commonly Orange Pi) using the current `Makefile` workflow.

## Scope and expectations

- This repository includes deployment helpers (`make install`, `make run`) and a useful service baseline.
- You should treat deployment as **environment-specific**: validate hardware wiring, Linux device access, and operational needs before long-running use.
- Avoid assuming this guide alone makes a system "production ready."

## Prerequisites

On the development machine:

- `make`
- `arm-linux-gnueabihf-gcc`
- SSH/SCP access to the target

On the target device:

- reachable over SSH
- permissions to execute the deployed binary
- required hardware interfaces enabled for your setup (SPI/I2C/UART/GPIO as needed)

## Configure target connection

Default Makefile deployment variables:

```bash
CROSS_USER=pi
CROSS_HOST=orange-pi.local
CROSS_PATH=/tmp
```

Override them per command when needed:

```bash
make DEBUG=0 install CROSS_USER=pi CROSS_HOST=192.168.1.100 CROSS_PATH=/tmp
```

## Deploy a release build

```bash
# build optimized binary
make clean
make DEBUG=0

# copy to target and set executable bit
make DEBUG=0 install
```

The release binary is `build/release/loki_app`.

## Run on target

```bash
# one-command deploy + remote run
make DEBUG=0 run
```

`run` executes the binary via SSH using `sudo` on the target host.

## Verify deployment quickly

After deployment, confirm:

1. binary exists at `CROSS_PATH` on target
2. process starts without immediate crash
3. logs show expected initialization progress
4. interfaces you depend on (display/storage/UART/etc.) behave as expected

Example checks:

```bash
ssh pi@orange-pi.local "ls -l /tmp/loki_app"
ssh pi@orange-pi.local "sudo /tmp/loki_app"
```

## Optional: systemd service baseline

For auto-start on boot, use a service file similar to:

```ini
[Unit]
Description=Loki service
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi
ExecStart=/usr/local/bin/loki_app
Restart=on-failure
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

Install and enable:

```bash
sudo cp loki_app /usr/local/bin/loki_app
sudo chmod +x /usr/local/bin/loki_app
sudo cp loki.service /etc/systemd/system/loki.service
sudo systemctl daemon-reload
sudo systemctl enable --now loki.service
sudo systemctl status loki.service
```

## Troubleshooting

### Compiler missing on dev machine

```bash
sudo apt-get install gcc-arm-linux-gnueabihf
```

### SSH/SCP connection failures

- verify host/IP and credentials
- test directly: `ssh pi@<host>`
- then retry `make install` with explicit `CROSS_HOST`

### Permission errors when running binary

Use `sudo` on the target for hardware interface access where required.

## Related docs

- `BUILD.md` for build-target details
- `README.md` for project scope and current-state overview
- `BUILD_WINDOWS.md` for Windows build workflow

## License

MIT License. See `LICENSE`.
