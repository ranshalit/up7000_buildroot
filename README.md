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

A fresh clone already contains the full `buildroot-2024.02.9/` source tree and the
pre-built Intel binary downloads (`dl/intel-compute-runtime/`, `dl/intel-ipp/`,
`dl/openvino-runtime/`, `dl/openvino-demo/`), so no separate tarball extraction or
download step is required before running `make`.

> **Important**: the defconfig uses **glibc** (required for OpenVINO pre-built binaries)
> and requires `BR2_EXTERNAL` pointing at `br2-external/`.
> A full clean rebuild is required after pulling these changes if you had a
> previous musl-based output tree.

```bash
REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
BUILDROOT="$REPO_ROOT/buildroot-2024.02.9"
OUTPUT="$REPO_ROOT/output"
BR2_EXT="$REPO_ROOT/br2-external"

# First-time configure (or after defconfig changes)
make -C "$BUILDROOT" O="$OUTPUT" BR2_EXTERNAL="$BR2_EXT" up7000_defconfig

# Full build (downloads standard sources, cross-compiles, generates up7000.img)
make -C "$BUILDROOT" O="$OUTPUT"

# Rebuild after board/config/package changes
make -C "$BUILDROOT" O="$OUTPUT"

# Rebuild kernel after changing board/up7000/linux.config
make -C "$BUILDROOT" O="$OUTPUT" linux-dirclean linux
make -C "$BUILDROOT" O="$OUTPUT"

# Rebuild only the demo (e.g. after editing detect.cpp)
make -C "$BUILDROOT" O="$OUTPUT" openvino-demo-dirclean openvino-demo
make -C "$BUILDROOT" O="$OUTPUT"
```

### Run under QEMU

Two QEMU boot paths are supported:

1. **GRUB / full-image boot**: boots `up7000.img` through UEFI + GRUB and exercises the
   same disk-image flow used on real hardware.
2. **Kernel / direct boot**: boots `bzImage` directly and mounts `rootfs.ext4` as the
   guest disk, which is useful for a faster kernel/userspace smoke test.

#### Option 1: GRUB / UEFI boot

Use **UEFI firmware** here: `up7000.img` contains a GRUB EFI system partition, not a
legacy BIOS boot sector. The command below runs headless on the serial console,
preserves the original disk image with `-snapshot`, and keeps the entire boot flow on
the same terminal: GRUB, kernel boot, and the final `up7000 login:` prompt.

```bash
REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
IMG="$REPO_ROOT/output/images/up7000.img"
OVMF_CODE=/usr/share/OVMF/OVMF_CODE.fd
OVMF_VARS_TEMPLATE=/usr/share/OVMF/OVMF_VARS.fd
OVMF_VARS="$(mktemp)"
cp "$OVMF_VARS_TEMPLATE" "$OVMF_VARS"
trap 'rm -f "$OVMF_VARS"' EXIT

if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
  ACCEL=(-accel kvm -cpu host)
else
  ACCEL=(-accel tcg)
fi

qemu-system-x86_64 \
  "${ACCEL[@]}" \
  -m 2048 \
  -smp 4 \
  -drive if=pflash,format=raw,readonly=on,file="$OVMF_CODE" \
  -drive if=pflash,format=raw,file="$OVMF_VARS" \
  -drive file="$IMG",format=raw,index=0,media=disk \
  -device e1000,netdev=n1 \
  -netdev user,id=n1 \
  -snapshot \
  -display none \
  -serial stdio \
  -monitor none
```

Expected result:

- The GRUB menu is visible on the serial terminal and can be controlled from the
  same QEMU session.
- After the GRUB timeout, the kernel boots on `ttyS0` and prints
  `Welcome to UP7000 Buildroot` followed by `up7000 login:`.
- Login credentials are `root` / `root`.

#### Option 2: Kernel / direct `bzImage` boot

This path skips UEFI and GRUB entirely. It is handy when you only want to test the
kernel + rootfs bring-up and do not need to validate the EFI boot chain.

