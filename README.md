# General
This is intel up 7000 device with buildroot
https://up-board.org/up-7000/


## Workspace operating defaults
- Device defaults used by workspace skills and automation:
  - `target_ip`: `192.168.55.1`
  - `target_prompt_regex`: `(?:<username>@<username>:.*[$#]|[$#]) ?$`
- Per top-level `README.md`, current device-side workflow often uses `.github/skills/terminal-command-inject` and `.github/skills/scp-file-copy`.

## Buildroot UP7000 image

This repo contains the current in-tree Buildroot board support for the UP7000:

- `buildroot-2024.02.9/configs/up7000_defconfig`
- `buildroot-2024.02.9/board/up7000/`

### Current image design

| Item | Value |
|---|---|
| Architecture | x86_64 |
| SoC | Intel Alder Lake-N |
| Kernel | Linux 6.6.63 |
| Init | BusyBox init |
| Bootloader | GRUB2 EFI |
| Rootfs | ext4 |
| Device manager | eudev |
| Network | static `eth0 = 192.168.55.1/24` |
| Installed credentials | `root` / `root` |

The generated image is `output/images/up7000.img`. It is a GPT disk image written to
the eMMC user area (`/dev/mmcblk0`) and contains:

| Partition | Type | Content |
|---|---|---|
| `mmcblk0p1` | 64 MiB FAT32 ESP | `EFI/BOOT/bootx64.efi`, `EFI/BOOT/grub.cfg`, `/bzImage` |
| `mmcblk0p2` | ext4 rootfs | Buildroot root filesystem |

Important storage note:

- The board has separate **SPI flash** for BIOS/UEFI firmware. We do **not** touch it.
- The eMMC boot areas (`/dev/mmcblk0boot0`, `/dev/mmcblk0boot1`) are **not** used by this GRUB EFI flow.
- Flashing `up7000.img` to `/dev/mmcblk0` is the correct deployment path.

### Build / rebuild

A fresh clone already contains the `buildroot-2024.02.9/` source tree, so no
separate tarball extraction step is required before running `make`.

```bash
REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
BUILDROOT="$REPO_ROOT/buildroot-2024.02.9"
OUTPUT="$REPO_ROOT/output"

# Configure
make -C "$BUILDROOT" O="$OUTPUT" up7000_defconfig

# Full build
make -C "$BUILDROOT" O="$OUTPUT"

# Rebuild after board/config/package changes
make -C "$BUILDROOT" O="$OUTPUT"

# Rebuild kernel after changing board/up7000/linux.config
make -C "$BUILDROOT" O="$OUTPUT" linux-dirclean linux
make -C "$BUILDROOT" O="$OUTPUT"
```

### Files that matter

| File | Why it matters |
|---|---|
| `buildroot-2024.02.9/configs/up7000_defconfig` | Main Buildroot configuration |
| `buildroot-2024.02.9/board/up7000/linux.config` | Full custom kernel config for UP7000 |
| `buildroot-2024.02.9/board/up7000/grub.cfg` | GRUB menu and kernel command line template |
| `buildroot-2024.02.9/board/up7000/genimage.cfg` | GPT image layout |
| `buildroot-2024.02.9/board/up7000/post-build.sh` | Removes SSH host keys and adds `tty1` getty |
| `buildroot-2024.02.9/board/up7000/post-image.sh` | Stamps the rootfs PARTUUID into GRUB and genimage output |
| `buildroot-2024.02.9/board/up7000/rootfs-overlay/etc/network/interfaces` | Static `eth0` config |
| `buildroot-2024.02.9/board/up7000/rootfs-overlay/etc/ssh/sshd_config` | SSH server policy |

### Critical findings / gotchas

1. **Rootfs must boot by `PARTUUID`, not by `/dev/disk/by-label/rootfs`.** The working image uses `root=PARTUUID=...` in GRUB, and `post-image.sh` replaces `UUID_TMP` with the real rootfs UUID during image generation.
2. **Do not use genimage `files = { "EFI/BOOT/..." }` for the ESP tree.** That strips directory structure and can place `bootx64.efi` at the FAT root. Preserve the whole directory with:
   ```txt
   file EFI {
       image = "efi-part/EFI"
   }
   ```
3. **`bzImage` must be in the ESP root as `/bzImage`.** GRUB loads it before the rootfs is mounted.
4. **HDMI needs a getty on `tty1`.** Serial-only getty on `ttyS0` makes a successful boot look hung on HDMI-only setups.
5. **BusyBox init + ifupdown is the current working setup.** Older notes that mention `systemd` or `systemd-networkd` are stale.
6. **The GRUB message `no suitable video mode found / Booting in blind mode` is not necessarily fatal.** The real blocker we hit was the kernel waiting for the root device.
7. **`GPT header not at the end of the disk` after writing the image to eMMC is expected.** The image is ~577 MiB and the target eMMC is much larger.
8. **The on-board Ethernet is Realtek RTL8111H-CG.** Keep `r8169` plus its PHY/MDIO stack available, and include `eudev` so modular hardware can coldplug correctly during boot.

### Flashing to eMMC from a live environment

The reliable workflow is:

1. Boot the board from a live USB.
2. From the host, pipe `output/images/up7000.img` over SSH directly into `dd` on `/dev/mmcblk0`.
3. Remove the USB stick.
4. Reboot and let the board boot from eMMC.

Example flash command:

```bash
REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
IMG="$REPO_ROOT/output/images/up7000.img"

sshpass -p user ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo dd of=/dev/mmcblk0 bs=16M iflag=fullblock oflag=direct conv=fsync status=progress' \
  < "$IMG"
```

Expected live-USB credentials in our current workflow are `user` / `user`.

### Sanity checks after a successful eMMC boot

```bash
uname -a
ip addr show eth0
lsblk -o NAME,SIZE,TYPE,MOUNTPOINT
df -h /
python3 --version
```


## Disk backup/restore

- Script: `device_code/disk_backup_restore.py`
- Docs: `docs/disk-backup-restore.md`

### Whole-board image backup/restore over SSH

Use this flow when the UP 7000 is booted from a live USB and the internal eMMC must be backed up or restored from the host.

Assumptions:
- Target is reachable over SSH at `user@192.168.55.1`
- Password is `user`
- The live USB is the boot device (`/dev/sda` in the examples below)
- The internal board storage is `/dev/mmcblk0`
- `sudo -n true` works on the target
- Host has `sshpass`, `gzip`, and enough free disk space

Always verify the target disks before writing anything:

```bash
sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'lsblk -b -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT'
```

Expected layout during live-USB imaging:
- `/dev/sda` is the live USB
- `/dev/mmcblk0` is the internal eMMC
- the eMMC boot areas are `/dev/mmcblk0boot0` and `/dev/mmcblk0boot1`

### Backup a full eMMC image

Create a backup directory on the host:

```bash
BACKUP_DIR=/media/ranshal/intel/up7000/backups/up7000-$(date +%Y%m%d)
mkdir -p "$BACKUP_DIR"
```

Save partition metadata:

```bash
sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo sfdisk -d /dev/mmcblk0' > "$BACKUP_DIR/mmcblk0.sfdisk"
```

Back up the two eMMC boot partitions:

```bash
sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo dd if=/dev/mmcblk0boot0 bs=1M status=none | gzip -1 -c' \
  > "$BACKUP_DIR/mmcblk0boot0.img.gz"

sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo dd if=/dev/mmcblk0boot1 bs=1M status=none | gzip -1 -c' \
  > "$BACKUP_DIR/mmcblk0boot1.img.gz"
```

Back up the main eMMC user area:

```bash
sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo dd if=/dev/mmcblk0 bs=16M iflag=fullblock status=none | gzip -1 -c' \
  > "$BACKUP_DIR/mmcblk0.img.gz"
```

Generate and verify checksums:

```bash
(cd "$BACKUP_DIR" && sha256sum *.gz > SHA256SUMS)
(cd "$BACKUP_DIR" && sha256sum -c SHA256SUMS)
gzip -t "$BACKUP_DIR"/*.gz
```

### Restore a full eMMC image

Warning: restore is destructive and overwrites the entire target eMMC and both boot partitions.

First, verify the replacement board exposes the expected eMMC target:

```bash
sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo blockdev --getsize64 /dev/mmcblk0 && lsblk -b -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT /dev/mmcblk0 /dev/mmcblk0boot0 /dev/mmcblk0boot1'
```

Restore the main eMMC image:

```bash
gzip -dc "$BACKUP_DIR/mmcblk0.img.gz" | \
  sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo dd of=/dev/mmcblk0 bs=16M oflag=direct conv=fsync status=none'
```

Temporarily unlock the boot partitions, restore them, then lock them again:

```bash
sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'echo 0 | sudo tee /sys/block/mmcblk0boot0/force_ro >/dev/null && \
   echo 0 | sudo tee /sys/block/mmcblk0boot1/force_ro >/dev/null'

gzip -dc "$BACKUP_DIR/mmcblk0boot0.img.gz" | \
  sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo dd of=/dev/mmcblk0boot0 bs=1M conv=fsync status=none'

gzip -dc "$BACKUP_DIR/mmcblk0boot1.img.gz" | \
  sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo dd of=/dev/mmcblk0boot1 bs=1M conv=fsync status=none'

sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'echo 1 | sudo tee /sys/block/mmcblk0boot0/force_ro >/dev/null && \
   echo 1 | sudo tee /sys/block/mmcblk0boot1/force_ro >/dev/null && \
   sudo sync'
```

Sanity-check the restored partitions:

```bash
sshpass -p user ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  user@192.168.55.1 \
  'sudo blkid /dev/mmcblk0p1 /dev/mmcblk0p2 /dev/mmcblk0p3; \
   cat /sys/block/mmcblk0boot0/force_ro; \
   cat /sys/block/mmcblk0boot1/force_ro'
```

After restore completes, reboot the board from internal storage.
## configurations
### MMC 
user: root
password: root
ip: 192.168.55.1

### Live USB
run with live usb with NAT on host:
user: user
pwd: user

sudo ip route add default via 192.168.55.100

echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf
ping -c 2 8.8.8.8

sudo apt update && sudo apt install -y openssh-server
enable ssh:
sudo systemctl enable --now ssh
change user passwd: sudo passwd user
