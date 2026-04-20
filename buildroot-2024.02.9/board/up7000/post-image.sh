#!/bin/bash
set -e

BOARD_DIR="$(dirname "$(realpath "$0")")"
GENIMAGE_CFG="${BINARIES_DIR}/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"
ROOTFS_UUID="$(dumpe2fs "${BINARIES_DIR}/rootfs.ext4" 2>/dev/null | sed -n 's/^Filesystem UUID: *\(.*\)/\1/p')"

sed "s/UUID_TMP/${ROOTFS_UUID}/g" "${BOARD_DIR}/grub.cfg" > "${BINARIES_DIR}/efi-part/EFI/BOOT/grub.cfg"
sed "s/UUID_TMP/${ROOTFS_UUID}/g" "${BOARD_DIR}/genimage.cfg" > "${GENIMAGE_CFG}"

# Clean and run genimage
rm -rf "${GENIMAGE_TMP}"
genimage \
    --rootpath "${TARGET_DIR}" \
    --tmppath  "${GENIMAGE_TMP}" \
    --inputpath "${BINARIES_DIR}" \
    --outputpath "${BINARIES_DIR}" \
    --config "${GENIMAGE_CFG}"

echo ">>> Image written to ${BINARIES_DIR}/up7000.img"
