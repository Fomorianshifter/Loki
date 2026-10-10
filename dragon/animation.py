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

        bob = int(math.sin(frame / 8.0) * 2)
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
        # A softly shaded cave wall gives the dragon some depth without
        # requiring a large background image on the device.
        wall_bottom = max(0, self.height - 55)
        for top in range(0, wall_bottom, 4):
            shade = top / max(1, wall_bottom)
            color = (12 + int(12 * shade), 18 + int(13 * shade), 38 + int(15 * shade))
            draw.rectangle((0, top, self.width, min(wall_bottom, top + 3)), fill=color)

        draw.polygon(
            [(0, 0), (0, 72), (30, 48), (57, 65), (91, 21), (119, 53),
             (154, 0), (self.width - 154, 0), (self.width - 120, 45),
             (self.width - 85, 18), (self.width - 54, 62), (self.width - 25, 40),
             (self.width, 68), (self.width, 0)],
            fill=(10, 15, 30),
        )
        draw.ellipse(
            (self.width // 2 - 105, 70, self.width // 2 + 105, wall_bottom + 70),
            fill=(24, 34, 57),
        )
        draw.rectangle((0, wall_bottom, self.width, self.height), fill=(30, 24, 40))
        draw.line(
            (0, wall_bottom, self.width, wall_bottom),
            fill=(97, 63, 54),
            width=2,
        )

        # A few drifting embers add life to the scene, but stay subdued.
        for i in range(9):
            x = (i * 71 + frame) % max(1, self.width)
            y = 66 + ((i * 43 - frame // 3) % max(1, wall_bottom - 66))
            radius = 1 if i % 3 else 2
            draw.ellipse((x, y, x + radius, y + radius), fill=(175, 113, 72))

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
        }.get(state.mood_name, (60, 170, 205))
        shadow = tuple(max(0, channel - 38) for channel in body_color)
        highlight = tuple(min(255, channel + 38) for channel in body_color)
        wing_flap = int(math.sin(frame / 4.0) * s(11))
        tail_sway = int(math.sin(frame / 7.0) * s(7))

        # Ground shadow anchors the dragon instead of letting it appear to float.
        draw.ellipse(
            (x - s(92), y + s(49), x + s(92), y + s(68)),
            fill=(18, 17, 25),
        )

        # Wings sit behind the body; ribs follow the membrane from the shoulder.
        for side in (-1, 1):
            wing = [
                (x + side * s(24), y - s(22)),
                (x + side * s(55), y - s(44) - wing_flap),
                (x + side * s(88), y - s(78) - wing_flap),
                (x + side * s(82), y - s(36) - wing_flap // 2),
                (x + side * s(68), y - s(8)),
                (x + side * s(59), y + s(17)),
                (x + side * s(42), y + s(2)),
            ]
            draw.polygon(wing, fill=(43, 91, 108), outline=(164, 207, 205))
            root = (x + side * s(27), y - s(20))
            for tip_index in (1, 2, 3, 4, 5):
                draw.line(
                    (root, wing[tip_index]),
                    fill=(113, 163, 164),
                    width=max(1, s(2)),
                )

        # Tail taper and animated tip, with small dorsal spines.
        tail_base = (x - s(27), y + s(39))
        tail_mid = (x - s(65), y + s(46) + tail_sway // 2)
        tail_tip = (x - s(103), y + s(36) + tail_sway)
        draw.line((tail_base, tail_mid), fill=shadow, width=max(3, s(19)))
        draw.line((tail_mid, tail_tip), fill=body_color, width=max(2, s(12)))
        draw.line((tail_base, tail_mid), fill=body_color, width=max(2, s(13)))
        draw.polygon(
            [(tail_tip[0] - s(4), tail_tip[1]), (tail_tip[0] - s(15), tail_tip[1] - s(9)),
             (tail_tip[0] - s(11), tail_tip[1] + s(5))],
            fill=highlight,
        )
        for i in range(4):
            spine_x = x - s(43 + i * 10)
            spine_y = y + s(28 + i * 2) + tail_sway // 3
            draw.polygon(
                [(spine_x - s(5), spine_y), (spine_x, spine_y - s(10)),
                 (spine_x + s(5), spine_y + s(2))],
                fill=(221, 164, 102),
            )

        # Back legs are partially hidden by the body.
        for side in (-1, 1):
            leg_x = x + side * s(25)
            draw.ellipse(
                (leg_x - s(17), y + s(35), leg_x + s(15), y + s(65)),
                fill=shadow,
                outline=highlight,
                width=max(1, s(2)),
            )
            draw.line(
                (leg_x - s(6), y + s(61), leg_x + s(10), y + s(63)),
                fill=(235, 207, 157),
                width=max(2, s(3)),
            )

        # Main torso with a shaded far side and a lighter chest.
        draw.ellipse(
            (x - s(42), y - s(18), x + s(42), y + s(65)),
            fill=body_color,
            outline=highlight,
            width=2,
        )
        draw.ellipse(
            (x + s(13), y - s(12), x + s(38), y + s(54)),
            fill=shadow,
        )
        draw.ellipse(
            (x - s(25), y + s(1), x + s(13), y + s(54)),
            fill=(190, 166, 123),
        )
        for plate in range(5):
            plate_y = y + s(10 + plate * 9)
            draw.arc(
                (x - s(23), plate_y - s(5), x + s(15), plate_y + s(7)),
                5,
                175,
                fill=(135, 113, 91),
                width=max(1, s(2)),
            )

        # Small overlapping scales catch the cave light along the shoulder.
        for row in range(4):
            for col in range(3):
                scale_x = x + s(19 + col * 7 + (row % 2) * 3)
                scale_y = y + s(2 + row * 9)
                draw.arc(
                    (scale_x - s(4), scale_y - s(3), scale_x + s(4), scale_y + s(4)),
                    195,
                    345,
                    fill=highlight,
                    width=max(1, s(2)),
                )

        # Neck and head overlap the chest naturally.
        draw.ellipse(
            (x - s(29), y - s(44), x + s(26), y - s(3)),
            fill=shadow,
            outline=highlight,
            width=max(1, s(2)),
        )
        draw.ellipse(
            (x - s(37), y - s(70), x + s(37), y),
            fill=body_color,
            outline=highlight,
            width=max(1, s(2)),
        )
        draw.ellipse(
            (x + s(12), y - s(65), x + s(34), y - s(15)),
            fill=shadow,
        )
        draw.ellipse(
            (x - s(32), y - s(25), x + s(25), y - s(1)),
            fill=highlight,
        )

        # Horns and short back spines add a more recognizable silhouette.
        for side in (-1, 1):
            horn = [
                (x + side * s(24), y - s(58)),
                (x + side * s(18), y - s(88)),
                (x + side * s(7), y - s(57)),
            ]
            draw.polygon(horn, fill=(221, 190, 128), outline=(255, 228, 174))
            draw.line((horn[0], horn[1]), fill=(255, 242, 205), width=max(1, s(2)))
        for i in range(4):
            spine_x = x - s(12 + i * 8)
            spine_y = y - s(12 - i * 4)
            draw.polygon(
                [(spine_x - s(4), spine_y), (spine_x, spine_y - s(11)),
                 (spine_x + s(5), spine_y + s(2))],
                fill=(221, 164, 102),
            )

        # Eye sockets, amber irises, pupils, and a brief natural blink.
        eye_color = (255, 211, 94) if state.mood_name != "sleepy" else (195, 176, 119)
        eye_y = y - s(39)
        blink = frame % 83 in (0, 1, 2)
        for eye_x in (x - s(14), x + s(14)):
            if blink:
                draw.line(
                    (eye_x - s(6), eye_y + s(5), eye_x + s(6), eye_y + s(5)),
                    fill=shadow,
                    width=max(2, s(3)),
                )
                continue
            draw.ellipse(
                (eye_x - s(7), eye_y - s(2), eye_x + s(7), eye_y + s(12)),
                fill=(31, 39, 40),
            )
            draw.ellipse(
                (eye_x - s(5), eye_y, eye_x + s(5), eye_y + s(10)),
                fill=eye_color,
            )
            draw.ellipse(
                (eye_x - s(2), eye_y + s(1), eye_x + s(2), eye_y + s(10)),
                fill=(32, 30, 28),
            )
            draw.ellipse(
                (eye_x - s(2), eye_y + s(1), eye_x, eye_y + s(3)),
                fill=(255, 255, 230),
            )

        # Muzzle, nostrils, and expression.
        draw.ellipse(
            (x - s(19), y - s(20), x + s(19), y - s(3)),
            fill=highlight,
        )
        for side in (-1, 1):
            nostril_x = x + side * s(9)
            draw.ellipse(
                (nostril_x - s(2), y - s(16), nostril_x + s(2), y - s(13)),
                fill=shadow,
            )
        mouth_y = y - s(15)
        if state.mood_name == "grumpy":
            draw.line(
                (x - s(12), mouth_y + s(3), x + s(12), mouth_y - s(2)),
                fill=(30, 30, 45),
                width=max(1, s(2)),
            )
        else:
            draw.arc(
                (x - s(12), mouth_y - s(1), x + s(12), mouth_y + s(10)),
                5,
                170,
                fill=(30, 30, 45),
                width=max(1, s(2)),
            )

        # Forearms sit over the chest and finish in pale claws.
        for side in (-1, 1):
            arm_x = x + side * s(29)
            draw.line(
                (arm_x, y + s(15), arm_x - side * s(4), y + s(43)),
                fill=shadow,
                width=max(5, s(12)),
            )
            draw.line(
                (arm_x - side * s(4), y + s(43), arm_x + side * s(4), y + s(45)),
                fill=(235, 207, 157),
                width=max(2, s(4)),
            )

        if state.stage == "adult":
            draw.text(
                (x - s(45), y + s(82)),
                "Ancient flame awakens",
                font=self._font,
                fill=(255, 170, 70),
            )
