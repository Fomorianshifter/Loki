from __future__ import annotations

import os
import struct
from typing import Any, BinaryIO

try:
    from PIL import Image as PILImage
except ImportError:
    PILImage = None  # type: ignore

class LokiDisplay:
    """Write Pillow frames to a framebuffer device when one is available."""

    def __init__(
        self,
        width: int = 480,
        height: int = 320,
        pixel_format: str = "RGB565",
        framebuffer_path: str | None = None,
        ) -> None:
        self.width = max(1, int(width))
        self.height = max(1, int(height))
        self.pixel_format = str(pixel_format or "RGB").upper()
        self.framebuffer_path = framebuffer_path
        self.fb: BinaryIO | None = None
        self._buffer: bytearray | None = None
        self._bytes_per_pixel = 3 if self.pixel_format == "RGB" else 2
        self._open_framebuffer()

    def _open_framebuffer(self) -> None:
        candidates: list[str] = []
        if self.framebuffer_path:
            candidates.append(self.framebuffer_path)
        else:
            candidates.extend([os.environ.get("LOKI_FRAMEBUFFER", ""), "/dev/fb1", "/dev/fb0"])

        for candidate in candidates:
            if not candidate:
                continue
            try:
                fd = os.open(candidate, os.O_RDWR | os.O_CLOEXEC)
            except OSError:
                continue
            self.fb = os.fdopen(fd, "r+b", buffering=0)
            self._buffer = bytearray(self.width * self.height * self._bytes_per_pixel)
            return

        self.fb = None
        self._buffer = None

    @staticmethod
    def _is_image_like(image: Any) -> bool:
        return image is not None and hasattr(image, "size") and hasattr(image, "mode")

    def draw_frame(self, image: Any) -> bool:
        """Write *image* to the framebuffer if one is available.

        Returns ``True`` when a frame was written successfully, otherwise ``False``.
        """
        if self.fb is None or self._buffer is None:
            return False

        if self._is_image_like(image):
            img = image.convert("RGB")
            if img.size != (self.width, self.height):
                if PILImage is not None and isinstance(image, PILImage.Image):
                    resample = getattr(getattr(PILImage, "Resampling", None), "LANCZOS", None)
                    if resample is None:
                        resample = getattr(PILImage, "LANCZOS", None)
                    if resample is not None:
                        img = img.resize((self.width, self.height), resample=resample)
                    else:
                        img = img.resize((self.width, self.height))
                else:
                    img = img.resize((self.width, self.height))
        else:
            img = image

        if True:
            self._write_rgb565(img)
        else:
            self._write_rgb(img)
        return True

    def close(self) -> None:
        if self.fb is not None:
            try:
                self.fb.close()
            except OSError:
                pass
            self.fb = None
            self._buffer = None

    def __del__(self) -> None:
        self.close()

    def _write_rgb(self, image: Any) -> None:
        if self._is_image_like(image):
            pixels = image.tobytes()
        else:
            pixels = image
        if not isinstance(pixels, (bytes, bytearray)):
            return
        self._buffer[: len(pixels)] = pixels[: len(self._buffer)]  # type: ignore
        try:
            self.fb.seek(0)  # type: ignore
            self.fb.write(self._buffer)  # type: ignore
        except OSError:
            self.fb = None

    def _write_rgb565(self, image: Any) -> None:
        if self._is_image_like(image):
            img_rgb = image.convert("RGB")
            pixels = bytearray()
            for r, g, b in img_rgb.getdata():
                val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                pixels.extend(val.to_bytes(2, byteorder='little'))
        else:
            pixels = image

        if not isinstance(pixels, (bytes, bytearray)):
            return

        self._buffer[: len(pixels)] = pixels[: len(self._buffer)]  # type: ignore
        try:
            self.fb.seek(0)  # type: ignore
            self.fb.write(self._buffer)  # type: ignore
        except OSError:
            self.fb = None

def init_display(cfg: Any = None) -> LokiDisplay:
    """Create a display instance from a config-like object or mapping."""
    if cfg is None:
        return LokiDisplay()

    if hasattr(cfg, "display"):
        cfg = cfg.display()
    if not isinstance(cfg, dict):
        cfg = {}

    return LokiDisplay(
        width=cfg.get("width", 480),
        height=cfg.get("height", 320),
        pixel_format=cfg.get("pixel_format", "RGB"),
        framebuffer_path=cfg.get("framebuffer_path") or cfg.get("framebuffer") or cfg.get("path"),
    )


__all__ = ["LokiDisplay", "init_display"]
