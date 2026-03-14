# Loki Release Readiness

Use this before publishing a downloadable disk image or calling the current build "ready".

## Current Status

The compiled zip can be produced now.
The disk image path exists now.
The project is not yet at a clean "publish the .img" point.

## Release Gates

- Dragon visuals need another pass so the hatchling and adult forms read clearly instead of as a blob.
- Web UI needs another polish pass before it should be treated as the finished public-facing control surface.
- README and supporting docs need to be brought fully in line with the current flat repo layout, web UI fields, default tools, and `/api/networks/reset` endpoint.
- Final Raspberry Pi image validation needs to be done on a clean target from first boot through service startup.
- Final image generation should be run from a stable Linux host, not the current WSL setup that has been intermittently failing with resource errors.

## Definition Of Ready For A Downloadable Disk Image

The `.img.xz` is ready to publish when all of the following are true:

- The dragon art and animation are signed off.
- The web UI layout, copy, and controls are signed off.
- The README matches the shipped runtime behavior and packaging flow.
- `build_native.sh release` succeeds on the target Pi.
- `install_loki.sh` completes on a clean Pi image without manual recovery.
- `loki.service` starts on boot and stays healthy.
- `GET /api/status` responds after first boot.
- Standalone scout mode, discovery alerts, and network memory reset are verified on-device.
- The final image is built successfully with `build_loki_image.sh` on a stable Linux environment.

## Final Pre-Publish Checklist

- Build fresh release binary.
- Build fresh disk image.
- Flash disk image to test media.
- Boot test hardware from that media.
- Confirm SSH access.
- Confirm first-boot provisioning finishes.
- Confirm TFT display comes up.
- Confirm web UI loads.
- Confirm API responds.
- Confirm scout mode behavior.
- Confirm the packaged download artifacts are replaced with the final versions.

## Current Recommendation

Keep shipping the compiled zip for interim testing.
Hold the downloadable disk image until the dragon pass and web UI pass are done and the image has been validated end-to-end on a clean Raspberry Pi boot.