```bash
REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
KERNEL="$REPO_ROOT/output/images/bzImage"
ROOTFS="$REPO_ROOT/output/images/rootfs.ext4"

if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
  ACCEL=(-accel kvm -cpu host)
else
  ACCEL=(-accel tcg)
fi

qemu-system-x86_64 \
  "${ACCEL[@]}" \
  -m 2048 \
  -smp 4 \
  -kernel "$KERNEL" \
  -append 'console=ttyS0,115200n8 console=tty1 root=/dev/sda rw rootwait net.ifnames=0 biosdevname=0' \
  -drive file="$ROOTFS",format=raw,index=0,media=disk \
  -device e1000,netdev=n1 \
  -netdev user,id=n1 \
  -snapshot \
  -display none \
  -serial stdio \
  -monitor none
```

Expected result:

- The kernel starts immediately without showing a GRUB menu.
- Linux boots on `ttyS0` and prints `Welcome to UP7000 Buildroot` followed by
  `up7000 login:`.
- Login credentials are `root` / `root`.

Notes for both options:

- On Debian/Ubuntu hosts, the OVMF files usually come from the `ovmf` package.
- `-snapshot` keeps the backing image file unchanged while you test boot.
- The GRUB path is the preferred one when you want to validate EFI + GRUB + Linux on
  the same serial console.
- The direct-kernel path is faster, but it does **not** validate the bootloader or ESP
  layout.
- This validates the generic x86_64 boot flow and userspace prompt only. QEMU does **not**
  emulate the UP7000 Intel iGPU, so GPU/OpenVINO validation still requires real hardware.

### Offline build (no internet required after clone)

A fully offline build requires two things: the git repository and a tarball of
standard Buildroot package downloads. Both travel separately because the standard
downloads (linux kernel, gcc, glibc, opencv4, etc.) are too large to commit to git.

**What is in git** — the 4 non-standard Intel binary downloads that cannot be fetched
from standard mirrors:

| Directory | Contents |
|---|---|
| `dl/intel-compute-runtime/` | OpenCL ICD, Level Zero, IGC, ocl-icd debs |
| `dl/intel-ipp/` | Intel IPP 2022.3 debs |
| `dl/openvino-runtime/` | OpenVINO 2024.4 toolkit tgz |
| `dl/openvino-demo/` | `person-detection-retail-0013.xml` + `.bin` |

**What travels as `standard_dl.tar.gz`** — the ~60 standard Buildroot package
downloads (~830 MB). This file is **not** in git; keep it alongside the repo on the
USB drive or shared storage.

#### Preparing the offline bundle (online machine, one-time)

```bash
cd /path/to/br_2024/

# Download all standard packages into a clean output dir
make -C buildroot-2024.02.9 \
     O=/tmp/br_source_only \
     BR2_EXTERNAL="$(pwd)/br2-external" \
     BR2_DL_DIR="$(pwd)/buildroot-2024.02.9/dl" \
     up7000_defconfig source

# Create the tarball (excludes the 4 non-standard dirs already in git)
cd buildroot-2024.02.9/dl
tar czf /path/to/standard_dl.tar.gz \
    $(ls -d */ | grep -vE '^(intel-compute-runtime|intel-ipp|openvino-runtime|openvino-demo)/$')
```

The resulting `standard_dl.tar.gz` is ~830 MB. Copy it alongside the git clone onto
the USB drive.

#### Building on the offline machine

```bash
# 1. Clone the repo from USB (or copy the directory)
git clone /path/to/usb/br_2024/ ~/br_2024/
cd ~/br_2024/

# 2. Extract standard downloads into the dl/ directory
cd buildroot-2024.02.9/dl
tar xzf /path/to/standard_dl.tar.gz
cd ../..

# 3. Configure and build (no network access needed)
mkdir output
make -C buildroot-2024.02.9 O="$(pwd)/output" BR2_EXTERNAL="$(pwd)/br2-external" up7000_defconfig
make -C buildroot-2024.02.9 O="$(pwd)/output"
```

> The `up7000_defconfig` step is required on each new machine — it regenerates
> `output/.config` with correct absolute paths for that machine.

