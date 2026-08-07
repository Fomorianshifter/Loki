"""Asset-free Pillow animation for Loki's egg-to-adult life cycle.

Renders simple geometric shapes—no external art assets required.  The
animator is intentionally isolated from display hardware so frames can be
generated headlessly for PNG/GIF previews and unit-tested without a Pi.

Pixel format note
-----------------
``render()`` returns a Pillow ``Image`` in ``"RGB"`` mode (3 bytes/pixel).
Most Raspberry Pi framebuffers (e.g. ``/dev/fb1`` for SPI TFT screens) expect
``RGB565`` (2 bytes/pixel, 16-bit).  ``display.py`` handles the conversion via
the ``pixel_format`` config key in ``[plugins.display]``; set it to
``"RGB565"`` for Pi TFT hardware.  PNG/GIF export always uses ``"RGB"``.
"""

from __future__ import annotations

import math

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:  # pragma: no cover
    raise ImportError(
        "Pillow is required for dragon animation.  "
        "Install it with: sudo apt install -y python3-pil"
    ) from exc

from dragon.state import DragonState


class DragonAnimator:
    """Renders one RGB Pillow frame per call to :meth:`render`.

    All visual parameters come from the ``[dragon.animation]`` section of
    ``config.toml``.  Missing keys fall back to sensible defaults.
    """

    def __init__(self, cfg: dict | None = None):
        cfg = cfg or {}
        self.width: int = int(cfg.get("width", 480))
        self.height: int = int(cfg.get("height", 320))
        self.fps: int = max(1, int(cfg.get("fps", 10)))
        bg = cfg.get("background", [12, 18, 38])
        self.background: tuple[int, int, int] = (
            tuple(int(c) for c in bg[:3])  # type: ignore[assignment]
            if isinstance(bg, (list, tuple))
            else (12, 18, 38)
        )
        self._font = ImageFont.load_default()

    def render(self, state: DragonState, frame: int = 0) -> Image.Image:
        """Return one ``RGB`` Pillow image for the current dragon state.

        :param state: Current dragon stats to visualise.
        :param frame: Animation frame counter; increment each call to animate.
        :return: A Pillow ``Image`` in ``"RGB"`` mode (3 bytes/pixel).
        """
        img = Image.new("RGB", (self.width, self.height), self.background)
        draw = ImageDraw.Draw(img)

        self._draw_background(draw, frame)
        self._draw_status(draw, state)

        bob = int(math.sin(frame / 5.0) * 4)
        cx = self.width // 2
        cy = self.height // 2 + 35 + bob

        if state.stage == "egg":
            self._draw_egg(draw, cx, cy, state, frame)
        else:
            self._draw_dragon(draw, cx, cy, state, frame)

        return img

    # ------------------------------------------------------------------
    # Private helpers
    # ------------------------------------------------------------------

    def _draw_background(self, draw: ImageDraw.ImageDraw, frame: int) -> None:
        # Cave floor
        draw.rectangle(
            (0, self.height - 55, self.width, self.height),
            fill=(30, 24, 40),
        )
        draw.line(
            (0, self.height - 55, self.width, self.height - 55),
            fill=(97, 63, 54),
            width=2,
        )
        # Slowly drifting stars
        for i in range(16):
            x = (i * 71 + frame * 2) % self.width
            y = 55 + ((i * 43) % 135)
            draw.ellipse((x, y, x + 2, y + 2), fill=(130, 170, 255))

    def _draw_status(self, draw: ImageDraw.ImageDraw, state: DragonState) -> None:
        draw.text((12, 10), f"LOKI - {state.title}", font=self._font, fill="white")
        draw.text(
            (12, 28),
            f"Level {state.level}  XP {state.xp}  Mood: {state.mood_name}",
            font=self._font,
            fill=(180, 210, 255),
        )
        # Mood bar
        draw.rectangle((12, 48, 172, 60), outline=(210, 220, 255))
        bar_w = int(156 * max(0, min(100, state.mood)) / 100)
        bar_color = (70, 220, 120) if state.mood >= 55 else (240, 160, 55)
        draw.rectangle((14, 50, 14 + bar_w, 58), fill=bar_color)

    def _draw_egg(
        self,
        draw: ImageDraw.ImageDraw,
        x: int,
        y: int,
        state: DragonState,
        frame: int,
    ) -> None:
        # Egg body
        draw.ellipse(
            (x - 55, y - 75, x + 55, y + 75),
            fill=(75, 190, 165),
            outline=(225, 255, 235),
            width=3,
        )
        # Highlight
        draw.ellipse((x - 32, y - 55, x - 12, y - 25), fill=(120, 235, 205))

        # Cracks grow with XP toward the hatchling threshold
        hatch_xp = state._thresholds.get("hatchling", 5)
        crack_count = min(4, int(state.xp * 4 / max(1, hatch_xp)))
        for n in range(crack_count):
            off = n * 11 - 17
            draw.line(
                [(x + off, y - 22), (x + off + 8, y - 3), (x + off - 3, y + 17)],
                fill=(32, 70, 68),
                width=2,
            )

        # Pulsing glow
        pulse = int((math.sin(frame / 3.0) + 1) * 3)
        draw.arc(
            (x - 65 - pulse, y - 85 - pulse, x + 65 + pulse, y + 85 + pulse),
            190,
            350,
            fill=(255, 210, 80),
            width=2,
        )
        draw.text((x - 64, y + 95), "Keep Loki company to hatch!", font=self._font, fill=(255, 220, 130))

    def _draw_dragon(
        self,
        draw: ImageDraw.ImageDraw,
        x: int,
        y: int,
        state: DragonState,
        frame: int,
    ) -> None:
        scale_map = {"hatchling": 0.65, "juvenile": 0.90, "adult": 1.20}
        scale = scale_map.get(state.stage, 1.0)

        def s(v: float) -> int:
            return int(v * scale)

        body_color = {
            "happy": (75, 205, 130),
            "content": (60, 170, 205),
            "sleepy": (115, 110, 190),
            "grumpy": (185, 80, 92),
        }[state.mood_name]

        # Wings
        wing_flap = int(math.sin(frame / 3.0) * s(9))
        draw.polygon(
            [(x - s(28), y - s(25)), (x - s(85), y - s(65) - wing_flap), (x - s(60), y + s(25))],
            fill=(42, 105, 130),
            outline=(205, 245, 255),
        )
        draw.polygon(
            [(x + s(28), y - s(25)), (x + s(85), y - s(65) - wing_flap), (x + s(60), y + s(25))],
            fill=(42, 105, 130),
            outline=(205, 245, 255),
        )

        # Tail
        draw.line(
            (x - s(25), y + s(35), x - s(90), y + s(55)),
            fill=body_color,
            width=max(2, s(14)),
        )
        # Body
        draw.ellipse(
            (x - s(42), y - s(18), x + s(42), y + s(65)),
            fill=body_color,
            outline=(225, 255, 240),
            width=2,
        )
        # Head
        draw.ellipse(
            (x - s(37), y - s(70), x + s(37), y),
            fill=body_color,
            outline=(225, 255, 240),
            width=2,
        )

        # Horns
        draw.polygon(
            [(x - s(24), y - s(61)), (x - s(15), y - s(87)), (x - s(7), y - s(58))],
            fill=(255, 230, 155),
        )
        draw.polygon(
            [(x + s(24), y - s(61)), (x + s(15), y - s(87)), (x + s(7), y - s(58))],
            fill=(255, 230, 155),
        )

        # Eyes
        eye_color = (255, 220, 65) if state.mood_name != "sleepy" else (180, 180, 105)
        eye_y = y - s(39)
        draw.ellipse((x - s(20), eye_y, x - s(8), eye_y + s(11)), fill=eye_color)
        draw.ellipse((x + s(8), eye_y, x + s(20), eye_y + s(11)), fill=eye_color)

        # Mouth
        mouth_y = y - s(15)
        if state.mood_name == "grumpy":
            draw.line(
                (x - s(15), mouth_y, x + s(15), mouth_y - s(4)),
                fill=(30, 30, 45),
                width=2,
            )
        else:
            draw.arc(
                (x - s(15), mouth_y - s(5), x + s(15), mouth_y + s(12)),
                0,
                180,
                fill=(30, 30, 45),
                width=2,
            )

        if state.stage == "adult":
            draw.text(
                (x - s(45), y + s(82)),
                "Ancient flame awakens",
                font=self._font,
                fill=(255, 170, 70),
            )
