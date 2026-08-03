from PIL import Image, ImageDraw
import logging
import os
import threading
import time
import traceback

logger = logging.getLogger("loki.display")

# Guard and single shared instance for the framebuffer/display
_fb_opened = False
_display_instance = None
_display_lock = threading.Lock()


def _resolve_display_config(config):
    """
    Extract a flat display-config dict from either a LokiConfig object or a
    plain dict (e.g. the ``[plugins.display]`` section from config.toml).

    Priority:
      1. LokiConfig object  → calls config.display()
      2. Plain dict         → used directly (supports plugin passing its own cfg)
      3. Anything else      → empty dict, driver defaults take over
    """
    if config is None:
        return {}
    if hasattr(config, "display") and callable(getattr(config, "display", None)):
        try:
            result = config.display()
            return result if isinstance(result, dict) else {}
        except Exception:
            logger.debug("config.display() raised; falling back to empty config")
            return {}
    if isinstance(config, dict):
        return config
    return {}


def _diagnose_framebuffer(fb_dev):
    """
    Log diagnostic information that helps operators understand why a
    framebuffer device could not be opened.
    """
    logger.error("=== Display bring-up diagnostics for %s ===", fb_dev)

    # Check whether the device node exists at all
    if not os.path.exists(fb_dev):
        logger.error("  [✗] Device node %s does not exist.", fb_dev)
        logger.error("      • Verify the kernel driver is loaded:  lsmod | grep -E 'fbtft|ili9488|st7789'")
        logger.error("      • Load the driver manually (example):  sudo modprobe fbtft_device name=adafruit18")
        logger.error("      • Check dmesg for driver errors:        dmesg | tail -40")
    else:
        try:
            stat = os.stat(fb_dev)
            logger.error("  [✓] Device node exists (mode=%o, uid=%d, gid=%d)",
                         stat.st_mode, stat.st_uid, stat.st_gid)
        except Exception as exc:
            logger.error("  [?] Could not stat %s: %s", fb_dev, exc)

        # Check read/write access
        readable = os.access(fb_dev, os.R_OK)
        writable = os.access(fb_dev, os.W_OK)
        logger.error("  [%s] Readable: %s", "✓" if readable else "✗", readable)
        logger.error("  [%s] Writable: %s", "✓" if writable else "✗", writable)
        if not writable:
            logger.error("      • Add your user to the 'video' group:  sudo usermod -aG video $USER")
            logger.error("      • Then log out and back in, or:         newgrp video")

    # Check for other framebuffer devices that may be available
    try:
        fbs = [f for f in os.listdir("/dev") if f.startswith("fb")]
        if fbs:
            logger.error("  [i] Other framebuffer devices present: %s", fbs)
        else:
            logger.error("  [i] No framebuffer devices found under /dev.")
    except Exception:
        pass

    logger.error("=== End of display diagnostics ===")


def _convert_to_pixel_format(img, pixel_format):
    """
    Convert a PIL RGB image to raw bytes matching the framebuffer pixel format.

    Supported formats:
      "RGB565" (default) – 16bpp, 2 bytes/pixel, little-endian.
                           Required by fbtft SPI TFT drivers on Raspberry Pi.
      "RGB888"           – 24bpp, 3 bytes/pixel.  Use only when the framebuffer
                           is configured for 24-bit colour depth.
    """
    img = img.convert("RGB")
    fmt = (pixel_format or "RGB565").upper()

    if fmt == "RGB888":
        return img.tobytes()

    # RGB565: pack R[7:3] G[7:2] B[7:3] into a 16-bit little-endian word
    try:
        import numpy as np
        arr = np.asarray(img, dtype=np.uint16)
        r = arr[:, :, 0]
        g = arr[:, :, 1]
        b = arr[:, :, 2]
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        return rgb565.astype("<u2").tobytes()
    except ImportError:
        # Pure-Python fallback (slower, no numpy dependency required)
        pixels = list(img.getdata())
        buf = bytearray(len(pixels) * 2)
        for i, (r, g, b) in enumerate(pixels):
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            buf[i * 2] = v & 0xFF
            buf[i * 2 + 1] = (v >> 8) & 0xFF
        return bytes(buf)