> To verify no network access is used during build, run make under a network namespace:
> `sudo unshare --net -- make -C buildroot-2024.02.9 O="$(pwd)/output"`
> Any missing download will fail immediately (no internet) rather than silently downloading.

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
| `br2-external/` | BR2_EXTERNAL tree with OpenVINO + Intel GPU + demo packages |
| `br2-external/package/openvino-runtime/` | Pre-built OpenVINO 2024.4 runtime package |
| `br2-external/package/intel-compute-runtime/` | Pre-built Intel OpenCL GPU runtime (NEO) |
| `br2-external/package/openvino-demo/` | Person-detection demo C++ app (person-detection-retail-0013) + images |
| `br2-external/package/openvino-benchmark/` | Benchmark app that links against the packaged OpenVINO runtime |

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
7. **`GPT header not at the end of the disk` after writing the image to eMMC is expected.** The image is now ~2.1 GiB (64 MiB ESP + 2048 MiB rootfs) and the target eMMC is much larger.
8. **The on-board Ethernet is Realtek RTL8111H-CG.** Keep `r8169` plus its PHY/MDIO stack available, and include `eudev` so modular hardware can coldplug correctly during boot.
9. **Full rebuild needed after musl→glibc toolchain switch.** All compiled objects from a previous musl build are incompatible. Clean only the build artefacts — the downloaded sources in `buildroot-2024.02.9/dl/` are preserved so nothing needs to be re-downloaded:
   ```bash
   # Wipe compiled output only (keeps downloaded tarballs in buildroot-2024.02.9/dl/)
   rm -rf output/build output/host output/staging output/target output/images
   # Then re-configure and rebuild normally (see Build / rebuild section above)
   ```

---

## OpenVINO + Object-Detection Demo

### What is installed on the target

| Path | Contents |
|---|---|
| `/usr/lib/libopenvino*.so*` | OpenVINO runtime libs + inference plugins (installed directly into `/usr/lib`) |
| `/usr/lib/plugins.xml` | OpenVINO plugin registry (must live alongside the runtime libs) |
| `/usr/lib/libOpenCL.so.1*` | Khronos OpenCL ICD loader (dispatches to Intel GPU driver) |
| `/usr/lib/libigdrcl.so` | Intel Compute Runtime OpenCL ICD implementation |
| `/etc/OpenCL/vendors/intel.icd` | OpenCL ICD manifest for the Intel GPU |
| `/usr/bin/openvino-detect` | Detection demo binary |
| `/usr/bin/benchmark_app` | Benchmark binary for OpenVINO models |
| `/usr/share/openvino-demo/models/person-detection-retail-0013.xml` | Model graph (OpenVINO IR) |
| `/usr/share/openvino-demo/models/person-detection-retail-0013.bin` | Model weights |
| `/usr/share/openvino-demo/images/` | 3 sample images (person.jpg, sports.png, electronics.jpg) |

### Running the demo

```bash
# CPU inference on all bundled sample images (default)
openvino-detect

# CPU inference on a custom image
openvino-detect /path/to/photo.jpg

# GPU inference (requires Intel iGPU + compute-runtime installed)
openvino-detect --device GPU

# Annotated output images
ls /tmp/ov-demo-out/
```

### Running the benchmark app

`benchmark_app` is a small repo-local wrapper around the packaged OpenVINO runtime.
It does **not** install its own model assets; on the default UP7000 image you can
reuse the model already installed by `openvino-demo`.

Common model path used in the examples below:

```bash
# Show command-line help
benchmark_app --help
MODEL=/usr/share/openvino-demo/models/person-detection-retail-0013.xml
```

#### CPU benchmark

```bash
benchmark_app \
  -m "$MODEL" \
  -d CPU \
  -nwarmup 5 \
  -niter 100
```

#### GPU benchmark

```bash
benchmark_app \
  -m "$MODEL" \
  -d GPU \
  -nwarmup 5 \
  -niter 100
```

#### Optional tuning

```bash
# Run multiple asynchronous requests
benchmark_app \
  -m "$MODEL" \
  -d CPU \
  -nireq 4 \
  -async \
  -niter 200

# AUTO device selection
benchmark_app \
  -m "$MODEL" \
  -d AUTO \
  -nwarmup 5 \
  -niter 100
```

Notes:

- CPU benchmarking works in QEMU for a basic functional smoke test, but the
  numbers are only representative on real hardware.
