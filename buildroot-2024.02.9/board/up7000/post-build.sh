#!/bin/bash
set -e

# Remove auto-generated SSH host keys so they are freshly created on first boot
rm -f "${TARGET_DIR}/etc/ssh/ssh_host_"*

# Add an HDMI login prompt in addition to the serial console.
if [ -e "${TARGET_DIR}/etc/inittab" ]; then
    grep -qE '^tty1::' "${TARGET_DIR}/etc/inittab" || \
        sed -i '/GENERIC_SERIAL/a\
tty1::respawn:/sbin/getty -L  tty1 0 vt100 # HDMI console' "${TARGET_DIR}/etc/inittab"
fi
