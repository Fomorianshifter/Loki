# Loki Image Build

`build_loki_image.sh` creates a Raspberry Pi OS image that provisions Loki automatically on first boot.

What it does:
- takes a Raspberry Pi OS `.img`, `.img.xz`, or `.zip`
- injects the current Loki repository into the image
- enables SSH by default
- stages a `loki-firstboot.service` unit inside the image
- runs `install_loki.sh` automatically on first boot
- outputs a flashable `.img.xz`

Requirements:
- Linux host system
- root or `sudo`
- `tar`, `losetup`, `mount`, `umount`, `findmnt`, `xz`
- `curl` or `wget` if `--base-image` is a URL
- `unzip` only when the base image is a `.zip`

Example with a local Raspberry Pi OS image:

```bash
sudo ./build_loki_image.sh \
  --base-image ~/Downloads/raspios-bookworm-armhf-lite.img.xz
```

Example with a downloadable base image:

```bash
sudo ./build_loki_image.sh \
  --base-image https://downloads.raspberrypi.com/raspios_lite_armhf_latest \
  --output ./dist/loki-zero2w.img \
  --hostname loki-zero
```

Output:
- raw image path: the `--output` path you selected
- compressed image path: `--output` plus `.xz`

After the build completes:
1. Flash the generated `.img.xz` with Raspberry Pi Imager or `dd`.
2. Boot the Pi.
3. Wait for the first-boot installer to finish.
4. Verify with `ssh`, `systemctl status loki`, and `journalctl -u loki -f`.

Notes:
- The image builder uses the current repository state, not GitHub release contents.
- The first boot still performs package installation, so network access is required.
- The builder does not modify Wi-Fi credentials or user accounts.