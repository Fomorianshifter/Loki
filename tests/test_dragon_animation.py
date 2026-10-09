import tempfile
import unittest
from pathlib import Path

from dragon.animation import DragonAnimator
from dragon.state import DragonState


class _UnicodeTitleState(DragonState):
    @property
    def title(self):
        return "Dragon — Stage"


class DragonAnimatorTests(unittest.TestCase):
    def test_render_returns_rgb_image_and_saves_png(self):
        animator = DragonAnimator({"width": 128, "height": 96, "fps": 5})
        state = DragonState(xp=10, mood=70)

        img = animator.render(state, frame=0)

        self.assertEqual(img.mode, "RGB")
        self.assertEqual(img.size, (128, 96))

        with tempfile.TemporaryDirectory() as td:
            out = Path(td) / "preview.png"
            img.save(out)
            self.assertTrue(out.exists())
            self.assertGreater(out.stat().st_size, 0)

    def test_render_tolerates_unicode_title_with_default_font(self):
        animator = DragonAnimator({"width": 96, "height": 64, "fps": 5})
        state = _UnicodeTitleState(xp=0, mood=65)
        img = animator.render(state, frame=0)
        self.assertEqual(img.mode, "RGB")

    def test_all_growth_stages_render_and_frames_animate(self):
        animator = DragonAnimator({"width": 128, "height": 96, "fps": 5})

        for xp in (0, 5, 25, 75):
            with self.subTest(xp=xp):
                state = DragonState(xp=xp)
                first = animator.render(state, frame=4)
                later = animator.render(state, frame=12)

                self.assertEqual(first.size, (128, 96))
                self.assertEqual(first.mode, "RGB")
                self.assertNotEqual(first.tobytes(), later.tobytes())


if __name__ == "__main__":
    unittest.main()
