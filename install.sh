#!/bin/sh
# MT7612U USB WiFi Driver Installer for FreeBSD
#
# Usage: sudo sh install.sh [/path/to/freebsd/src]
#
# Copies driver sources, firmware, and Makefiles into the FreeBSD source
# tree, then builds and installs the kernel modules.

set -e

SRCDIR="${1:-/usr/src}"
REPODIR="$(cd "$(dirname "$0")" && pwd)"

die()  { echo "ERROR: $1" >&2; exit 1; }
info() { echo "==> $1"; }

# --- Sanity checks ---
[ "$(id -u)" -eq 0 ] || die "Must run as root (or with sudo)"
[ -d "$SRCDIR/sys/dev/usb/wlan" ] || die "FreeBSD source tree not found at $SRCDIR"

info "Installing MT7612U USB WiFi driver into $SRCDIR"

# --- Copy driver source files ---
info "Copying driver source files"
cp "$REPODIR/sys/dev/usb/wlan/if_mt76x2.c"    "$SRCDIR/sys/dev/usb/wlan/"
cp "$REPODIR/sys/dev/usb/wlan/if_mt76x2reg.h"  "$SRCDIR/sys/dev/usb/wlan/"
cp "$REPODIR/sys/dev/usb/wlan/if_mt76x2var.h"  "$SRCDIR/sys/dev/usb/wlan/"

# --- Copy module Makefiles ---
info "Copying module Makefiles"
mkdir -p "$SRCDIR/sys/modules/mt76x2"
cp "$REPODIR/sys/modules/mt76x2/Makefile" "$SRCDIR/sys/modules/mt76x2/"

mkdir -p "$SRCDIR/sys/modules/mt76x2fw"
cp "$REPODIR/sys/modules/mt76x2fw/Makefile" "$SRCDIR/sys/modules/mt76x2fw/"

# --- Copy firmware ---
info "Copying firmware"
mkdir -p "$SRCDIR/sys/contrib/dev/mt76x2"
for f in mt7662u.bin.uu mt7662u_rom_patch.bin.uu; do
    if [ -f "$REPODIR/sys/contrib/dev/mt76x2/$f" ]; then
        cp "$REPODIR/sys/contrib/dev/mt76x2/$f" "$SRCDIR/sys/contrib/dev/mt76x2/"
    fi
done

# --- Build ---
info "Building if_mt76x2 kernel module"
make -C "$SRCDIR/sys/modules/mt76x2" clean all

info "Building mt76x2fw firmware module"
make -C "$SRCDIR/sys/modules/mt76x2fw" clean all

# --- Install ---
info "Installing kernel modules"
make -C "$SRCDIR/sys/modules/mt76x2" install
make -C "$SRCDIR/sys/modules/mt76x2fw" install

# --- Load ---
info "Loading modules"

# Destroy wlan interfaces before unloading (module can't unload while in use)
for iface in $(ifconfig -l); do
    case "$iface" in
    wlan*)
        parent=$(sysctl -n net.wlan.${iface#wlan}.%parent 2>/dev/null || true)
        if [ "$parent" = "mt76x20" ]; then
            info "Destroying $iface (parent $parent)"
            ifconfig "$iface" destroy 2>/dev/null || true
        fi
        ;;
    esac
done

kldunload if_mt76x2 2>/dev/null || true
kldload mt76x2fw 2>/dev/null || true
kldload if_mt76x2 2>/dev/null || true

echo ""
echo "Done! To make persistent, add to /boot/loader.conf:"
echo '  mt76x2fw_load="YES"'
echo '  if_mt76x2_load="YES"'
echo ""
echo "Quick test:"
echo "  ifconfig wlan1 create wlandev mt76x20"
echo "  ifconfig wlan1 up"
echo "  ifconfig wlan1 list scan"