class LokiDisplay:
    """
    LokiDisplay opens the framebuffer once and exposes simple drawing helpers.
    """

    def __init__(self, config):
        global _fb_opened, _display_instance
        # If another instance was created while this constructor was queued,
        # reuse it to avoid opening the device twice.
        if _fb_opened and _display_instance is not None:
            logger.debug("LokiDisplay.__init__: reusing existing display instance")
            existing = _display_instance
            self.width = existing.width
            self.height = existing.height
            self.animation = existing.animation
            self.fb_dev = existing.fb_dev
            self.pixel_format = existing.pixel_format
            self.fb = existing.fb
            return

        # Resolve config to a plain dict regardless of what was passed in
        disp_cfg = _resolve_display_config(config)

        self.width = disp_cfg.get("width", 480)
        self.height = disp_cfg.get("height", 320)
        self.animation = disp_cfg.get("animation", "boot_sequence")
        self.fb_dev = disp_cfg.get("device", "/dev/fb1")
        # pixel_format must match the framebuffer depth configured by the kernel driver.
        # fbtft SPI TFT displays default to 16bpp RGB565.
        self.pixel_format = disp_cfg.get("pixel_format", "RGB565")
        self.fb = None

        logger.info(
            "Initialising display: device=%s pixel_format=%s size=%dx%d",
            self.fb_dev, self.pixel_format, self.width, self.height,
        )

        try:
            self.fb = open(self.fb_dev, "wb", buffering=0)
            logger.info("Opened framebuffer device %s (%s)", self.fb_dev, self.pixel_format)
        except PermissionError as e:
            logger.error("Permission denied opening framebuffer %s: %s", self.fb_dev, e)
            _diagnose_framebuffer(self.fb_dev)
        except FileNotFoundError as e:
            logger.error("Framebuffer device %s not found: %s", self.fb_dev, e)
            _diagnose_framebuffer(self.fb_dev)
        except Exception as e:
            logger.error("Unexpected error opening framebuffer %s: %s", self.fb_dev, e)
            logger.debug(traceback.format_exc())
            _diagnose_framebuffer(self.fb_dev)

        _display_instance = self
        _fb_opened = True

    @property
    def available(self):
        """Return True if the underlying framebuffer is open and usable."""
        return self.fb is not None

    def draw_frame(self, img):
        if not self.fb:
            return
        try:
            raw = _convert_to_pixel_format(img, self.pixel_format)
            self.fb.write(raw)
            self.fb.flush()
        except Exception:
            logger.exception("Error writing frame to framebuffer")

    def boot_animation(self):
        for i in range(60):
            img = Image.new("RGB", (self.width, self.height), (i * 4 % 255, 0, 40))
            draw = ImageDraw.Draw(img)
            draw.text((10, 10), "Loki booting...", fill=(255, 255, 255))
            draw.text((10, 40), f"Step {i}", fill=(200, 200, 200))
            self.draw_frame(img)
            time.sleep(0.05)

    def show_plugin_status(self, active_plugins, enabled_plugins):
        img = Image.new("RGB", (self.width, self.height), (0, 0, 0))
        draw = ImageDraw.Draw(img)
        draw.text((10, 10), "Loki Plugins", fill=(0, 255, 0))
        y = 40
        for name in enabled_plugins:
            color = (255, 255, 255)
            if name in active_plugins:
                color = (0, 255, 0)
            draw.text((10, y), f"- {name}", fill=color)
            y += 20
        self.draw_frame(img)

    def close(self):
        try:
            if self.fb:
                self.fb.close()
                self.fb = None
        except Exception:
            logger.exception("Error closing framebuffer")


def init_display(config):
    """
    Return a single shared display instance. Thread-safe: prevents concurrent creation.
    """
    global _fb_opened, _display_instance

    # Fast path
    if _fb_opened and _display_instance is not None:
        logger.debug("Display already initialized; returning existing instance")
        return _display_instance

    # Slow path: ensure only one thread creates the instance
    with _display_lock:
        if _fb_opened and _display_instance is not None:
            logger.debug("Display initialized while waiting for lock; returning existing instance")
            return _display_instance

        disp = LokiDisplay(config)
        _display_instance = disp
        _fb_opened = True
        return _display_instance
