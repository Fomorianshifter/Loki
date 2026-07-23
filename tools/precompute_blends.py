from PIL import Image
from pathlib import Path
outdir = Path(__file__).resolve().parent.parent / 'assets' / 'boot_blends'
srcdir = Path(__file__).resolve().parent.parent / 'assets' / 'boot_frames'
outdir.mkdir(parents=True, exist_ok=True)

# parameters
fade_steps = 8  # number of intermediate blended frames between consecutive frames
# load sorted frames
frames = sorted(srcdir.glob('*.png'))
if not frames:
    print("No source frames found in", srcdir)
    raise SystemExit(1)

imgs = [Image.open(p).convert('RGB') for p in frames]
# ensure consistent size
w,h = imgs[0].size
for i,im in enumerate(imgs):
    if im.size != (w,h):
        imgs[i] = im.resize((w,h))

# write original frames to blends folder (preserve)
for idx,im in enumerate(imgs):
    im.save(outdir / f"blend_{idx:04d}.png")

# precompute crossfades between consecutive frames
count = len(imgs)
out_index = count
for i in range(count - 1):
    a = imgs[i]
    b = imgs[i+1]
    for s in range(1, fade_steps+1):
        t = s / float(fade_steps + 1)
        try:
            blended = Image.blend(a, b, t)
        except Exception:
            # fallback: simple linear per-channel blend
            blended = Image.new('RGB', (w,h))
            pa = a.load(); pb = b.load(); pd = blended.load()
            for y in range(h):
                for x in range(w):
                    ra,ga,ba = pa[x,y]; rb,gb,bb = pb[x,y]
                    pd[x,y] = (int(ra*(1-t)+rb*t), int(ga*(1-t)+gb*t), int(ba*(1-t)+bb*t))
        blended.save(outdir / f"blend_{out_index:04d}.png")
        out_index += 1

print("Wrote", out_index, "preblended frames to", outdir)
