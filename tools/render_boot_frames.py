from PIL import Image, ImageDraw, ImageFont
from pathlib import Path
outdir = Path(__file__).resolve().parent.parent / 'assets' / 'boot_frames'
outdir.mkdir(parents=True, exist_ok=True)

ttf = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
try:
    if Path(ttf).exists():
        font = ImageFont.truetype(ttf, 48)
        small = ImageFont.truetype(ttf, 18)
    else:
        font = ImageFont.load_default()
        small = ImageFont.load_default()
except Exception:
    font = ImageFont.load_default()
    small = ImageFont.load_default()

w, h = 480, 320
img = Image.new("RGB", (w, h), (8, 12, 20))
draw = ImageDraw.Draw(img)
draw.text((w//2 - 80, h//2 - 40), "LOKI", font=font, fill=(255, 200, 0))
draw.text((w//2 - 120, h//2 + 10), "Initializing plugins...", font=small, fill=(200,200,200))
img.save(outdir / "frame_000.png")

for i in range(1, 31):
    bg = ((i*6) % 255, (30 + i*3) % 255, (60 + i*2) % 255)
    f = Image.new("RGB", (w, h), bg)
    d = ImageDraw.Draw(f)
    d.text((w//2 - 80, h//2 - 40), "LOKI", font=font, fill=(255,255,255))
    d.text((w//2 - 120, h//2 + 10), f"Boot step {i}", font=small, fill=(230,230,230))
    f.save(outdir / f"frame_{i:03d}.png")

print("Rendered", len(list(outdir.glob('*.png'))), "frames to", outdir)