- GPU benchmarking requires real UP7000 hardware with the Intel iGPU and compute
  runtime stack active.
- If you want to benchmark a different network, pass any supported `.xml` IR or
  `.onnx` model via `-m`.
- Output includes throughput (FPS) and latency statistics (avg/min/max/p50/p90/p99).

### Standalone SDK app (`app/`)

The repo also contains a small standalone OpenVINO example in `app/`. It is **not**
packaged into the Buildroot image; instead, it is intended to be built against the
generated SDK in `sdk/`.

#### Build on the host

From the repo root:

```bash
cmake -S app -B app/build \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/sdk/share/buildroot/toolchainfile.cmake"
cmake --build app/build
```

Host-side smoke test using the SDK loader and the model already present in the
target rootfs staging area:

```bash
MODEL="$(pwd)/output/target/usr/share/openvino-demo/models/person-detection-retail-0013.xml"
LOADER="$(pwd)/sdk/x86_64-buildroot-linux-gnu/sysroot/lib/ld-linux-x86-64.so.2"
LIBPATH="$(pwd)/sdk/x86_64-buildroot-linux-gnu/sysroot/usr/openvino/runtime/lib/intel64:$(pwd)/sdk/x86_64-buildroot-linux-gnu/sysroot/usr/lib:$(pwd)/sdk/x86_64-buildroot-linux-gnu/sysroot/lib"

# List devices seen by the OpenVINO runtime
"$LOADER" --library-path "$LIBPATH" ./app/build/vino-hello --list-devices

# Single-inference CPU smoke test
"$LOADER" --library-path "$LIBPATH" ./app/build/vino-hello \
  --model "$MODEL" \
  --device CPU
```

#### Deploy to the device

Copy the built binary to the target over SCP. The current image uses
`root@192.168.55.1` by default.

```bash
scp ./app/build/vino-hello root@192.168.55.1:/root/
```

#### Run on the device

SSH to the board and run `vino-hello` against the model already installed by
`openvino-demo`:

```bash
ssh root@192.168.55.1

# CPU
/root/vino-hello --model /usr/share/openvino-demo/models/person-detection-retail-0013.xml --device CPU

# GPU (real UP7000 hardware only)
/root/vino-hello --model /usr/share/openvino-demo/models/person-detection-retail-0013.xml --device GPU
```

Notes:

- `vino-hello` is intentionally minimal: it loads a model, creates zero-filled
  input tensors, runs one inference, and prints model IO metadata.
- CPU smoke testing works from the host with the SDK loader as shown above.
- GPU execution should be validated on real UP7000 hardware.

### Validation results (UP7000, Alder Lake-N, kernel 6.6.63)

Both CPU and GPU inference validated on device. Commands and expected output:

**CPU:**
```
# openvino-detect --device CPU
Model:  /usr/share/openvino-demo/models/person-detection-retail-0013.xml
Device: CPU
Model compiled on CPU. Running inference...

/usr/share/openvino-demo/images/electronics.jpg → /tmp/ov-demo-out/electronics.jpg  (0 persons detected)
/usr/share/openvino-demo/images/person.jpg → /tmp/ov-demo-out/person.jpg  (0 persons detected)
/usr/share/openvino-demo/images/sports.png → /tmp/ov-demo-out/sports.png  (2 persons detected)
  person conf=0.982457 box=(39,85 122x389)
  person conf=0.846887 box=(497,26 136x374)

Done. Annotated images saved to: /tmp/ov-demo-out
```

**GPU:**
```
# openvino-detect --device GPU
Model:  /usr/share/openvino-demo/models/person-detection-retail-0013.xml
Device: GPU
Model compiled on GPU. Running inference...

/usr/share/openvino-demo/images/electronics.jpg → /tmp/ov-demo-out/electronics.jpg  (0 persons detected)
/usr/share/openvino-demo/images/person.jpg → /tmp/ov-demo-out/person.jpg  (0 persons detected)
/usr/share/openvino-demo/images/sports.png → /tmp/ov-demo-out/sports.png  (2 persons detected)
  person conf=0.98291 box=(39,85 122x389)
  person conf=0.846191 box=(497,26 136x374)

Done. Annotated images saved to: /tmp/ov-demo-out
```

