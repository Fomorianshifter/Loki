# Raspberry Pi UART + RS-485 Setup Guide

This guide covers how to enable the UART on a Raspberry Pi, connect an RS-485 HAT,
and configure the Loki firmware to use it.

## 1. Enable the UART

### Using `raspi-config` (recommended)

```bash
sudo raspi-config
```

Navigate to:

```
Interfacing Options → Serial Port
  Would you like a login shell to be accessible over serial? → No
  Would you like the serial port hardware to be enabled?     → Yes
```

Reboot after saving.

### Manual (`/boot/firmware/config.txt`)

Add to `/boot/firmware/config.txt` (or `/boot/config.txt` on older images):

```ini
enable_uart=1
```

Disable the serial console if it was previously enabled:

```ini
# In /boot/firmware/cmdline.txt — remove this part if present:
# console=serial0,115200
```

## 2. Verify the device path

After rebooting:

```bash
ls -l /dev/serial*
# /dev/serial0 -> ttyAMA0   (on Pi with Bluetooth disabled or Pi Zero)
# /dev/serial0 -> ttyS0     (on Pi 3/4/5 when Bluetooth is on ttyAMA0)

# Also check:
ls /dev/ttyAMA* /dev/ttyS*
```

The default device path used by the UART HAL is `/dev/serial0`.  
Override it in `uart_config_t.device_path` if needed, or at compile time:

```bash
make CFLAGS+='-DUART1_DEVICE_PATH=\"/dev/ttyAMA0\"'
```

## 3. Grant UART access without root (optional)

```bash
sudo usermod -aG dialout $USER
# Log out and back in for the group change to take effect.
```

## 4. Connect the RS-485 HAT

Most RS-485 HATs for Raspberry Pi wire directly onto the 40-pin header:

| Pi Pin | Function   | Notes                             |
|--------|------------|-----------------------------------|
| 8      | TXD (BCM14)| UART transmit → HAT A/D+ line    |
| 10     | RXD (BCM15)| UART receive ← HAT B/D− line     |
| 2 / 4  | 5 V        | Power (if HAT requires 5 V)      |
| 6      | GND        |                                   |

Check your HAT's datasheet for exact wiring.

## 5. RS-485 mode selection

The Loki UART HAL supports RS-485 through multiple control methods, with kernel
auto-direction attempted first and GPIO DE toggle as a fallback:

### Option A: Kernel auto-direction (preferred)

The Linux kernel can automatically toggle DE/RE via the RTS line.
This requires the UART driver to support `TIOCSRS485`.

On Raspberry Pi, the `ttyAMA0` (PL011) driver supports `TIOCSRS485` from
kernel 5.14 onwards. The miniUART (`ttyS0`) may not support it.

Enable by setting `rs485_enabled = 1` in `uart_config_t`:

```c
uart_config_t cfg = {
    .baud_rate      = 9600,
    .data_bits      = UART_DATA_BITS_8,
    .stop_bits      = UART_STOP_BITS_1,
    .parity         = UART_PARITY_NONE,
    .device_path    = "/dev/serial0",
    .rs485_enabled  = 1,
    /* GPIO fallback is disabled when kernel auto-direction succeeds */
};
uart_init(UART_PORT_1, &cfg);
```

### Option B: Manual GPIO DE/RE fallback

If the kernel ioctl is not supported by the HAT's driver, the HAL falls back
to toggling a GPIO pin as DE/RE.

```c
uart_config_t cfg = {
    .baud_rate             = 9600,
    .data_bits             = UART_DATA_BITS_8,
    .stop_bits             = UART_STOP_BITS_1,
    .parity                = UART_PARITY_NONE,
    .device_path           = "/dev/serial0",
    .rs485_enabled         = 1,
    .rs485_de_gpio_fallback = 1,
    .rs485_de_gpio_pin     = 4,   /* BCM GPIO4 (Pi pin 7) — adjust to your HAT */
    .rs485_turnaround_us   = 100, /* TX→RX turnaround in µs; 0 for none */
};
uart_init(UART_PORT_1, &cfg);
```

The build now auto-detects **libgpiod** (recommended on modern Raspberry Pi kernels).
If it is not installed, it falls back to legacy **sysfs GPIO**.
To force libgpiod support:

```bash
sudo apt-get install libgpiod-dev
make HAVE_LIBGPIOD=1
```

### Option C: No RS-485 (plain TTL/serial)

Leave `rs485_enabled = 0` (default when zero-initialised):

```c
uart_config_t cfg = {
    .baud_rate  = 115200,
    .data_bits  = UART_DATA_BITS_8,
    .stop_bits  = UART_STOP_BITS_1,
    .parity     = UART_PARITY_NONE,
};
uart_init(UART_PORT_1, &cfg);
```

## 6. Quick smoke test

### Loopback test (no RS-485 device needed)

Wire Pi **TXD (pin 8)** to **RXD (pin 10)** with a jumper wire.

```bash
# Terminal 1 — listen
cat /dev/serial0

# Terminal 2 — send
echo "hello loki" > /dev/serial0
```

You should see `hello loki` appear in terminal 1.

### RS-485 bus test with a known-good device

```bash
# Configure port (9600 8N1, no flow control)
stty -F /dev/serial0 9600 cs8 -cstopb -parenb -crtscts raw

# Send a byte to address 1 (Modbus-like ping — adjust for your protocol)
printf '\x01\x03\x00\x00\x00\x01\x84\x0A' > /dev/serial0

# Read response (pipe through xxd for hex)
timeout 1 cat /dev/serial0 | xxd
```

### Verify kernel RS-485 was applied

```bash
# Use setserial or check the kernel log after uart_init()
dmesg | grep -i rs485
```

## 7. Build and deploy

```bash
# Cross-compile from a Linux host (requires arm-linux-gnueabihf-gcc)
make DEBUG=0

# Or native build on the Pi itself
make DEBUG=0

# With libgpiod DE support
make DEBUG=0 HAVE_LIBGPIOD=1

# Deploy to the Pi
make install CROSS_USER=pi CROSS_HOST=raspberrypi.local CROSS_PATH=/tmp
```

## 8. Troubleshooting

| Symptom                            | Likely cause                                  | Fix                                             |
|------------------------------------|-----------------------------------------------|-------------------------------------------------|
| `open /dev/serial0: No such file`  | UART not enabled                              | Run `raspi-config`, enable serial port          |
| `open /dev/serial0: Permission denied` | User not in `dialout` group             | `sudo usermod -aG dialout $USER`, re-login      |
| `TIOCSRS485: Operation not supported` | Kernel/driver too old or miniUART       | Use GPIO fallback; or switch to `ttyAMA0`       |
| Garbled data                       | Baud/parity mismatch                          | Match settings with the other device            |
| TX works but no RX                 | DE stuck high (GPIO fallback)                 | Check `rs485_turnaround_us`, verify DE pin wiring|
| No data at all                     | Serial console still enabled on UART          | Remove `console=serial0` from `cmdline.txt`     |
