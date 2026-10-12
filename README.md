# MT7612U USB WiFi Driver for FreeBSD

FreeBSD kernel driver for MediaTek MT7612U/MT7632U USB 802.11a/b/g/n/ac wireless adapters.
Supports 16+ device variants from 10 manufacturers. The MT7612U chipset remains in
active production — adapters are readily available from major retailers.

## Features

- Dual-band 2.4 GHz and 5 GHz operation
- 802.11a/b/g/n/ac — HT (MCS0-15) and VHT (MCS0-9, NSS 1-2)
- VHT80 channel support with proper TXWI encoding (PHY=4, NSS+MCS)
- 2x2 MIMO (2T2R)
- STA, Monitor, HOSTAP, and IBSS modes
- WPA/WPA2 hardware encryption (AES-CCM, TKIP via WCID key table)
- AMPDU TX aggregation with BA session management
- TX aggregation via NEXT_VLD (multiple frames per USB transfer)
- RX bulk aggregation (up to 8 frames per USB transfer)
- AMRR rate control with hardware TX statistics (FIFO + USB completion)
- VHT-aware rate control via `ieee80211_node_get_txrate()`
- VHT-aware SGI, STBC, LDPC, and SMPS negotiation
- HT/ERP/VHT protection mode programming (12 protection registers)
- Periodic radio calibration (60s): TSSI, DPD, temperature, AGC, EDCCA
- BW80 TX power delta from EEPROM
- MAC error watchdog with automatic CSR reset
- Monitor mode with full radiotap headers (including VHT field)
- Power save (IEEE80211_C_PMGT)
- AP beacon template upload with `iv_update_beacon` callback
- Firmware loading via firmware(9) — two-stage: ROM patch + main firmware
- Per-channel RX gain calibration and calibrated RSSI reporting

## Supported Devices

| Device | USB ID | Status |
|--------|--------|--------|
| NetGear A6210 | 0846:9053 | **Tested** |
| Alfa AWUS036ACM | 0e8d:7612 | Active |
| Microsoft Xbox One WLAN v2 (1790) | 045e:02fe | Active |
| Microsoft Xbox One WLAN (1713) | 045e:02e6 | Available |
| ASUS USB-AC55 | 0b05:17eb | Discontinued |
| ASUS USB-AC54 | 0b05:1833 | Discontinued |
| ASUS USB-N53 B1 | 0b05:180b | Discontinued |
| Edimax EW-7722UAC | 7392:b711 | Discontinued |
| TP-Link TL-WDN6200 | 2357:0137 | Discontinued |
| AVM FRITZ!WLAN AC 860 | 057c:8503 | Available |
| Mercury UD13 | 2c4e:0103 | Discontinued |
| NetGear WNDA3100v3 | 0846:9014 | Discontinued |
| MediaTek MT7612U (generic) | 0e8d:7612 | Active |
| MediaTek HC-M7662BU1 | 0e8d:7632 | OEM |

> **Note:** The driver matches on USB vendor/product IDs. Other MT7612U/MT7632U
> devices not listed above should also work — the chipset is identical.

## Quick Install

```sh
git clone https://github.com/maxsteciuk/freebsd-mt76x2.git
cd freebsd-mt76x2
sudo sh install.sh
```

## Manual Install

```sh
# Copy source files
sudo cp sys/dev/usb/wlan/if_mt76x2.c    /usr/src/sys/dev/usb/wlan/
sudo cp sys/dev/usb/wlan/if_mt76x2reg.h /usr/src/sys/dev/usb/wlan/
sudo cp sys/dev/usb/wlan/if_mt76x2var.h /usr/src/sys/dev/usb/wlan/
sudo mkdir -p /usr/src/sys/modules/mt76x2 /usr/src/sys/modules/mt76x2fw
sudo cp sys/modules/mt76x2/Makefile /usr/src/sys/modules/mt76x2/
sudo cp sys/modules/mt76x2fw/Makefile /usr/src/sys/modules/mt76x2fw/
sudo mkdir -p /usr/src/sys/contrib/dev/mt76x2
sudo cp sys/contrib/dev/mt76x2/*.uu /usr/src/sys/contrib/dev/mt76x2/

# Build and install
sudo make -C /usr/src/sys/modules/mt76x2 clean all install
sudo make -C /usr/src/sys/modules/mt76x2fw clean all install

# Load (firmware first)
sudo kldload mt76x2fw
sudo kldload if_mt76x2
```

Add to `/boot/loader.conf` for persistence:
```
mt76x2fw_load="YES"
if_mt76x2_load="YES"
```

## Usage

```sh
ifconfig wlan0 create wlandev mt76x20
ifconfig wlan0 up scan
wpa_supplicant -i wlan0 -c /etc/wpa_supplicant.conf -B
dhclient wlan0
```

For `/etc/rc.conf`:
```sh
wlans_mt76x20="wlan0"
ifconfig_wlan0="WPA DHCP"
```

## Debugging

```sh
sysctl hw.usb.mt76x2.debug=0xff
```

Debug flags: TX=0x01, RX=0x02, State=0x04, Rate=0x08, USB=0x10, Reset=0x20, MCU=0x40, Firmware=0x80.

## Architecture

Single-file driver (~7200 lines) following FreeBSD USB WiFi conventions, modeled
after `if_mtw(4)` (MT7601U) with hardware register maps from the Linux `mt76x2u`
driver (BSD-3-Clause).

Two kernel modules:
- **`if_mt76x2`** — Driver: attach/detach, USB transfers, net80211, firmware loading, PHY/RF init, TX/RX paths, MCU commands
- **`mt76x2fw`** — Firmware blobs packaged as firmware(9) module

USB transfer layout: 9 endpoints — 4 TX (per QoS AC: BE/BK/VI/VO), 1 HCCA, 1 firmware/MCU command, 1 RX data, 1 MCU response, 1 priority TX.

```
sys/
├── contrib/dev/mt76x2/
│   ├── mt7662u.bin.uu               <- Main firmware (uuencoded)
│   └── mt7662u_rom_patch.bin.uu     <- ROM patch firmware (uuencoded)
├── dev/usb/wlan/
│   ├── if_mt76x2.c                  <- Main driver source (~7200 lines)
│   ├── if_mt76x2reg.h               <- Hardware register definitions
│   └── if_mt76x2var.h               <- Driver data structures
└── modules/
    ├── mt76x2/Makefile               <- Driver kernel module build
    └── mt76x2fw/Makefile             <- Firmware kernel module build
```

## Known Limitations

- TX throughput capped at ~0.5 Mbps upload on USB 2.0 EHCI due to FreeBSD USB per-endpoint serialization (not a driver issue)
- DFS channels not supported (Linux USB driver also omits DFS by design)
- First-assoc stall after kldunload/reload: AP CCMP TSC replay rejection; requires USB replug or reboot

## Tested On

- FreeBSD 15.0-RELEASE
- NetGear A6210 (0846:9053)
- Cisco Aironet 3702I AP (2.4 GHz HT20, 5 GHz HT40+)

## License

- MediaTek firmware (`sys/contrib/dev/mt76x2`): MediaTek's redistributable
  firmware license, see `sys/contrib/dev/mt76x2/LICENSE`
- BSD-2-Clause (driver structure from FreeBSD `if_mtw(4)`)
- BSD-3-Clause (hardware register definitions from Linux `mt76`)