Verify GPU firmware loaded correctly before running GPU inference:
```bash
dmesg | grep -E "guc|huc"
# Expected:
# i915 0000:00:02.0: [drm] GT0: GuC firmware i915/tgl_guc_70.bin version 70.13.1
# i915 0000:00:02.0: [drm] GT0: HuC firmware i915/tgl_huc.bin version 7.9.3
```

### Copying annotated images back to the host

```bash
scp root@192.168.55.1:/tmp/ov-demo-out/*.jpg ./
```

### GPU inference requirements

For `--device GPU` to work, the Intel iGPU must be visible as `/dev/dri/renderD128`
and the Compute Runtime (installed by `intel-compute-runtime` package) must have
loaded its OpenCL ICD. Check with:

```bash
ls /dev/dri/
cat /etc/OpenCL/vendors/intel.icd
```

### Adding your own images or model

- Drop images (JPEG/PNG) into `/usr/share/openvino-demo/images/` on the target,
  or pass them as arguments to `openvino-detect`.
- The bundled model (`person-detection-retail-0013`) detects people only.
  For multi-class detection, rebuild with a different model and update `detect.cpp`
  for its input/output format.

---

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

sshpass -p root ssh \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'dd of=/dev/mmcblk0 bs=16M iflag=fullblock oflag=direct conv=fsync' \
  < "$IMG"
```

Expected live-USB credentials are `root` / `root`.

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
- Target is reachable over SSH at `root@192.168.55.1`
- Password is `root`
- The live USB is the boot device (`/dev/sda` in the examples below)
- The internal board storage is `/dev/mmcblk0`
- `sudo -n true` works on the target
- Host has `sshpass`, `gzip`, and enough free disk space

Always verify the target disks before writing anything:

```bash
sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
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
sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'sudo sfdisk -d /dev/mmcblk0' > "$BACKUP_DIR/mmcblk0.sfdisk"
```

Back up the two eMMC boot partitions:

```bash
sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'sudo dd if=/dev/mmcblk0boot0 bs=1M status=none | gzip -1 -c' \
  > "$BACKUP_DIR/mmcblk0boot0.img.gz"

sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'sudo dd if=/dev/mmcblk0boot1 bs=1M status=none | gzip -1 -c' \
  > "$BACKUP_DIR/mmcblk0boot1.img.gz"
```

Back up the main eMMC user area:

```bash
sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
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
sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'sudo blockdev --getsize64 /dev/mmcblk0 && lsblk -b -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT /dev/mmcblk0 /dev/mmcblk0boot0 /dev/mmcblk0boot1'
```

Restore the main eMMC image:

```bash
gzip -dc "$BACKUP_DIR/mmcblk0.img.gz" | \
  sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'sudo dd of=/dev/mmcblk0 bs=16M oflag=direct conv=fsync status=none'
```

Temporarily unlock the boot partitions, restore them, then lock them again:

```bash
sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'echo 0 | sudo tee /sys/block/mmcblk0boot0/force_ro >/dev/null && \
   echo 0 | sudo tee /sys/block/mmcblk0boot1/force_ro >/dev/null'

gzip -dc "$BACKUP_DIR/mmcblk0boot0.img.gz" | \
  sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'sudo dd of=/dev/mmcblk0boot0 bs=1M conv=fsync status=none'

gzip -dc "$BACKUP_DIR/mmcblk0boot1.img.gz" | \
  sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'sudo dd of=/dev/mmcblk0boot1 bs=1M conv=fsync status=none'

sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
  'echo 1 | sudo tee /sys/block/mmcblk0boot0/force_ro >/dev/null && \
   echo 1 | sudo tee /sys/block/mmcblk0boot1/force_ro >/dev/null && \
   sudo sync'
```

Sanity-check the restored partitions:

```bash
sshpass -p root ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
  root@192.168.55.1 \
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
user: root
pwd: root

sudo ip route add default via 192.168.55.100

echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf
ping -c 2 8.8.8.8

sudo apt update && sudo apt install -y openssh-server
enable ssh:
sudo systemctl enable --now ssh
change user passwd: sudo passwd user
