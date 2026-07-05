# Raspberry Pi Zero 2 W Quick Start

This guide is the beginner-friendly path for getting Loki onto a Raspberry Pi Zero 2 W with safe defaults.

## Before you start

You will need:

- Raspberry Pi Zero 2 W
- microSD card (8 GB minimum, 16 GB recommended)
- 5V power supply
- another computer for flashing the card

Recommended OS:

- **Raspberry Pi OS Lite (32-bit)** for the lowest overhead

For headless setup, use **Raspberry Pi Imager** and set these options before writing the SD card:

- hostname (for example `loki-pi`)
- enable SSH
- username and password
- Wi-Fi SSID, password, and country

Helpful official pointers:

- Raspberry Pi Imager: https://www.raspberrypi.com/software/
- Headless SSH documentation: https://www.raspberrypi.com/documentation/computers/remote-access.html

## First boot

1. Insert the SD card into the Pi Zero 2 W.
2. Power it on and wait 1-2 minutes.
3. SSH in from another machine:

```bash
ssh pi@loki-pi.local
```

If `.local` does not resolve, use your router-assigned IP address instead.

## Install Loki the friendly way

If you already cloned this repository on the Pi:

```bash
cd ~/Loki
./setup_pi.sh
```

The setup wizard will:

- run preflight checks
- offer to install build packages
- build Loki
- write a single `loki.env` config file
- install helper commands
- run a short smoke test
- optionally enable autostart only after that smoke test passes

## Install Loki in one command

If you want the non-interactive path from inside the repository:

```bash
cd ~/Loki
./install_loki_pi.sh --auto --enable-service
```

If you want the installer to clone or update the repo into `~/Loki`, run it from a checkout or a downloaded copy and choose an install directory:

```bash
./install_loki_pi.sh --auto --enable-service --install-dir "$HOME/Loki"
```

## Config file locations

The generated config file is plain English and uses safe defaults:

- `/etc/loki/loki.env` when setup has sudo-capable system access
- `~/.config/loki/loki.env` otherwise

The checked-in template is at:

```text
config/loki.env.example
```

## Service management commands

The installer creates friendly wrappers:

```bash
loki-start
loki-stop
loki-status
loki-logs
```

Equivalent raw systemd commands:

```bash
sudo systemctl start loki.service
sudo systemctl stop loki.service
sudo systemctl status loki.service --no-pager
sudo journalctl -u loki.service -f
```

## Safe mode and unsafe credit-write opt-in

Loki ships with these defaults on Raspberry Pi:

- `SAFE_MODE=1`
- `ENABLE_CREDIT_WRITE=0`

That means write-style credit-changing behavior is **off by default**.

If you intentionally want to opt in, edit the generated config file and set:

```bash
SAFE_MODE=0
ENABLE_CREDIT_WRITE=1
```

Or use the installer flag:

```bash
./install_loki_pi.sh --auto --allow-credit-write
```

Only do this if you fully understand the risk and want the write behavior on purpose.

## Troubleshooting

Check the service and logs:

```bash
loki-status
loki-logs
sudo journalctl -u loki.service -n 100 --no-pager
```

Run Loki manually with the generated config:

```bash
LOKI_CONFIG_FILE=/etc/loki/loki.env ./scripts/loki-run
```

If the build fails:

- confirm `make` is installed
- confirm a compiler is installed (`gcc` on-device is fine)
- re-run `./setup_pi.sh` and read the actionable preflight output

If the service will not enable:

- confirm the smoke test passed first
- confirm `LOKI_BIN` in `loki.env` points to a real executable
- confirm the Pi user has permission to access the checkout directory
