/*-
 * Copyright (c) 2008-2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2013-2014 Kevin Lo
 * Copyright (c) 2018 Lorenzo Bianconi <lorenzo.bianconi83@gmail.com>
 * Copyright (c) 2021 James Hastings
 * Copyright (c) 2026 Maksym Stetsyuk
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * MediaTek MT7612U 802.11a/b/g/n/ac USB wireless adapter driver.
 *
 * Hardware register details derived from the Linux mt76x2u driver
 * (BSD-3-Clause license).
 *
 * Supported devices:
 *   - ASUS USB-AC54          (0x0b05:0x1833)
 *   - ASUS USB-AC55          (0x0b05:0x17eb)
 *   - ASUS USB-N53 B1        (0x0b05:0x180b)
 *   - AVM FRITZ!WLAN AC860   (0x057c:0x8503)
 *   - Edimax EW-7722UAC      (0x7392:0xb711)
 *   - LiteOn WN4516R         (0x0471:0x2126)
 *   - LiteOn WN4519R         (0x0471:0x7600)
 *   - MediaTek MT7612U       (0x0e8d:0x7612)  [Aukey/Alfa AWUS036ACM]
 *   - MediaTek HC-M7662BU1   (0x0e8d:0x7632)
 *   - Mercury UD13           (0x2c4e:0x0103)
 *   - Microsoft Xbox One WLAN (0x045e:0x02e6, 0x045e:0x02fe)
 *   - NetGear A6210          (0x0846:0x9053)
 *   - NetGear WNDA3100v3     (0x0846:0x9014)
 *   - TP-Link TL-WDN6200     (0x2357:0x0137)
 */

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#include <sys/firmware.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_regdomain.h>
#ifdef IEEE80211_SUPPORT_SUPERG
#include <net80211/ieee80211_superg.h>
#endif
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include "usbdevs.h"

/*
 * USB device IDs for MT7612U-based adapters.
 * Full list from Linux mt76x2u driver.
 * Defined here so the driver builds out-of-tree without patching usbdevs.
 */
#ifndef USB_PRODUCT_NETGEAR_A6210
#define USB_PRODUCT_NETGEAR_A6210	0x9053
#endif
#ifndef USB_PRODUCT_NETGEAR_WNDA3100V3
#define USB_PRODUCT_NETGEAR_WNDA3100V3	0x9014
#endif
#ifndef USB_PRODUCT_MEDIATEK_MT7612U
#define USB_PRODUCT_MEDIATEK_MT7612U	0x7612
#endif
#ifndef USB_PRODUCT_MEDIATEK_MT7632U
#define USB_PRODUCT_MEDIATEK_MT7632U	0x7632
#endif
#ifndef USB_PRODUCT_ASUS_USBAC54
#define USB_PRODUCT_ASUS_USBAC54	0x1833
#endif
#ifndef USB_PRODUCT_ASUS_USBAC55
#define USB_PRODUCT_ASUS_USBAC55	0x17eb
#endif
#ifndef USB_PRODUCT_ASUS_USBN53B1
#define USB_PRODUCT_ASUS_USBN53B1	0x180b
#endif
#ifndef USB_VENDOR_AVM
#define USB_VENDOR_AVM			0x057c
#endif
#ifndef USB_PRODUCT_AVM_FRITZWLANAC860
#define USB_PRODUCT_AVM_FRITZWLANAC860	0x8503
#endif
#ifndef USB_VENDOR_EDIMAX
#define USB_VENDOR_EDIMAX		0x7392
#endif
#ifndef USB_PRODUCT_EDIMAX_EW7722UAC
#define USB_PRODUCT_EDIMAX_EW7722UAC	0xb711
#endif
#ifndef USB_VENDOR_LITEON2
#define USB_VENDOR_LITEON2		0x0471
#endif
#ifndef USB_PRODUCT_LITEON2_WN4516R
#define USB_PRODUCT_LITEON2_WN4516R	0x2126
#endif
#ifndef USB_PRODUCT_LITEON2_WN4519R
#define USB_PRODUCT_LITEON2_WN4519R	0x7600
#endif
#ifndef USB_VENDOR_MERCURY
#define USB_VENDOR_MERCURY		0x2c4e
#endif
#ifndef USB_PRODUCT_MERCURY_UD13
#define USB_PRODUCT_MERCURY_UD13	0x0103
#endif
#ifndef USB_VENDOR_MICROSOFT
#define USB_VENDOR_MICROSOFT		0x045e
#endif
#ifndef USB_PRODUCT_MICROSOFT_XBOX_WLAN1
#define USB_PRODUCT_MICROSOFT_XBOX_WLAN1 0x02e6
#endif
#ifndef USB_PRODUCT_MICROSOFT_XBOX_WLAN2
#define USB_PRODUCT_MICROSOFT_XBOX_WLAN2 0x02fe
#endif
#ifndef USB_VENDOR_TPLINK
#define USB_VENDOR_TPLINK		0x2357
#endif
#ifndef USB_PRODUCT_TPLINK_TLWDN6200
#define USB_PRODUCT_TPLINK_TLWDN6200	0x0137
#endif

#define USB_DEBUG_VAR mt76x2_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_msctest.h>

#include "if_mt76x2reg.h"
#include "if_mt76x2var.h"

/* BW override: 0=auto (match channel BW), 1=force BW20 */
static uint8_t mt76x2_bw_mode;

/* Max MCS index: 7=1-stream (default), 15=2-stream */
static uint8_t mt76x2_max_mcs = 7;

#define MT76X2_DEBUG

#ifdef MT76X2_DEBUG
int mt76x2_debug;

enum {
	MT76X2_DEBUG_XMIT	= 0x00000001,
	MT76X2_DEBUG_RECV	= 0x00000002,
	MT76X2_DEBUG_STATE	= 0x00000004,
	MT76X2_DEBUG_RATE	= 0x00000008,
	MT76X2_DEBUG_USB	= 0x00000010,
	MT76X2_DEBUG_RESET	= 0x00000020,
	MT76X2_DEBUG_CMD	= 0x00000040,
	MT76X2_DEBUG_FIRMWARE	= 0x00000080,
	MT76X2_DEBUG_ANY	= 0xffffffff,
};

#define MT76X2_DPRINTF(sc, m, ...)					\
	do {								\
		if (mt76x2_debug & (m))					\
			device_printf((sc)->sc_dev, __VA_ARGS__);	\
	} while (0)

SYSCTL_NODE(_hw_usb, OID_AUTO, mt76x2, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "USB MT76x2");
SYSCTL_INT(_hw_usb_mt76x2, OID_AUTO, debug, CTLFLAG_RWTUN, &mt76x2_debug, 0,
    "MT76x2 debug level");

static int mt76x2_last_tx_ridx;
SYSCTL_INT(_hw_usb_mt76x2, OID_AUTO, last_tx_ridx, CTLFLAG_RD,
    &mt76x2_last_tx_ridx, 0, "Last TX rate index used");

/* TX pipeline diagnostic counters (reset by writing 0) */
static uint32_t mt76x2_tx_queue_full;
SYSCTL_U32(_hw_usb_mt76x2, OID_AUTO, tx_queue_full, CTLFLAG_RW,
    &mt76x2_tx_queue_full, 0, "TX mbufq overflow count");
static uint32_t mt76x2_tx_epq_full;
SYSCTL_U32(_hw_usb_mt76x2, OID_AUTO, tx_epq_full, CTLFLAG_RW,
    &mt76x2_tx_epq_full, 0, "TX endpoint ring full count");
static uint32_t mt76x2_tx_submit;
SYSCTL_U32(_hw_usb_mt76x2, OID_AUTO, tx_submit, CTLFLAG_RW,
    &mt76x2_tx_submit, 0, "USB TX transfers submitted");
static uint32_t mt76x2_tx_complete;
SYSCTL_U32(_hw_usb_mt76x2, OID_AUTO, tx_complete, CTLFLAG_RW,
    &mt76x2_tx_complete, 0, "USB TX transfers completed");
static uint32_t mt76x2_tx_hw_fail;
SYSCTL_U32(_hw_usb_mt76x2, OID_AUTO, tx_hw_fail, CTLFLAG_RW,
    &mt76x2_tx_hw_fail, 0, "HW TX fail (MAC retry exhausted)");
static uint32_t mt76x2_tx_hw_ok;
SYSCTL_U32(_hw_usb_mt76x2, OID_AUTO, tx_hw_ok, CTLFLAG_RW,
    &mt76x2_tx_hw_ok, 0, "HW TX success (ACKed)");
static uint32_t mt76x2_tx_hw_retry;
SYSCTL_U32(_hw_usb_mt76x2, OID_AUTO, tx_hw_retry, CTLFLAG_RW,
    &mt76x2_tx_hw_retry, 0, "HW TX retry count");

SYSCTL_U8(_hw_usb_mt76x2, OID_AUTO, bw_mode, CTLFLAG_RW,
    &mt76x2_bw_mode, 0, "BW override: 0=auto (match channel), 1=force BW20");
SYSCTL_U8(_hw_usb_mt76x2, OID_AUTO, max_mcs, CTLFLAG_RW,
    &mt76x2_max_mcs, 7, "Max MCS index: 7=1-stream, 15=2-stream");

static int32_t mt76x2_temp_cal_count;
SYSCTL_S32(_hw_usb_mt76x2, OID_AUTO, temp_cal_count, CTLFLAG_RD,
    &mt76x2_temp_cal_count, 0, "Temperature calibration cycle count");

static uint32_t mt76x2_mac_err_count;
SYSCTL_U32(_hw_usb_mt76x2, OID_AUTO, mac_err_count, CTLFLAG_RD,
    &mt76x2_mac_err_count, 0, "MAC error watchdog recovery count");
#else
#define MT76X2_DPRINTF(sc, m, ...)	do { (void)(sc); } while (0)
#endif

/* DMA header (TXD/TXINFO) size — 4 bytes prepended to TXWI on USB */
#define MT76X2_DMA_HDR_LEN	4

/* Matches if_run(4) / if_mtw(4) local definition */
#define	IEEE80211_HAS_ADDR4(wh)	IEEE80211_IS_DSTODS(wh)

/* Single frame: TXD + TXWI + max 802.11 frame + pad + zero trailer */
#define MT76X2_FRAME_TXSZ						\
	(MT76X2_DMA_HDR_LEN + sizeof(struct mt76x2_txwi) +		\
	 IEEE80211_MAX_LEN + sizeof(struct mt76x2_rxd) + 8)

/* Aggregated USB buffer: multiple frames packed per transfer */
#define MT76X2_MAX_TXSZ		(MT76X2_TX_AGG_COUNT * MT76X2_FRAME_TXSZ)

static const STRUCT_USB_HOST_ID mt76x2_devs[] = {
	{ USB_VP(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_USBAC54) },
	{ USB_VP(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_USBAC55) },
	{ USB_VP(USB_VENDOR_ASUS, USB_PRODUCT_ASUS_USBN53B1) },
	{ USB_VP(USB_VENDOR_AVM, USB_PRODUCT_AVM_FRITZWLANAC860) },
	{ USB_VP(USB_VENDOR_EDIMAX, USB_PRODUCT_EDIMAX_EW7722UAC) },
	{ USB_VP(USB_VENDOR_LITEON2, USB_PRODUCT_LITEON2_WN4516R) },
	{ USB_VP(USB_VENDOR_LITEON2, USB_PRODUCT_LITEON2_WN4519R) },
	{ USB_VP(USB_VENDOR_MEDIATEK, USB_PRODUCT_MEDIATEK_MT7612U) },
	{ USB_VP(USB_VENDOR_MEDIATEK, USB_PRODUCT_MEDIATEK_MT7632U) },
	{ USB_VP(USB_VENDOR_MERCURY, USB_PRODUCT_MERCURY_UD13) },
	{ USB_VP(USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_XBOX_WLAN1) },
	{ USB_VP(USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_XBOX_WLAN2) },
	{ USB_VP(USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_A6210) },
	{ USB_VP(USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_WNDA3100V3) },
	{ USB_VP(USB_VENDOR_TPLINK, USB_PRODUCT_TPLINK_TLWDN6200) },
};

/*
 * Rate table — maps rate index to MCS code, PHY mode, and
 * IEEE802.11 rate value.  Matches if_run.c rt2860_rates[].
 */
static const struct {
	uint8_t		rate;	/* IEEE80211 rate value (0x80+ for HT MCS) */
	uint8_t		mcs;	/* hardware MCS index for TXWI */
	enum ieee80211_phytype phy;
} mt76x2_rates[] = {
	/* CCK rates (11b) */
	{   2, 0, IEEE80211_T_DS },	/* 0: 1 Mbps */
	{   4, 1, IEEE80211_T_DS },	/* 1: 2 Mbps */
	{  11, 2, IEEE80211_T_DS },	/* 2: 5.5 Mbps */
	{  22, 3, IEEE80211_T_DS },	/* 3: 11 Mbps */

	/* OFDM rates (11a/g) */
	{  12, 0, IEEE80211_T_OFDM },	/* 4: 6 Mbps */
	{  18, 1, IEEE80211_T_OFDM },	/* 5: 9 Mbps */
	{  24, 2, IEEE80211_T_OFDM },	/* 6: 12 Mbps */
	{  36, 3, IEEE80211_T_OFDM },	/* 7: 18 Mbps */
	{  48, 4, IEEE80211_T_OFDM },	/* 8: 24 Mbps */
	{  72, 5, IEEE80211_T_OFDM },	/* 9: 36 Mbps */
	{  96, 6, IEEE80211_T_OFDM },	/* 10: 48 Mbps */
	{ 108, 7, IEEE80211_T_OFDM },	/* 11: 54 Mbps */

	/* HT MCS 0-7 (1 stream) */
	{ 0x80,  0, IEEE80211_T_HT },	/* 12: MCS0 6.5/7.2 Mbps */
	{ 0x81,  1, IEEE80211_T_HT },	/* 13: MCS1 */
	{ 0x82,  2, IEEE80211_T_HT },	/* 14: MCS2 */
	{ 0x83,  3, IEEE80211_T_HT },	/* 15: MCS3 */
	{ 0x84,  4, IEEE80211_T_HT },	/* 16: MCS4 */
	{ 0x85,  5, IEEE80211_T_HT },	/* 17: MCS5 */
	{ 0x86,  6, IEEE80211_T_HT },	/* 18: MCS6 */
	{ 0x87,  7, IEEE80211_T_HT },	/* 19: MCS7 65/72.2 Mbps */

	/* HT MCS 8-15 (2 streams) */
	{ 0x88,  8, IEEE80211_T_HT },	/* 20: MCS8 13/14.4 Mbps */
	{ 0x89,  9, IEEE80211_T_HT },	/* 21: MCS9 */
	{ 0x8a, 10, IEEE80211_T_HT },	/* 22: MCS10 */
	{ 0x8b, 11, IEEE80211_T_HT },	/* 23: MCS11 */
	{ 0x8c, 12, IEEE80211_T_HT },	/* 24: MCS12 */
	{ 0x8d, 13, IEEE80211_T_HT },	/* 25: MCS13 */
	{ 0x8e, 14, IEEE80211_T_HT },	/* 26: MCS14 */
	{ 0x8f, 15, IEEE80211_T_HT },	/* 27: MCS15 130/144.4 Mbps */
};

#define MT76X2_RIDX_CCK1	0
#define MT76X2_RIDX_CCK11	3
#define MT76X2_RIDX_OFDM6	4
#define MT76X2_RIDX_OFDM54	11
#define MT76X2_RIDX_MCS0	12
#define MT76X2_RIDX_MCS15	27
#define MT76X2_RIDX_MAX		nitems(mt76x2_rates)
#define MT76X2_RIDX_VHT		0xff	/* sentinel: VHT rate in vht_nss/vht_mcs */

/* Forward declarations */
static device_probe_t mt76x2_match;
static device_attach_t mt76x2_attach;
static device_detach_t mt76x2_detach;

static usb_callback_t mt76x2_bulk_rx_callback;
static usb_callback_t mt76x2_mcu_rx_callback;
static usb_callback_t mt76x2_bulk_tx_callback0;
static usb_callback_t mt76x2_bulk_tx_callback1;
static usb_callback_t mt76x2_bulk_tx_callback2;
static usb_callback_t mt76x2_bulk_tx_callback3;
static usb_callback_t mt76x2_bulk_tx_callback4;
static usb_callback_t mt76x2_bulk_tx_callback5;
static usb_callback_t mt76x2_fw_callback;

static void mt76x2_autoinst(void *, struct usb_device *,
    struct usb_attach_arg *);
static int mt76x2_driver_loaded(struct module *, int, void *);
static void mt76x2_bulk_tx_callbackN(struct usb_xfer *, usb_error_t, u_int);
static struct ieee80211vap *mt76x2_vap_create(struct ieee80211com *,
    const char[IFNAMSIZ], int, enum ieee80211_opmode, int,
    const uint8_t[IEEE80211_ADDR_LEN],
    const uint8_t[IEEE80211_ADDR_LEN]);
static void mt76x2_vap_delete(struct ieee80211vap *);
static void mt76x2_cmdq_cb(void *, int);
static void mt76x2_setup_tx_list(struct mt76x2_softc *,
    struct mt76x2_endpoint_queue *);
static void mt76x2_unsetup_tx_list(struct mt76x2_softc *,
    struct mt76x2_endpoint_queue *);
static void mt76x2_tx_free(struct mt76x2_endpoint_queue *,
    struct mt76x2_tx_data *, int);
static void mt76x2_start(struct mt76x2_softc *);
static int mt76x2_load_microcode(struct mt76x2_softc *);
static int mt76x2_load_rom_patch(struct mt76x2_softc *);
static int mt76x2_load_firmware(struct mt76x2_softc *);
static int mt76x2_read(struct mt76x2_softc *, uint16_t, uint32_t *);
static int mt76x2_write(struct mt76x2_softc *, uint16_t, uint32_t);
static int mt76x2_write_region_1(struct mt76x2_softc *, uint16_t,
    const uint8_t *, int);
static int mt76x2_read_eeprom(struct mt76x2_softc *);
static void mt76x2_set_macaddr(struct mt76x2_softc *, const uint8_t *);
static void mt76x2_set_bssid(struct mt76x2_softc *, const uint8_t *);
static int mt76x2_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static int mt76x2_set_chan(struct mt76x2_softc *,
    struct ieee80211_channel *);
static void mt76x2_set_channel(struct ieee80211com *);
static void mt76x2_getradiocaps(struct ieee80211com *, int,
    int *, struct ieee80211_channel[]);
static void mt76x2_scan_start(struct ieee80211com *);
static void mt76x2_scan_end(struct ieee80211com *);
static void mt76x2_update_mcast(struct ieee80211com *);
static void mt76x2_update_promisc(struct ieee80211com *);
static void mt76x2_update_promisc_locked(struct mt76x2_softc *);
static int mt76x2_wme_update(struct ieee80211com *);
static int mt76x2_raw_xmit(struct ieee80211_node *, struct mbuf *,
    const struct ieee80211_bpf_params *);
static int mt76x2_transmit(struct ieee80211com *, struct mbuf *);
static void mt76x2_parent(struct ieee80211com *);
static struct ieee80211_node *mt76x2_node_alloc(struct ieee80211vap *,
    const uint8_t[IEEE80211_ADDR_LEN]);
static void mt76x2_newassoc(struct ieee80211_node *, int);
static void mt76x2_txstat_drain(void *, int);
static void mt76x2_ratectl_to(void *);
static void mt76x2_updateslot(struct ieee80211com *);
static void mt76x2_updateslot_cb(void *);
static int mt76x2_txrx_enable(struct mt76x2_softc *);
static int mt76x2_hw_init(struct mt76x2_softc *);
static void mt76x2_init_locked(struct mt76x2_softc *);
static void mt76x2_stop(void *);
static void mt76x2_delay(struct mt76x2_softc *, u_int);
static void mt76x2_update_chw(struct ieee80211com *);
static int mt76x2_ampdu_enable(struct ieee80211_node *,
    struct ieee80211_tx_ampdu *);
static int mt76x2_addba_request(struct ieee80211_node *,
    struct ieee80211_tx_ampdu *, int, int, int);
static int mt76x2_addba_response(struct ieee80211_node *,
    struct ieee80211_tx_ampdu *, int, int, int);
static void mt76x2_addba_stop(struct ieee80211_node *,
    struct ieee80211_tx_ampdu *);
static void mt76x2_newassoc_cb(void *);
static int mt76x2_key_set(struct ieee80211vap *,
    const struct ieee80211_key *);
static int mt76x2_key_delete(struct ieee80211vap *,
    const struct ieee80211_key *);
static void mt76x2_drain_mbufq(struct mt76x2_softc *);
static void mt76x2_mac_stop_light(struct mt76x2_softc *);
static void mt76x2_set_leds(struct mt76x2_softc *, uint8_t);
static void mt76x2_set_txrts(struct mt76x2_softc *);
static void mt76x2_abort_tsf_sync(struct mt76x2_softc *);
static void mt76x2_enable_tsf_sync(struct mt76x2_softc *);
static void mt76x2_enable_mrr(struct mt76x2_softc *);
static void mt76x2_set_txpreamble(struct mt76x2_softc *);
static void mt76x2_set_basicrates(struct mt76x2_softc *);
static int mt76x2_mcu_cmd(struct mt76x2_softc *, int, void *, int);
static int mt76x2_mcu_cmd_nowait(struct mt76x2_softc *, int, void *, int);
static int mt76x2_mcu_random_read(struct mt76x2_softc *, const uint32_t *,
    uint32_t *, int);
static int mt76x2_mcu_load_cr(struct mt76x2_softc *, uint8_t, uint8_t,
    uint8_t);
static int mt76x2_mcu_calibrate(struct mt76x2_softc *, int, uint32_t);
static void mt76x2_phy_channel_calibrate(struct mt76x2_softc *, int);
static void mt76x2_phy_set_rxpath(struct mt76x2_softc *);
static void mt76x2_phy_set_txdac(struct mt76x2_softc *);
/* mt76x2_phy_set_antenna() removed — Linux USB driver never calls it */
static void mt76x2_phy_set_band(struct mt76x2_softc *, int, int);
static void mt76x2_phy_set_bw(struct mt76x2_softc *,
    struct ieee80211_channel *, int);
static void mt76x2_read_rx_gain(struct mt76x2_softc *, int, int);
static void mt76x2_apply_gain_adj(struct mt76x2_softc *);
static void mt76x2_init_agc_gain(struct mt76x2_softc *);
static void mt76x2_configure_tx_delay(struct mt76x2_softc *, int, int);
static void mt76x2_phy_set_txpower_regs(struct mt76x2_softc *, int);
static void mt76x2_phy_set_txpower(struct mt76x2_softc *,
    struct ieee80211_channel *);
static int mt76x2_read_cfg(struct mt76x2_softc *, uint16_t, uint32_t *);
static int mt76x2_write_cfg(struct mt76x2_softc *, uint16_t, uint32_t);
static void mt76x2_temp_cal_to(void *);
static void mt76x2_temp_cal_cb(void *, int);
static void mt76x2_set_monitor_filter(struct mt76x2_softc *);
static void mt76x2_beacon_upload(struct mt76x2_softc *,
    struct ieee80211vap *);
static void mt76x2_phy_tssi_compensate(struct mt76x2_softc *);
static void mt76x2_phy_update_channel_gain(struct mt76x2_softc *);
static void mt76x2_phy_set_gain_val(struct mt76x2_softc *);
static void mt76x2_edcca_check(struct mt76x2_softc *);
static void mt76x2_check_mac_err(struct mt76x2_softc *);
static int mt76x2_tx_get_txpwr_adj(struct mt76x2_softc *, int);
static void mt76x2_updateprot(struct mt76x2_softc *);
static void mt76x2_update_beacon(struct ieee80211vap *, int);
static void mt76x2_update_beacon_cb(void *);

static eventhandler_tag mt76x2_etag;

/* Default MAC register values for MT7612U */
/*
 * Default MAC register init table — matches Linux mt76_write_mac_initvals()
 * in mt76x2/init.c exactly, followed by protection configs.
 * All addresses verified against Linux mt76x02_regs.h.
 */
static const struct mt76x2_mac_val mt7612_def_mac[] = {
	/* PBF + FCE */
	{ 0x0400, 0x00080c00 },	/* PBF_SYS_CTRL */
	{ 0x0404, 0x1efebcff },	/* PBF_CFG */
	{ 0x0800, 0x00000001 },	/* FCE_PSE_CTRL */
	/* MAC core — Linux writes 0 (no BBP reset) */
	{ 0x1004, 0x00000000 },	/* MAC_SYS_CTRL */
	{ 0x1018, 0x003e3f00 },	/* MAX_LEN_CFG */
	{ 0x1030, 0xaaa99887 },	/* AMPDU_MAX_LEN_20M1S */
	{ 0x1034, 0x000000aa },	/* AMPDU_MAX_LEN_20M2S */
	{ 0x1100, 0x33a40d0a },	/* XIFS_TIME_CFG */
	{ 0x1104, 0x00000209 },	/* BKOFF_SLOT_CFG */
	{ 0x1118, 0x00422010 },	/* TBTT_SYNC_CFG */
	{ 0x1204, 0x00000000 },	/* PWR_PIN_CFG */
	{ 0x1238, 0x001700c8 },	/* (undocumented, from Linux) */
	/* TX config */
	{ 0x1330, 0x00101001 },	/* TX_SW_CFG0 */
	{ 0x1334, 0x00010000 },	/* TX_SW_CFG1 */
	{ 0x1338, 0x00000000 },	/* TX_SW_CFG2 */
	{ 0x1340, 0x0400583f },	/* TXOP_CTRL_CFG */
	{ 0x1344, 0x00ffff20 },	/* TX_RTS_CFG */
	{ 0x1348, 0x000a2290 },	/* TX_TIMEOUT_CFG */
	{ 0x134c, 0x47f01f0f },	/* TX_RETRY_CFG */
	{ 0x1380, 0x002c00dc },	/* EXP_ACK_TIME */
	{ 0x13e0, 0xe3f42004 },	/* TX_PROT_CFG6 */
	{ 0x13e4, 0xe3f42084 },	/* TX_PROT_CFG7 */
	{ 0x13e8, 0xe3f42104 },	/* TX_PROT_CFG8 */
	{ 0x13ec, 0x00060fff },	/* PIFS_TX_CFG */
	/* RX config */
	{ 0x1400, 0x00015f97 },	/* RX_FILTR_CFG */
	/* Rate tables — corrected to match Linux */
	{ 0x1408, 0x0000017f },	/* LEGACY_BASIC_RATE */
	{ 0x140c, 0x00004003 },	/* HT_BASIC_RATE */
	{ 0x150c, 0x00000003 },	/* PN_PAD_MODE */
	{ 0x1608, 0x00000002 },	/* TXOP_HLDR_ET */
	{ 0x0a44, 0x00000000 },	/* (undocumented, from Linux) */
	{ 0x0260, 0x00000000 },	/* HEADER_TRANS_CTRL_REG */
	{ 0x0250, 0x00000000 },	/* TSO_CTRL */
	{ 0x120c, 0x00000000 },	/* AUX_CLK_CFG */
	{ 0x1264, 0x00000000 },	/* DACCLK_EN_DLY_CFG */
	/* TX ALC + power */
	{ 0x13c0, 0x00000000 },	/* TX_ALC_CFG_4 */
	{ 0x13c8, 0x00000000 },	/* TX_ALC_VGA3 */
	{ 0x1314, 0x3a3a3a3a },	/* TX_PWR_CFG_0 */
	{ 0x1318, 0x3a3a3a3a },	/* TX_PWR_CFG_1 */
	{ 0x131c, 0x3a3a3a3a },	/* TX_PWR_CFG_2 */
	{ 0x1320, 0x3a3a3a3a },	/* TX_PWR_CFG_3 */
	{ 0x1324, 0x3a3a3a3a },	/* TX_PWR_CFG_4 */
	{ 0x13d4, 0x3a3a3a3a },	/* TX_PWR_CFG_7 */
	{ 0x13d8, 0x0000003a },	/* TX_PWR_CFG_8 */
	{ 0x13dc, 0x0000003a },	/* TX_PWR_CFG_9 */
	/* EFUSE, flow control, misc */
	{ 0x0024, 0x0000d000 },	/* EFUSE_CTRL */
	{ 0x0a38, 0x0000000a },	/* PAUSE_ENABLE_CONTROL1 */
	{ 0x0824, 0x60401c18 },	/* FCE_WLAN_FLOW_CONTROL1 */
	{ 0x0210, 0x94ff0000 },	/* WPDMA_DELAY_INT_CFG */
	{ 0x1478, 0x00000004 },	/* TX_SW_CFG3 */
	{ 0x1384, 0x00001818 },	/* HT_FBK_TO_LEGACY */
	{ 0x1358, 0xedcba980 },	/* VHT_HT_FBK_CFG1 */
	{ 0x1648, 0x00830083 },	/* PROT_AUTO_TX_CFG */
	{ 0x1410, 0x000001ff },	/* HT_CTRL_CFG */
	{ 0x1350, 0x00001020 },	/* TX_LINK_CFG */
	/* Protection configs — correct per-type values from Linux */
	{ 0x1364, 0x07f40003 },	/* CCK_PROT_CFG */
	{ 0x1368, 0x07f42004 },	/* OFDM_PROT_CFG */
	{ 0x136c, 0x01752004 },	/* MM20_PROT_CFG */
	{ 0x1370, 0x03f52084 },	/* MM40_PROT_CFG */
	{ 0x1374, 0x01752004 },	/* GF20_PROT_CFG */
	{ 0x1378, 0x03f52084 },	/* GF40_PROT_CFG */
};

/* USB transfer configuration */
static const struct usb_config mt76x2_config[MT76X2_N_XFER] = {
	/*
	 * Endpoint index mapping matches Linux mt76 out_ep[] order:
	 *   ep_index 0 → EP 0x08 (inband cmd / firmware)
	 *   ep_index 1 → EP 0x04 (AC_BE)
	 *   ep_index 2 → EP 0x05 (AC_BK)
	 *   ep_index 3 → EP 0x06 (AC_VI)
	 *   ep_index 4 → EP 0x07 (AC_VO)
	 *   ep_index 5 → EP 0x09 (HCCA)
	 */
	[MT76X2_BULK_TX_BE] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.ep_index = 1,
		.direction = UE_DIR_OUT,
		.bufsize = MT76X2_MAX_TXSZ,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1 },
		.callback = mt76x2_bulk_tx_callback0,
		.timeout = 5000,
	},
	[MT76X2_BULK_TX_BK] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.ep_index = 2,
		.direction = UE_DIR_OUT,
		.bufsize = MT76X2_MAX_TXSZ,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1 },
		.callback = mt76x2_bulk_tx_callback1,
		.timeout = 5000,
	},
	[MT76X2_BULK_TX_VI] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.ep_index = 3,
		.direction = UE_DIR_OUT,
		.bufsize = MT76X2_MAX_TXSZ,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1 },
		.callback = mt76x2_bulk_tx_callback2,
		.timeout = 5000,
	},
	[MT76X2_BULK_TX_VO] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.ep_index = 4,
		.direction = UE_DIR_OUT,
		.bufsize = MT76X2_MAX_TXSZ,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1 },
		.callback = mt76x2_bulk_tx_callback3,
		.timeout = 5000,
	},
	[MT76X2_BULK_TX_HCCA] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.ep_index = 5,
		.direction = UE_DIR_OUT,
		.bufsize = MT76X2_MAX_TXSZ,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1,
			   .no_pipe_ok = 1 },
		.callback = mt76x2_bulk_tx_callback4,
		.timeout = 5000,
	},
	[MT76X2_BULK_FW] = {
		.type = UE_BULK,
		.endpoint = 0x08,
		.direction = UE_DIR_OUT,
		.bufsize = MT76X2_FW_MAX_PAYLOAD,
		.flags = { .pipe_bof = 1, .no_pipe_ok = 1 },
		.callback = mt76x2_fw_callback,
		.timeout = 5000,
	},
	[MT76X2_BULK_RX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.ep_index = 0,
		.direction = UE_DIR_IN,
		.bufsize = MT76X2_MAX_RXSZ,
		.flags = { .pipe_bof = 1, .short_xfer_ok = 1 },
		.callback = mt76x2_bulk_rx_callback,
	},
	[MT76X2_BULK_MCU_RX] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.ep_index = 1,
		.direction = UE_DIR_IN,
		.bufsize = 512,
		.flags = { .pipe_bof = 1, .short_xfer_ok = 1,
			   .no_pipe_ok = 1 },
		.callback = mt76x2_mcu_rx_callback,
		.timeout = 500,		/* 500 ms */
	},
	[MT76X2_BULK_TX_PRIO] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.ep_index = 6,
		.direction = UE_DIR_OUT,
		.bufsize = MT76X2_MAX_TXSZ,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1,
			   .no_pipe_ok = 1 },
		.callback = mt76x2_bulk_tx_callback5,
		.timeout = 5000,
	},
};

static uint8_t mt76x2_wme_ac_xfer_map[4] = {
	[WME_AC_BE] = MT76X2_BULK_TX_BE,
	[WME_AC_BK] = MT76X2_BULK_TX_BK,
	[WME_AC_VI] = MT76X2_BULK_TX_VI,
	[WME_AC_VO] = MT76X2_BULK_TX_VO,
};

/*
 * ============================================================
 * Register read/write via USB vendor requests
 * ============================================================
 */

static int
mt76x2_read(struct mt76x2_softc *sc, uint16_t reg, uint32_t *val)
{
	uint32_t tmp;
	usb_device_request_t req;
	int error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = MT76X2_VEND_MULTI_READ;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 4);

	error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, &tmp);
	if (error == 0)
		*val = le32toh(tmp);
	else
		*val = 0xffffffff;
	return (error);
}

static int
mt76x2_write(struct mt76x2_softc *sc, uint16_t reg, uint32_t val)
{
	usb_device_request_t req;
	int error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MT76X2_VEND_MULTI_WRITE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 4);

	val = htole32(val);
	error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, &val);
	return (error);
}

static int
mt76x2_read_cfg(struct mt76x2_softc *sc, uint16_t reg, uint32_t *val)
{
	usb_device_request_t req;
	uint32_t tmp;
	int error;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = MT76X2_VEND_READ_CFG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 4);

	error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, &tmp);
	if (error == 0)
		*val = le32toh(tmp);
	else
		*val = 0xffffffff;
	return (error);
}

static int
mt76x2_write_cfg(struct mt76x2_softc *sc, uint16_t reg, uint32_t val)
{
	usb_device_request_t req;
	int error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MT76X2_VEND_WRITE_CFG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 4);

	val = htole32(val);
	error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, &val);
	return (error);
}

static int
mt76x2_vendor_request(struct mt76x2_softc *sc, uint8_t request,
    uint8_t type, uint16_t val, uint16_t idx, void *buf, uint16_t len)
{
	usb_device_request_t req;
	int error;

	req.bmRequestType = type;
	req.bRequest = request;
	USETW(req.wValue, val);
	USETW(req.wIndex, idx);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req, buf);
	return (error);
}

static int
mt76x2_vendor_single_write(struct mt76x2_softc *sc, uint8_t request,
    uint16_t offset, uint32_t val)
{
	int error;

	/* Write low 16 bits, then high 16 bits at offset+2 */
	error = mt76x2_vendor_request(sc, request,
	    UT_WRITE_VENDOR_DEVICE, val & 0xffff, offset, NULL, 0);
	if (error != 0)
		return (error);
	error = mt76x2_vendor_request(sc, request,
	    UT_WRITE_VENDOR_DEVICE, (val >> 16) & 0xffff, offset + 2,
	    NULL, 0);
	return (error);
}

static int __unused
mt76x2_write_region_1(struct mt76x2_softc *sc, uint16_t reg,
    const uint8_t *buf, int len)
{
	usb_device_request_t req;
	int error = 0;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = MT76X2_VEND_MULTI_WRITE;
	USETW(req.wValue, 0);

	/* write in chunks */
	while (len > 0) {
		int xlen = MIN(len, 64);

		USETW(req.wIndex, reg);
		USETW(req.wLength, xlen);
		error = usbd_do_request(sc->sc_udev, &sc->sc_mtx, &req,
		    __DECONST(void *, buf));
		if (error != 0)
			break;
		reg += xlen;
		buf += xlen;
		len -= xlen;
	}
	return (error);
}

/*
 * ============================================================
 * EEPROM access
 * ============================================================
 */

/*
 * MediaTek sign-magnitude decoding for EEPROM RSSI offset values.
 * Equivalent to Linux mt76x2_set_rssi_offset():
 *   field_valid(val) + mt76x02_sign_extend_optional(val, 7)
 *
 * Encoding: BIT(7)=enable, BIT(6)=sign (1=positive, 0=negative),
 * bits [5:0]=magnitude.  Returns 0 if val is 0, 0xff, or enable bit clear.
 */
static int8_t
mt76x2_rssi_offset_decode(uint8_t val)
{

	if (val == 0 || val == 0xff)
		return (0);
	if (!(val & 0x80))
		return (0);
	if (val & 0x40)
		return (int8_t)(val & 0x3f);
	return (-(int8_t)(val & 0x3f));
}

/*
 * Compute calibrated RSSI from raw RXWI value.
 * Matches Linux mt76x02_mac_get_rssi() from mt76x02_mac.c.
 *
 * rssi = raw + rssi_offset[chain] - lna_gain
 */
static int
mt76x2_get_rssi(struct mt76x2_softc *sc, int8_t raw, int chain)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int8_t offset;
	int rssi;

	if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))
		offset = sc->rssi_offset_5g[chain & 1];
	else
		offset = sc->rssi_offset_2g[chain & 1];

	rssi = raw + offset - sc->lna_gain;
	return (rssi);
}

static int
mt76x2_read_eeprom(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;
	int error, i;

	/*
	 * Read EEPROM via USB vendor request (bRequest=0x09).
	 * This matches Linux mt76x2u_init_eeprom() which uses
	 * MT_VEND_ADDR(EEPROM, i) → bRequest=MT_VEND_READ_EEPROM.
	 * Our old EFUSE register method may return different data
	 * since USB dongles often use flash storage, not eFuse.
	 */
	for (i = 0; i + 4 <= MT76X2_EE_SIZE; i += 4) {
		usb_device_request_t req;
		uint32_t tmp;

		req.bmRequestType = UT_READ_VENDOR_DEVICE;
		req.bRequest = MT76X2_VEND_READ_EEPROM;
		USETW(req.wValue, 0);
		USETW(req.wIndex, i);
		USETW(req.wLength, 4);

		error = usbd_do_request(sc->sc_udev, &sc->sc_mtx,
		    &req, &tmp);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "EEPROM read failed at 0x%x: %d\n", i, error);
			return (error);
		}
		tmp = le32toh(tmp);
		sc->eeprom[i] = tmp & 0xff;
		sc->eeprom[i + 1] = (tmp >> 8) & 0xff;
		sc->eeprom[i + 2] = (tmp >> 16) & 0xff;
		sc->eeprom[i + 3] = (tmp >> 24) & 0xff;
	}

	/* MAC address */
	IEEE80211_ADDR_COPY(ic->ic_macaddr,
	    &sc->eeprom[MT76X2_EE_MAC_ADDR]);

	/* NIC configuration */
	val = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF0]);
	sc->nrxchains = val & MT76X2_EE_NIC_CONF0_RX_PATH;
	sc->ntxchains = (val & MT76X2_EE_NIC_CONF0_TX_PATH) >> 4;

	/* Clamp chain counts for MT7612 (2T2R max) */
	if (sc->nrxchains == 0 || sc->nrxchains > 2)
		sc->nrxchains = 2;
	if (sc->ntxchains == 0 || sc->ntxchains > 2)
		sc->ntxchains = 2;

	/* Default TX power limit (30 = 15 dBm in 0.5 dBm units) */
	sc->txpower_conf = 30;

	/* RSSI offsets — apply MediaTek sign-magnitude decoding */
	sc->rssi_offset_2g[0] =
	    mt76x2_rssi_offset_decode(sc->eeprom[MT76X2_EE_RSSI_OFFSET_2G_0]);
	sc->rssi_offset_2g[1] =
	    mt76x2_rssi_offset_decode(sc->eeprom[MT76X2_EE_RSSI_OFFSET_2G_1]);
	sc->rssi_offset_5g[0] =
	    mt76x2_rssi_offset_decode(sc->eeprom[MT76X2_EE_RSSI_OFFSET_5G_0]);
	sc->rssi_offset_5g[1] =
	    mt76x2_rssi_offset_decode(sc->eeprom[MT76X2_EE_RSSI_OFFSET_5G_1]);

	/* LNA gains */
	sc->lna_gain_2g = sc->eeprom[MT76X2_EE_LNA_GAIN_2G];
	sc->lna_gain_5g[0] = sc->eeprom[MT76X2_EE_LNA_GAIN_5G_0];
	sc->lna_gain_5g[1] = sc->eeprom[MT76X2_EE_LNA_GAIN_5G_1];
	sc->lna_gain_5g[2] = sc->eeprom[MT76X2_EE_LNA_GAIN_5G_2];

	return (0);
}

/*
 * ============================================================
 * MAC address / BSSID setup
 * ============================================================
 */

static void
mt76x2_set_macaddr(struct mt76x2_softc *sc, const uint8_t *addr)
{
	uint32_t addr_lo, addr_hi;

	addr_lo = (uint32_t)addr[0] | (uint32_t)addr[1] << 8 |
	    (uint32_t)addr[2] << 16 | (uint32_t)addr[3] << 24;
	addr_hi = (uint32_t)addr[4] | (uint32_t)addr[5] << 8;

	/* MAC address registers */
	mt76x2_write(sc, MT76X2_MAC_ADDR_DW0, addr_lo);
	mt76x2_write(sc, MT76X2_MAC_ADDR_DW1, addr_hi | 0xffU << 16);

	/*
	 * BSSID registers — Linux mt76x02_mac_setaddr() writes the MAC
	 * address into BSSID_DW0/DW1 with MBSS_MODE=3, MBSS_LOCAL_BIT,
	 * and MBEACON_N=7.  Without this, the MAC hardware filters out
	 * received frames because they don't match the zeroed BSSID.
	 */
	mt76x2_write(sc, MT76X2_MAC_BSSID_DW0, addr_lo);
	mt76x2_write(sc, MT76X2_MAC_BSSID_DW1, addr_hi |
	    MT76X2_MAC_BSSID_DW1_MBSS_MODE |	/* MBSS_MODE = 3 */
	    MT76X2_MAC_BSSID_DW1_MBSS_LOCAL |	/* locally administered */
	    MT76X2_MAC_BSSID_DW1_MBEACON_N);	/* MBEACON_N = 7 */
}

static void
mt76x2_set_bssid(struct mt76x2_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	mt76x2_write(sc, MT76X2_MAC_BSSID_DW0,
	    (uint32_t)addr[0] | (uint32_t)addr[1] << 8 |
	    (uint32_t)addr[2] << 16 | (uint32_t)addr[3] << 24);

	/* Preserve MBSS bits [31:16] in DW1, update MAC bytes only */
	mt76x2_read(sc, MT76X2_MAC_BSSID_DW1, &tmp);
	tmp &= 0xffff0000;
	tmp |= (uint32_t)addr[4] | (uint32_t)addr[5] << 8;
	mt76x2_write(sc, MT76X2_MAC_BSSID_DW1, tmp);
}

/*
 * ============================================================
 * Power on / firmware loading
 * ============================================================
 */

static void
mt76x2_power_on_rf(struct mt76x2_softc *sc, int unit)
{
	int shift = unit ? 8 : 0;
	uint32_t val;

	/* Enable RF BG */
	mt76x2_read_cfg(sc, 0x0130, &val);
	val |= (1U << shift);
	mt76x2_write_cfg(sc, 0x0130, val);
	DELAY(10);

	/* Enable RFDIG LDO/AFE/ABB/ADDA */
	mt76x2_read_cfg(sc, 0x0130, &val);
	val |= ((1U << 1) | (1U << 3) | (1U << 4) | (1U << 5)) << shift;
	mt76x2_write_cfg(sc, 0x0130, val);
	DELAY(10);

	/* Switch RFDIG power to internal LDO */
	mt76x2_read_cfg(sc, 0x0130, &val);
	val &= ~((1U << 2) << shift);
	mt76x2_write_cfg(sc, 0x0130, val);
	DELAY(10);

	/* RF patch */
	mt76x2_read_cfg(sc, 0x0130, &val);
	val |= (1U << 0) | (1U << 16);
	mt76x2_write_cfg(sc, 0x0130, val);
	DELAY(1);

	mt76x2_read_cfg(sc, 0x001c, &val);
	val &= ~0xff;
	val |= 0x30;
	mt76x2_write_cfg(sc, 0x001c, val);

	mt76x2_write_cfg(sc, 0x0014, 0x484f);
	DELAY(1);

	mt76x2_read_cfg(sc, 0x0130, &val);
	val |= (1U << 17);
	mt76x2_write_cfg(sc, 0x0130, val);
	DELAY(150);

	mt76x2_read_cfg(sc, 0x0130, &val);
	val &= ~(1U << 16);
	mt76x2_write_cfg(sc, 0x0130, val);
	DELAY(50);

	mt76x2_read_cfg(sc, 0x014c, &val);
	val |= (1U << 19) | (1U << 20);
	mt76x2_write_cfg(sc, 0x014c, val);

	/* Enable RF read/write */
	mt76x2_read(sc, 0x0530, &val);
	val |= 0xf;
	mt76x2_write(sc, 0x0530, val);
}

static void
mt76x2_power_on(struct mt76x2_softc *sc)
{
	uint32_t val;

	/* Turn on WL MTCMOS */
	mt76x2_read_cfg(sc, 0x0148, &val);
	val |= MT76X2_MTC_PWR_UP;
	mt76x2_write_cfg(sc, 0x0148, val);

	/* Wait for power up */
	int ntries;
	for (ntries = 0; ntries < 100; ntries++) {
		mt76x2_read_cfg(sc, 0x0148, &val);
		if (val & (MT76X2_MTC_STATE_UP | MT76X2_MTC_PWR_ACK |
		    MT76X2_MTC_PWR_ACK_S))
			break;
		DELAY(10);
	}

	mt76x2_read_cfg(sc, 0x0148, &val);
	val &= ~(0x7f << 16);
	mt76x2_write_cfg(sc, 0x0148, val);
	DELAY(10);

	mt76x2_read_cfg(sc, 0x0148, &val);
	val &= ~(0xf << 24);
	mt76x2_write_cfg(sc, 0x0148, val);
	DELAY(10);

	mt76x2_read_cfg(sc, 0x0148, &val);
	val |= (0xf << 24);
	mt76x2_write_cfg(sc, 0x0148, val);

	mt76x2_read_cfg(sc, 0x0148, &val);
	val &= ~0xfff;
	mt76x2_write_cfg(sc, 0x0148, val);

	/* Turn on AD/DA power */
	mt76x2_read_cfg(sc, 0x1204, &val);
	val &= ~(1U << 3);
	mt76x2_write_cfg(sc, 0x1204, val);

	/* WLAN function enable */
	mt76x2_read_cfg(sc, 0x0080, &val);
	val |= (1U << 0);
	mt76x2_write_cfg(sc, 0x0080, val);

	/* Release BBP software reset */
	mt76x2_read_cfg(sc, 0x0064, &val);
	val &= ~(1U << 18);
	mt76x2_write_cfg(sc, 0x0064, val);

	mt76x2_power_on_rf(sc, 0);
	mt76x2_power_on_rf(sc, 1);

}

static void
mt76x2_usb_dma_setup(struct mt76x2_softc *sc)
{
	uint32_t val;

	/*
	 * Configure USB DMA via CFG register space (0x9018).
	 * RMW to preserve firmware-configured bits — the bootloader/
	 * firmware may set bits that we don't know about.  A full
	 * overwrite causes MCU LOAD_CR to timeout.
	 *
	 * Enable RX bulk aggregation: the hardware packs multiple
	 * received frames into one USB bulk IN transfer.  Linux
	 * disables this because it has 128 URBs in flight.  We have
	 * only 1 RX transfer (FreeBSD USB serialization), so
	 * aggregation is critical — it multiplies effective RX
	 * throughput by the number of frames per transfer.
	 *
	 * AGG_TOUT = 0x20 (~1 ms at 33 µs ticks): how long HW waits
	 * for more frames before sending a short aggregate.
	 * AGG_LMT = 8: max frames per USB transfer.
	 * Buffer size (MT76X2_MAX_RXSZ) must be large enough for
	 * AGG_LMT frames; 16384 bytes fits ~8 typical data frames.
	 */
	mt76x2_read_cfg(sc, MT76X2_USB_U3DMA_CFG, &val);
	val |= MT76X2_USB_DMA_CFG_RX_DROP_PAD |
	    MT76X2_USB_DMA_CFG_RX_BULK_EN |
	    MT76X2_USB_DMA_CFG_TX_BULK_EN;
	val &= ~(0xffffU);	/* clear AGG_TOUT[7:0] and AGG_LMT[15:8] */
	val |= (0x20 << MT76X2_USB_DMA_CFG_RX_AGG_TOUT_SHIFT);
	val |= (8 << MT76X2_USB_DMA_CFG_RX_AGG_LMT_SHIFT);
	val |= MT76X2_USB_DMA_CFG_RX_AGG_EN;
	mt76x2_write_cfg(sc, MT76X2_USB_U3DMA_CFG, val);
}

static int
mt76x2_wait_for_mac(struct mt76x2_softc *sc)
{
	uint32_t val;
	int ntries, error;

	for (ntries = 0; ntries < 500; ntries++) {
		error = mt76x2_read(sc, MT76X2_MAC_CSR0, &val);
		if (error != 0)
			return (error);
		if (val != 0 && val != 0xffffffff)
			return (0);
		DELAY(5000);
	}
	device_printf(sc->sc_dev, "MAC not ready (CSR0=0x%08x)\n", val);
	return (ETIMEDOUT);
}

static void
mt76x2_fw_upload_setup(struct mt76x2_softc *sc)
{
	/*
	 * Match Linux mt76x2u_mcu_load_rom_patch / _load_firmware:
	 *   1. Enable USB DMA TX/RX (via CFG register space)
	 *   2. Vendor reset (VEND_DEV_MODE, wValue=1)
	 *   3. Wait 5ms
	 *   4. FCE setup (via normal register space)
	 */

	/*
	 * Match Linux mt76x2u_mcu_load_firmware() order exactly:
	 *   1. Vendor reset (MCU fw_reset)
	 *   2. Sleep 5-10ms
	 *   3. Enable USB DMA TX/RX (via CFG space 0x9018)
	 *   4. FCE setup
	 */

	/* Vendor reset — resets MCU, prepares for fresh firmware upload */
	mt76x2_vendor_request(sc, MT76X2_VEND_DEV_MODE,
	    UT_WRITE_VENDOR_DEVICE, 0x01, 0, NULL, 0);
	DELAY(5000);

	/* Enable USB DMA TX/RX for firmware upload */
	mt76x2_write_cfg(sc, MT76X2_USB_U3DMA_CFG, 0x00C00020);

	/* Enable FCE */
	mt76x2_write(sc, MT76X2_FCE_PSE_CTRL, 0x01);
	mt76x2_write(sc, MT76X2_TX_CPU_FROM_FCE_BASE, 0x400230);
	mt76x2_write(sc, MT76X2_TX_CPU_FROM_FCE_MAX_CNT, 0x01);
	mt76x2_write(sc, MT76X2_FCE_PDMA_GLOBAL_CONF, 0x44);
	mt76x2_write(sc, MT76X2_FCE_SKIP_FS, 0x03);
}

static int
mt76x2_fw_send_data(struct mt76x2_softc *sc, const uint8_t *data,
    int len, uint32_t dst_addr, int max_payload)
{
	int pos = 0, mlen, orig_mlen, error;
	int max_len = max_payload - 8;
	uint32_t val, info;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (sc->fw_buf == NULL || sc->fw_bufsz < max_payload) {
		if (sc->fw_buf != NULL)
			free(sc->fw_buf, M_USBDEV);
		sc->fw_buf = malloc(max_payload, M_USBDEV, M_NOWAIT | M_ZERO);
		if (sc->fw_buf == NULL) {
			sc->fw_bufsz = 0;
			return (ENOMEM);
		}
		sc->fw_bufsz = max_payload;
	}

	while (pos < len) {
		orig_mlen = MIN(len - pos, max_len);
		mlen = roundup(orig_mlen, 4);

		/* Build firmware chunk: 4-byte info header + payload
		 * Bit layout (from Linux mt76x02_dma.h):
		 *   [15:0]  = length
		 *   [29:27] = port (CPU_TX_PORT = 2)
		 *   [30]    = MSG_TYPE_CMD
		 */
		info = htole32(
		    ((uint32_t)orig_mlen & 0xffff) |
		    ((uint32_t)MT76X2_CPU_TX_PORT << 27) |
		    MT76X2_MCU_MSG_TYPE_CMD);

		memset(sc->fw_buf, 0, max_payload);
		memcpy(sc->fw_buf, &info, sizeof(info));
		memcpy(sc->fw_buf + sizeof(info), data + pos, orig_mlen);

		/* Tell hardware where to DMA this chunk */
		error = mt76x2_vendor_single_write(sc, MT76X2_VEND_WRITE_FCE,
		    MT76X2_FCE_DMA_ADDR & 0xffff, dst_addr + pos);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "FCE DMA_ADDR write failed: %d\n", error);
			return (error);
		}
		error = mt76x2_vendor_single_write(sc, MT76X2_VEND_WRITE_FCE,
		    MT76X2_FCE_DMA_LEN & 0xffff, mlen << 16);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "FCE DMA_LEN write failed: %d\n", error);
			return (error);
		}

		MT76X2_DPRINTF(sc, MT76X2_DEBUG_FIRMWARE,
		    "FW chunk: pos=%d mlen=%d dst=0x%x info=0x%08x\n",
		    pos, mlen, dst_addr + pos, le32toh(info));

		/* Set up bulk transfer state */
		sc->fw_buflen = mlen + (int)sizeof(info);
		/* Pad to 4-byte alignment + 4 bytes trailing */
		sc->fw_buflen = roundup(sc->fw_buflen, 4) + 4;
		sc->fw_chunk_done = 0;
		sc->fw_error = 0;

		/* Kick the bulk FW transfer */
		usbd_transfer_start(sc->sc_xfer[MT76X2_BULK_FW]);

		/* Wait for completion */
		error = msleep(&sc->fw_chunk_done, &sc->sc_mtx, 0,
		    "mt76fw", 5 * hz);

		if (error == EWOULDBLOCK) {
			device_printf(sc->sc_dev,
			    "firmware upload timeout at pos=%d\n", pos);
			usbd_transfer_stop(sc->sc_xfer[MT76X2_BULK_FW]);
			return (ETIMEDOUT);
		}
		if (sc->fw_error != 0) {
			device_printf(sc->sc_dev,
			    "firmware upload failed: %d\n", sc->fw_error);
			return (sc->fw_error);
		}

		/* Advance CPU index */
		mt76x2_read(sc, MT76X2_TX_CPU_FROM_FCE_CPU_IDX, &val);
		val++;
		mt76x2_write(sc, MT76X2_TX_CPU_FROM_FCE_CPU_IDX, val);

		pos += orig_mlen;
		DELAY(5000);
	}

	return (0);
}

static int
mt76x2_load_rom_patch(struct mt76x2_softc *sc)
{
	const struct firmware *fw;
	const struct mt76x2_patch_header *hdr;
	uint32_t val, patch_mask, patch_reg;
	int error;
	static const uint8_t enable_patch[] = {
		0x6f, 0xfc, 0x08, 0x01,
		0x20, 0x04, 0x00, 0x00,
		0x00, 0x09, 0x00,
	};
	static const uint8_t reset_wmt[] = {
		0x6f, 0xfc, 0x05, 0x01,
		0x07, 0x01, 0x00, 0x04,
	};

	/* Check if patch is already applied */
	if (sc->asic_rev >= MT76X2_REV_E3) {
		patch_mask = (1U << 0);
		patch_reg = MT76X2_MCU_CLOCK_CTL;
	} else {
		patch_mask = (1U << 1);
		patch_reg = MT76X2_MCU_COM_REG0;
	}

	mt76x2_read(sc, patch_reg, &val);
	if (val & patch_mask) {
		device_printf(sc->sc_dev, "ROM patch already applied\n");
		return (0);
	}

	MT76X2_UNLOCK(sc);
	fw = firmware_get("mt76x2_rom_patch");
	MT76X2_LOCK(sc);
	if (fw == NULL) {
		device_printf(sc->sc_dev,
		    "failed to load ROM patch firmware\n");
		return (ENOENT);
	}

	if (fw->datasize <= sizeof(*hdr)) {
		device_printf(sc->sc_dev, "ROM patch too short\n");
		MT76X2_UNLOCK(sc);
		firmware_put(fw, FIRMWARE_UNLOAD);
		MT76X2_LOCK(sc);
		return (EINVAL);
	}

	hdr = (const struct mt76x2_patch_header *)fw->data;
	device_printf(sc->sc_dev, "ROM patch build: %.16s\n",
	    hdr->build_time);

	mt76x2_fw_upload_setup(sc);

	error = mt76x2_fw_send_data(sc,
	    (const uint8_t *)fw->data + sizeof(*hdr),
	    fw->datasize - sizeof(*hdr),
	    MT76X2_MCU_ROM_PATCH_OFFSET,
	    MT76X2_ROM_PATCH_MAX_PAYLOAD);

	MT76X2_UNLOCK(sc);
	firmware_put(fw, FIRMWARE_UNLOAD);
	MT76X2_LOCK(sc);

	if (error != 0) {
		device_printf(sc->sc_dev,
		    "failed to upload ROM patch: %d\n", error);
		return (error);
	}

	/* Enable patch (class request, not vendor - matches Linux USB_TYPE_CLASS) */
	{
		uint8_t buf[11];
		memcpy(buf, enable_patch, sizeof(enable_patch));
		error = mt76x2_vendor_request(sc, MT76X2_VEND_DEV_MODE,
		    UT_WRITE_CLASS_DEVICE, 0x12, 0,
		    buf, sizeof(enable_patch));
		if (error != 0)
			device_printf(sc->sc_dev,
			    "enable_patch vendor request failed: %d\n", error);
	}

	/* Reset WMT (class request, not vendor) */
	{
		uint8_t buf[8];
		memcpy(buf, reset_wmt, sizeof(reset_wmt));
		error = mt76x2_vendor_request(sc, MT76X2_VEND_DEV_MODE,
		    UT_WRITE_CLASS_DEVICE, 0x12, 0,
		    buf, sizeof(reset_wmt));
		if (error != 0)
			device_printf(sc->sc_dev,
			    "reset_wmt vendor request failed: %d\n", error);
	}

	DELAY(20000);

	/* Verify patch was applied */
	int ntries;
	for (ntries = 0; ntries < 100; ntries++) {
		mt76x2_read(sc, patch_reg, &val);
		if (val & patch_mask)
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "ROM patch failed to apply\n");
		return (ETIMEDOUT);
	}

	return (0);
}

static int
mt76x2_load_firmware(struct mt76x2_softc *sc)
{
	const struct firmware *fw;
	const struct mt76x2_fw_header *hdr;
	uint32_t val, ilm_len, dlm_len, dlm_offset;
	int error;

	/*
	 * Always re-upload firmware, matching Linux mt76x2u_mcu_load_firmware().
	 * Linux does NOT skip re-upload when firmware is already running —
	 * it always resets, re-uploads, and re-starts the MCU.  Skipping
	 * the upload leaves stale firmware state (calibration caches,
	 * routing tables) from a previous driver load.
	 */

	MT76X2_UNLOCK(sc);
	fw = firmware_get("mt76x2_fw");
	MT76X2_LOCK(sc);
	if (fw == NULL) {
		device_printf(sc->sc_dev,
		    "failed to load firmware\n");
		return (ENOENT);
	}

	if (fw->datasize < sizeof(*hdr)) {
		device_printf(sc->sc_dev, "firmware too short\n");
		MT76X2_UNLOCK(sc);
		firmware_put(fw, FIRMWARE_UNLOAD);
		MT76X2_LOCK(sc);
		return (EINVAL);
	}

	hdr = (const struct mt76x2_fw_header *)fw->data;
	ilm_len = le32toh(hdr->ilm_len);
	dlm_len = le32toh(hdr->dlm_len);

	if (fw->datasize < sizeof(*hdr) + ilm_len + dlm_len) {
		device_printf(sc->sc_dev, "firmware payload too short\n");
		MT76X2_UNLOCK(sc);
		firmware_put(fw, FIRMWARE_UNLOAD);
		MT76X2_LOCK(sc);
		return (EINVAL);
	}

	device_printf(sc->sc_dev, "Firmware Version: %d.%d.%02d\n",
	    (le16toh(hdr->fw_ver) >> 12) & 0xf,
	    (le16toh(hdr->fw_ver) >> 8) & 0xf,
	    le16toh(hdr->fw_ver) & 0xf);
	device_printf(sc->sc_dev, "Build: %x  Time: %.16s\n",
	    le16toh(hdr->build_ver), hdr->build_time);

	mt76x2_fw_upload_setup(sc);

	/* Upload ILM */
	error = mt76x2_fw_send_data(sc,
	    (const uint8_t *)fw->data + sizeof(*hdr), ilm_len,
	    MT76X2_MCU_ILM_OFFSET, MT76X2_FW_MAX_PAYLOAD);
	if (error != 0)
		goto fail;

	/* Upload DLM */
	dlm_offset = MT76X2_MCU_DLM_OFFSET;
	if (sc->asic_rev >= MT76X2_REV_E3)
		dlm_offset += 0x800;

	error = mt76x2_fw_send_data(sc,
	    (const uint8_t *)fw->data + sizeof(*hdr) + ilm_len,
	    dlm_len, dlm_offset, MT76X2_FW_MAX_PAYLOAD);
	if (error != 0)
		goto fail;

	/* Start firmware (load IVB) */
	mt76x2_vendor_request(sc, MT76X2_VEND_DEV_MODE,
	    UT_WRITE_VENDOR_DEVICE, 0x12, 0, NULL, 0);

	/* Wait for firmware to start */
	int ntries;
	for (ntries = 0; ntries < 100; ntries++) {
		mt76x2_read(sc, MT76X2_MCU_COM_REG0, &val);
		if (val & 1)
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "firmware failed to start\n");
		error = ETIMEDOUT;
		goto fail;
	}

	/* Acknowledge firmware start */
	mt76x2_read(sc, MT76X2_MCU_COM_REG0, &val);
	val |= (1U << 1);
	mt76x2_write(sc, MT76X2_MCU_COM_REG0, val);

	/* Re-enable FCE */
	mt76x2_write(sc, MT76X2_FCE_PSE_CTRL, 0x01);

	device_printf(sc->sc_dev, "firmware running\n");

fail:
	MT76X2_UNLOCK(sc);
	firmware_put(fw, FIRMWARE_UNLOAD);
	MT76X2_LOCK(sc);
	return (error);
}

static int
mt76x2_load_microcode(struct mt76x2_softc *sc)
{
	int error;

	error = mt76x2_load_rom_patch(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "ROM patch load failed: %d\n",
		    error);
		return (error);
	}

	error = mt76x2_load_firmware(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "firmware load failed: %d\n",
		    error);
		return (error);
	}

	return (0);
}

/*
 * ============================================================
 * MCU commands
 * ============================================================
 */

static int
mt76x2_mcu_cmd(struct mt76x2_softc *sc, int cmd, void *data, int len)
{
	uint32_t info;
	int error, buflen, mlen;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * Once an MCU timeout occurs, the firmware is likely wedged.
	 * Block all further MCU commands to prevent cascading timeouts
	 * and eventual USB disconnect.  Matches Linux mcu_timeout flag.
	 */
	if (sc->sc_mcu_timeout) {
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
		    "MCU cmd 0x%02x blocked (previous timeout)\n", cmd);
		return (EIO);
	}

	sc->mcu_seq = (sc->mcu_seq + 1) & 0xf;
	if (sc->mcu_seq == 0)
		sc->mcu_seq = 1;

	/*
	 * MCU commands are sent via the bulk inband command endpoint (0x08),
	 * same as firmware.  Buffer layout (matches Linux mt76x02u_skb_dma_info):
	 *   |  4B TXINFO  |  command data  |  zero pad to 4B  |  4B zero  |
	 *
	 * TXINFO encodes: length | seq | cmd | port | MSG_TYPE_CMD
	 */
	mlen = roundup(len, 4);
	info = htole32(
	    ((uint32_t)mlen & 0xffff) |
	    ((uint32_t)sc->mcu_seq << 16) |
	    ((uint32_t)cmd << 20) |
	    ((uint32_t)MT76X2_CPU_TX_PORT << 27) |
	    MT76X2_MCU_MSG_TYPE_CMD);

	buflen = sizeof(info) + mlen + 4;
	if (buflen > MT76X2_FW_MAX_PAYLOAD)
		return (EINVAL);

	if (sc->fw_buf == NULL || sc->fw_bufsz < MT76X2_FW_MAX_PAYLOAD) {
		if (sc->fw_buf != NULL)
			free(sc->fw_buf, M_USBDEV);
		sc->fw_buf = malloc(MT76X2_FW_MAX_PAYLOAD, M_USBDEV,
		    M_NOWAIT | M_ZERO);
		if (sc->fw_buf == NULL) {
			sc->fw_bufsz = 0;
			return (ENOMEM);
		}
		sc->fw_bufsz = MT76X2_FW_MAX_PAYLOAD;
	}

	memset(sc->fw_buf, 0, buflen);
	memcpy(sc->fw_buf, &info, sizeof(info));
	if (data != NULL && len > 0)
		memcpy(sc->fw_buf + sizeof(info), data, len);

	sc->fw_buflen = buflen;
	sc->fw_chunk_done = 0;
	sc->fw_error = 0;

	usbd_transfer_start(sc->sc_xfer[MT76X2_BULK_FW]);

	error = msleep(&sc->fw_chunk_done, &sc->sc_mtx, 0,
	    "mt76mcu", 2 * hz);

	if (error == EWOULDBLOCK) {
		usbd_transfer_stop(sc->sc_xfer[MT76X2_BULK_FW]);
		sc->sc_mcu_timeout = 1;
		device_printf(sc->sc_dev,
		    "MCU cmd 0x%02x timeout (blocking further cmds)\n",
		    cmd);
		return (ETIMEDOUT);
	}
	if (sc->fw_error != 0) {
		device_printf(sc->sc_dev,
		    "MCU cmd 0x%02x bulk error: %d\n", cmd, sc->fw_error);
		return (sc->fw_error);
	}

	/*
	 * Wait for firmware to process the command and send a response.
	 * Linux mt76x02u_mcu_wait_resp() does a synchronous bulk IN read
	 * with 300ms timeout, up to 5 retries.  We use msleep on
	 * mcu_resp_done which the MCU RX callback signals.
	 *
	 * This ensures each response is consumed before the next command
	 * is sent, preventing firmware response buffer overflow that
	 * causes device crashes during scanning.
	 */
	if (sc->sc_xfer[MT76X2_BULK_MCU_RX] != NULL) {
		sc->mcu_resp_done = 0;
		sc->mcu_resp_retries = 0;
		usbd_transfer_start(sc->sc_xfer[MT76X2_BULK_MCU_RX]);
		error = msleep(&sc->mcu_resp_done, &sc->sc_mtx, 0,
		    "mt76rsp", hz);	/* 1 second timeout */
		if (error == EWOULDBLOCK) {
			/*
			 * Cancel the pending IN transfer so the next
			 * command can resubmit a fresh one.
			 */
			usbd_transfer_stop(sc->sc_xfer[MT76X2_BULK_MCU_RX]);
			MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
			    "MCU cmd 0x%02x resp timeout\n", cmd);
			/* Not fatal — response may have been lost */
		}
	} else {
		/* No MCU RX endpoint — fall back to fixed delay */
		MT76X2_UNLOCK(sc);
		pause("mt76mcu", hz / 50);	/* 20 ms */
		MT76X2_LOCK(sc);
	}

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
	    "MCU cmd 0x%02x seq %d len %d OK\n",
	    cmd, sc->mcu_seq, len);

	return (0);
}

/*
 * Send MCU command without waiting for response — matches Linux behavior
 * for FUN_SET(Q_SELECT) and RADIO_ON which use wait=false (seq=0).
 * With seq=0, firmware processes the command but does not send a response.
 */
static int
mt76x2_mcu_cmd_nowait(struct mt76x2_softc *sc, int cmd, void *data, int len)
{
	uint32_t info;
	int error, buflen, mlen;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (sc->sc_mcu_timeout)
		return (EIO);

	/*
	 * seq=0 tells firmware not to send a response.
	 * Matches Linux __mt76x02u_mcu_send_msg() when wait=false.
	 */
	mlen = roundup(len, 4);
	info = htole32(
	    ((uint32_t)mlen & 0xffff) |
	    (0 << 16) |			/* seq=0: no response */
	    ((uint32_t)cmd << 20) |
	    ((uint32_t)MT76X2_CPU_TX_PORT << 27) |
	    MT76X2_MCU_MSG_TYPE_CMD);

	buflen = sizeof(info) + mlen + 4;
	if (buflen > MT76X2_FW_MAX_PAYLOAD)
		return (EINVAL);

	if (sc->fw_buf == NULL || sc->fw_bufsz < MT76X2_FW_MAX_PAYLOAD) {
		if (sc->fw_buf != NULL)
			free(sc->fw_buf, M_USBDEV);
		sc->fw_buf = malloc(MT76X2_FW_MAX_PAYLOAD, M_USBDEV,
		    M_NOWAIT | M_ZERO);
		if (sc->fw_buf == NULL) {
			sc->fw_bufsz = 0;
			return (ENOMEM);
		}
		sc->fw_bufsz = MT76X2_FW_MAX_PAYLOAD;
	}

	memset(sc->fw_buf, 0, buflen);
	memcpy(sc->fw_buf, &info, sizeof(info));
	if (data != NULL && len > 0)
		memcpy(sc->fw_buf + sizeof(info), data, len);

	sc->fw_buflen = buflen;
	sc->fw_chunk_done = 0;
	sc->fw_error = 0;

	usbd_transfer_start(sc->sc_xfer[MT76X2_BULK_FW]);

	/* Wait only for USB bulk OUT to complete, not for MCU response */
	error = msleep(&sc->fw_chunk_done, &sc->sc_mtx, 0,
	    "mt76nw", 2 * hz);

	if (error == EWOULDBLOCK) {
		usbd_transfer_stop(sc->sc_xfer[MT76X2_BULK_FW]);
		sc->sc_mcu_timeout = 1;
		device_printf(sc->sc_dev,
		    "MCU cmd_nowait 0x%02x bulk timeout\n", cmd);
		return (ETIMEDOUT);
	}
	if (sc->fw_error != 0)
		return (sc->fw_error);

	/* Brief pause to let firmware process the command */
	MT76X2_UNLOCK(sc);
	pause("mt76nw", hz / 50);	/* 20 ms */
	MT76X2_LOCK(sc);

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
	    "MCU cmd_nowait 0x%02x len %d OK\n", cmd, len);

	return (0);
}

/*
 * Read multiple registers in one MCU bulk transfer using CMD_RANDOM_READ.
 * Replaces N individual USB control reads with one bulk OUT + one bulk IN.
 *
 * Register addresses use WLAN memory map (0x410000 + offset), matching
 * Linux mt76x02u_mcu_rd_rp() with base = MT_MCU_MEMMAP_WLAN.
 *
 * Maximum 24 registers per call (MT_INBAND_PACKET_MAX_LEN / 8).
 * Request:  [4B TXINFO][n × (4B addr_le32 + 4B zero)]
 * Response: [4B FCE][n × (4B addr_le32 + 4B value_le32)]
 */
#define MT76X2_MCU_MEMMAP_WLAN	0x410000
#define MT76X2_MCU_RANDOM_READ_MAX 24

static int
mt76x2_mcu_random_read(struct mt76x2_softc *sc, const uint32_t *regs,
    uint32_t *vals, int n)
{
	uint32_t buf[MT76X2_MCU_RANDOM_READ_MAX * 2];
	uint32_t *resp;
	int error, i, resp_len;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (n <= 0 || n > MT76X2_MCU_RANDOM_READ_MAX)
		return (EINVAL);

	/* Build request: n × (absolute_addr, 0) pairs */
	for (i = 0; i < n; i++) {
		buf[i * 2] = htole32(MT76X2_MCU_MEMMAP_WLAN + regs[i]);
		buf[i * 2 + 1] = 0;
	}

	sc->mcu_resp_len = 0;
	error = mt76x2_mcu_cmd(sc, MT76X2_MCU_CMD_RANDOM_READ,
	    buf, n * 8);
	if (error != 0)
		return (error);

	/*
	 * Parse response: 4-byte FCE header, then n × (addr, value) pairs.
	 * Matches Linux mt76x02u_multiple_mcu_reads().
	 */
	resp_len = sc->mcu_resp_len;
	if (resp_len < (int)(4 + n * 8)) {
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
		    "MCU random_read: short response %d (need %d)\n",
		    resp_len, 4 + n * 8);
		return (EIO);
	}

	resp = (uint32_t *)(sc->mcu_resp_buf + 4);
	for (i = 0; i < n; i++)
		vals[i] = le32toh(resp[i * 2 + 1]);

	return (0);
}

/*
 * Send CMD_LOAD_CR to make the MCU firmware load default calibration
 * registers for RF and BBP.  Without this, the radio cannot receive.
 * Matches Linux mt76x2_mcu_load_cr().
 *
 * type: 1 = RF/BBP CR, 2 = other
 */
static int
mt76x2_mcu_load_cr(struct mt76x2_softc *sc, uint8_t type, uint8_t temp_level,
    uint8_t channel)
{
	struct {
		uint8_t		cr_mode;
		uint8_t		temp;
		uint8_t		ch;
		uint8_t		pad0;
		uint32_t	cfg;
	} __packed msg;
	uint16_t nic0, nic1;
	uint32_t val;

	memset(&msg, 0, sizeof(msg));
	msg.cr_mode = type;
	msg.temp = temp_level;
	msg.ch = channel;

	/* Build cfg from EEPROM NIC_CONF0 and NIC_CONF1 */
	nic0 = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF0]);
	nic1 = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF1]);
	val = (1U << 31) | ((nic0 >> 8) & 0xff) | ((nic1 & 0xff) << 8);
	msg.cfg = htole32(val);

	return mt76x2_mcu_cmd(sc, MT76X2_MCU_CMD_LOAD_CR, &msg, sizeof(msg));
}

/*
 * Configure BBP AGC for the correct number of RX chains.
 * Matches Linux mt76x02_phy_set_rxpath().
 * BBP AGC reg 0: BIT(4) always cleared, BIT(3) set only for single
 * chain (chainmask & 0xf == 2, meaning only chain 1 active).
 * For dual chain (nrxchains == 2), BIT(3) must be CLEAR.
 */
static void
mt76x2_phy_set_rxpath(struct mt76x2_softc *sc)
{
	uint32_t val;

	mt76x2_read(sc, MT76X2_BBP(AGC, 0), &val);
	val &= ~(1U << 4);
	if (sc->nrxchains == 1)
		val |= (1U << 3);	/* single RX chain */
	else
		val &= ~(1U << 3);	/* dual RX chain */
	mt76x2_write(sc, MT76X2_BBP(AGC, 0), val);
}

/*
 * Configure BBP TX DAC for the correct number of TX chains.
 * Matches Linux mt76x02_phy_set_txdac().
 * BBP TXBE reg 5.
 */
static void
mt76x2_phy_set_txdac(struct mt76x2_softc *sc)
{
	uint32_t val;

	mt76x2_read(sc, MT76X2_BBP(TXBE, 5), &val);
	if (sc->ntxchains == 2)
		val |= 0x3;
	else
		val &= ~0x3;
	mt76x2_write(sc, MT76X2_BBP(TXBE, 5), val);
}

/*
 * mt76x2_phy_set_antenna() removed.
 * Was writing bits [6:5] of AGC(0) which Linux mt76 NEVER touches.
 * Linux USB driver does not call any antenna config function.
 * The AGC RX chain config is handled entirely by phy_set_rxpath()
 * which correctly sets/clears BIT(3) and BIT(4) of AGC(0).
 */

/*
 * Select 2G or 5G band in TX_BAND_CFG.
 * Matches Linux mt76x02_phy_set_band().
 */
static void
mt76x2_phy_set_band(struct mt76x2_softc *sc, int is_5ghz, int primary_upper)
{
	uint32_t val;

	mt76x2_read(sc, MT76X2_TX_BAND_CFG, &val);
	val &= ~(MT76X2_TX_BAND_CFG_2G | MT76X2_TX_BAND_CFG_5G |
	    MT76X2_TX_BAND_CFG_UPPER_40M);
	if (is_5ghz)
		val |= MT76X2_TX_BAND_CFG_5G;
	else
		val |= MT76X2_TX_BAND_CFG_2G;
	if (primary_upper)
		val |= MT76X2_TX_BAND_CFG_UPPER_40M;
	mt76x2_write(sc, MT76X2_TX_BAND_CFG, val);
}

/*
 * Configure BBP bandwidth registers.
 * Matches Linux mt76x02_phy_set_bw().
 */
static void
mt76x2_phy_set_bw(struct mt76x2_softc *sc, struct ieee80211_channel *c,
    int ch_group_index)
{
	uint32_t val;
	int core_val, agc_val;

	/* Determine bandwidth values matching Linux mt76x02_phy_set_bw() */
	if (IEEE80211_IS_CHAN_VHT80(c)) {
		core_val = 3;	/* 80MHz */
		agc_val = 7;
	} else if (IEEE80211_IS_CHAN_HT40(c) || IEEE80211_IS_CHAN_VHT40(c)) {
		core_val = 2;	/* 40MHz */
		agc_val = 3;
	} else {
		core_val = 0;	/* 20MHz */
		agc_val = 1;
	}

	/* BBP CORE reg 1: bandwidth field at bits [4:3] (GENMASK(4,3)) */
	mt76x2_read(sc, MT76X2_BBP(CORE, 1), &val);
	val &= ~(0x3 << 3);
	val |= (core_val << 3);
	mt76x2_write(sc, MT76X2_BBP(CORE, 1), val);

	/* BBP AGC reg 0: AGC bandwidth at bits [14:12] */
	mt76x2_read(sc, MT76X2_BBP(AGC, 0), &val);
	val &= ~(0x7 << 12);
	val |= (agc_val << 12);
	/* AGC reg 0: control channel at bits [9:8] */
	val &= ~(0x3 << 8);
	val |= ((ch_group_index & 0x3) << 8);
	mt76x2_write(sc, MT76X2_BBP(AGC, 0), val);

	/* BBP TXBE reg 0: control channel at bits [1:0] */
	mt76x2_read(sc, MT76X2_BBP(TXBE, 0), &val);
	val &= ~0x3;
	val |= (ch_group_index & 0x3);
	mt76x2_write(sc, MT76X2_BBP(TXBE, 0), val);
}

/*
 * Read per-channel RX high gain from EEPROM.
 * Matches Linux mt76x2_read_rx_gain() from eeprom.c.
 * Unpacks two 4-bit per-chain gain values into sc->high_gain[].
 */
static void
mt76x2_read_rx_gain(struct mt76x2_softc *sc, int channel, int is_5ghz)
{
	uint16_t val;
	uint8_t gain_byte;

	if (!is_5ghz) {
		/* 2G: high byte of 16-bit value at 0xf8 */
		val = le16dec(&sc->eeprom[MT76X2_EE_RF_2G_RX_HIGH_GAIN]);
		gain_byte = (val >> 8) & 0xff;
	} else {
		/* 5G: map channel to gain group */
		if (channel <= 48) {
			val = le16dec(&sc->eeprom[
			    MT76X2_EE_RF_5G_GRP01_RX_HIGH_GAIN]);
			gain_byte = val & 0xff;
		} else if (channel <= 64) {
			val = le16dec(&sc->eeprom[
			    MT76X2_EE_RF_5G_GRP01_RX_HIGH_GAIN]);
			gain_byte = (val >> 8) & 0xff;
		} else if (channel <= 114) {
			val = le16dec(&sc->eeprom[
			    MT76X2_EE_RF_5G_GRP23_RX_HIGH_GAIN]);
			gain_byte = val & 0xff;
		} else if (channel <= 144) {
			val = le16dec(&sc->eeprom[
			    MT76X2_EE_RF_5G_GRP23_RX_HIGH_GAIN]);
			gain_byte = (val >> 8) & 0xff;
		} else if (channel <= 165) {
			val = le16dec(&sc->eeprom[
			    MT76X2_EE_RF_5G_GRP45_RX_HIGH_GAIN]);
			gain_byte = val & 0xff;
		} else {
			val = le16dec(&sc->eeprom[
			    MT76X2_EE_RF_5G_GRP45_RX_HIGH_GAIN]);
			gain_byte = (val >> 8) & 0xff;
		}
	}

	/* Unpack: 2 x 4-bit sign-extended values per chain */
	if (gain_byte == 0 || gain_byte == 0xff) {
		sc->high_gain[0] = 0;
		sc->high_gain[1] = 0;
	} else {
		/* Sign extend each 4-bit nibble */
		sc->high_gain[0] = (int8_t)((gain_byte & 0xf) << 4) >> 4;
		sc->high_gain[1] = (int8_t)((gain_byte >> 4) << 4) >> 4;
	}

	/*
	 * Compute per-channel LNA gain for RSSI calibration.
	 * Matches Linux mt76x2_read_rx_gain() lna_gain logic:
	 * - Select sub-band LNA from EEPROM (2G or 5G group)
	 * - Only apply if internal LNA (check NIC_CONF_1 ext LNA bits)
	 * - Sign-extend: BIT(7)=sign (1=positive), [6:0]=magnitude
	 */
	{
		uint16_t nic_conf1;
		uint8_t lna;
		int use_lna;

		nic_conf1 = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF1]);
		if (!is_5ghz)
			use_lna = !(nic_conf1 &
			    MT76X2_EE_NIC_CONF1_LNA_EXT_2G);
		else
			use_lna = !(nic_conf1 &
			    MT76X2_EE_NIC_CONF1_LNA_EXT_5G);

		lna = 0;
		if (use_lna) {
			if (!is_5ghz)
				lna = sc->lna_gain_2g;
			else if (channel <= 64)
				lna = sc->lna_gain_5g[0];
			else if (channel <= 128)
				lna = sc->lna_gain_5g[1];
			else
				lna = sc->lna_gain_5g[2];
			if (lna == 0xff)
				lna = 0;
		}

		/* Sign-extend: BIT(7)=sign(1=pos), [6:0]=magnitude */
		if (lna & 0x80)
			sc->lna_gain = (int8_t)(lna & 0x7f);
		else
			sc->lna_gain = -(int8_t)(lna & 0x7f);
	}
}

/*
 * Apply per-channel RX gain adjustment to AGC registers.
 * Matches Linux mt76x2_apply_gain_adj() from phy.c.
 *
 * Adjusts BBP AGC registers:
 *   AGC[4]/[5] — LNA_HIGH_GAIN field (bits [21:16]): subtract high_gain/2
 *   AGC[8]/[9] — AGC_GAIN field (bits [14:8]): add high_gain
 */
static void
mt76x2_apply_gain_adj(struct mt76x2_softc *sc)
{
	uint32_t val;
	int8_t gain;
	int i;

	/* Chain 0 = AGC regs 4/8, Chain 1 = AGC regs 5/9 */
	for (i = 0; i < 2; i++) {
		/* Adjust LNA HIGH GAIN: AGC[4+i] bits [21:16] */
		mt76x2_read(sc, MT76X2_BBP(AGC, 4 + i), &val);
		gain = (int8_t)((val >> 16) & 0x3f);
		gain -= sc->high_gain[i] / 2;
		val &= ~(0x3fU << 16);
		val |= ((uint32_t)gain & 0x3f) << 16;
		mt76x2_write(sc, MT76X2_BBP(AGC, 4 + i), val);

		/* Adjust AGC GAIN: AGC[8+i] bits [14:8] */
		mt76x2_read(sc, MT76X2_BBP(AGC, 8 + i), &val);
		gain = (int8_t)((val >> 8) & 0x7f);
		gain += sc->high_gain[i];
		val &= ~(0x7fU << 8);
		val |= ((uint32_t)gain & 0x7f) << 8;
		mt76x2_write(sc, MT76X2_BBP(AGC, 8 + i), val);
	}
}

/*
 * Snapshot initial AGC gain values after channel calibration.
 * Matches Linux mt76x02_init_agc_gain() from mt76x02_phy.c.
 */
static void
mt76x2_init_agc_gain(struct mt76x2_softc *sc)
{
	uint32_t val;

	mt76x2_read(sc, MT76X2_BBP(AGC, 8), &val);
	sc->agc_gain_init[0] = (val >> 8) & 0x7f;

	mt76x2_read(sc, MT76X2_BBP(AGC, 9), &val);
	sc->agc_gain_init[1] = (val >> 8) & 0x7f;
}

/*
 * Configure PA mode and TX power correction registers per band.
 * Matches Linux mt76x2_phy_set_txpower_regs() from phy.c.
 */
static void
mt76x2_phy_set_txpower_regs(struct mt76x2_softc *sc, int is_5ghz)
{
	uint32_t pa_mode[2];
	uint16_t nic0;
	int ext_pa;

	nic0 = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF0]);
	if (is_5ghz)
		ext_pa = !(nic0 & (1U << 9));
	else
		ext_pa = !(nic0 & (1U << 8));

	if (!is_5ghz) {
		pa_mode[0] = 0x010055ff;
		pa_mode[1] = 0x00550055;

		mt76x2_write(sc, MT76X2_TX_ALC_CFG_2, 0x35160a00);
		mt76x2_write(sc, MT76X2_TX_ALC_CFG_3, 0x35160a06);

		if (ext_pa) {
			mt76x2_write(sc, MT76X2_RF_PA_MODE_ADJ0, 0x0000ec00);
			mt76x2_write(sc, MT76X2_RF_PA_MODE_ADJ1, 0x0000ec00);
		} else {
			mt76x2_write(sc, MT76X2_RF_PA_MODE_ADJ0, 0xf4000200);
			mt76x2_write(sc, MT76X2_RF_PA_MODE_ADJ1, 0xfa000200);
		}
	} else {
		pa_mode[0] = 0x0000ffff;
		pa_mode[1] = 0x00ff00ff;

		if (ext_pa) {
			mt76x2_write(sc, MT76X2_TX_ALC_CFG_2, 0x2f0f0400);
			mt76x2_write(sc, MT76X2_TX_ALC_CFG_3, 0x2f0f0476);
		} else {
			mt76x2_write(sc, MT76X2_TX_ALC_CFG_2, 0x1b0f0400);
			mt76x2_write(sc, MT76X2_TX_ALC_CFG_3, 0x1b0f0476);
		}

		if (ext_pa) {
			mt76x2_write(sc, MT76X2_RF_PA_MODE_ADJ0, 0x04000000);
			mt76x2_write(sc, MT76X2_RF_PA_MODE_ADJ1, 0x04000000);
		} else {
			mt76x2_write(sc, MT76X2_RF_PA_MODE_ADJ0, 0);
			mt76x2_write(sc, MT76X2_RF_PA_MODE_ADJ1, 0);
		}
	}

	mt76x2_write(sc, MT76X2_BB_PA_MODE_CFG0, pa_mode[0]);
	mt76x2_write(sc, MT76X2_BB_PA_MODE_CFG1, pa_mode[1]);
	mt76x2_write(sc, MT76X2_RF_PA_MODE_CFG0, pa_mode[0]);
	mt76x2_write(sc, MT76X2_RF_PA_MODE_CFG1, pa_mode[1]);

	if (ext_pa) {
		uint32_t val;

		if (!is_5ghz)
			val = 0x3c3c023c;
		else
			val = 0x363c023c;

		mt76x2_write(sc, MT76X2_TX0_RF_GAIN_CORR, val);
		mt76x2_write(sc, MT76X2_TX1_RF_GAIN_CORR, val);
		mt76x2_write(sc, MT76X2_TX_ALC_CFG_4, 0x00001818);
	} else {
		if (!is_5ghz) {
			mt76x2_write(sc, MT76X2_TX0_RF_GAIN_CORR, 0x0f3c3c3c);
			mt76x2_write(sc, MT76X2_TX1_RF_GAIN_CORR, 0x0f3c3c3c);
			mt76x2_write(sc, MT76X2_TX_ALC_CFG_4, 0x00000606);
		} else {
			mt76x2_write(sc, MT76X2_TX0_RF_GAIN_CORR, 0x383c023c);
			mt76x2_write(sc, MT76X2_TX1_RF_GAIN_CORR, 0x24282e28);
			mt76x2_write(sc, MT76X2_TX_ALC_CFG_4, 0);
		}
	}
}

/*
 * Configure TX delay (TX_SW_CFG0/1) per band and bandwidth.
 * Matches Linux mt76x2_configure_tx_delay() from phy.c.
 * Also sets OFDM SIFS to 15us in XIFS_TIME_CFG.
 */
static void
mt76x2_configure_tx_delay(struct mt76x2_softc *sc, int is_5ghz, int bw)
{
	uint16_t nic0;
	int ext_pa;
	uint32_t cfg0, cfg1, val;

	/*
	 * Check external PA from NIC_CONF0:
	 *   Bit 8 = PA_INT_2G (internal PA for 2G if set)
	 *   Bit 9 = PA_INT_5G (internal PA for 5G if set)
	 */
	nic0 = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF0]);
	if (is_5ghz)
		ext_pa = !(nic0 & (1U << 9));
	else
		ext_pa = !(nic0 & (1U << 8));

	if (ext_pa) {
		cfg0 = bw ? 0x000b0c01 : 0x00101101;
		cfg1 = 0x00011414;
	} else {
		cfg0 = bw ? 0x000b0b01 : 0x00101001;
		cfg1 = 0x00021414;
	}
	mt76x2_write(sc, MT76X2_TX_SW_CFG0, cfg0);
	mt76x2_write(sc, MT76X2_TX_SW_CFG1, cfg1);

	/* Set OFDM SIFS to 15us: XIFS_TIME_CFG bits [15:8] */
	mt76x2_read(sc, MT76X2_XIFS_TIME_CFG, &val);
	val &= ~0x0000ff00U;
	val |= (15U << 8);
	mt76x2_write(sc, MT76X2_XIFS_TIME_CFG, val);
}

/*
 * ============================================================
 * TX Power Control
 *
 * Ported from Linux mt76x2/phy.c + mt76x2/eeprom.c + mt76x02_phy.c.
 * Sets per-channel, per-rate, per-chain TX power from EEPROM data.
 * ============================================================
 */

static int
mt76x2_sign_extend(uint32_t val, unsigned int size)
{
	int sign = val & (1U << (size - 1));

	val &= (1U << (size - 1)) - 1;
	return sign ? (int)val : -(int)val;
}

static int
mt76x2_sign_extend_optional(uint32_t val, unsigned int size)
{
	if (!(val & (1U << size)))
		return 0;
	return mt76x2_sign_extend(val, size);
}

static int8_t
mt76x2_rate_power_val(uint8_t val)
{
	if (val == 0 || val == 0xff)
		return 0;
	return (int8_t)mt76x2_sign_extend_optional(val, 7);
}

static enum mt76x2_cal_channel_group
mt76x2_get_cal_channel_group(int channel)
{
	if (channel >= 184 && channel <= 196)
		return MT76X2_CH_5G_JAPAN;
	if (channel <= 48)
		return MT76X2_CH_5G_UNII_1;
	if (channel <= 64)
		return MT76X2_CH_5G_UNII_2;
	if (channel <= 114)
		return MT76X2_CH_5G_UNII_2E_1;
	if (channel <= 144)
		return MT76X2_CH_5G_UNII_2E_2;
	return MT76X2_CH_5G_UNII_3;
}

/* Read per-channel 2G power info from EEPROM (Linux mt76x2_get_power_info_2g) */
static void
mt76x2_get_power_info_2g(struct mt76x2_softc *sc,
    struct mt76x2_tx_power_info *t, int channel, int chain, int offset)
{
	uint8_t data[6];
	uint16_t val;
	int delta_idx;

	if (channel < 6)
		delta_idx = 3;
	else if (channel < 11)
		delta_idx = 4;
	else
		delta_idx = 5;

	memcpy(data, &sc->eeprom[offset], sizeof(data));

	t->chain[chain].tssi_slope = data[0];
	t->chain[chain].tssi_offset = data[1];
	t->chain[chain].target_power = data[2];
	t->chain[chain].delta =
	    mt76x2_sign_extend_optional(data[delta_idx], 7);

	val = le16dec(&sc->eeprom[MT76X2_EE_RF_2G_TSSI_OFF_TXPOWER]);
	t->target_power = val >> 8;
}

/* Read per-channel 5G power info from EEPROM (Linux mt76x2_get_power_info_5g) */
static void
mt76x2_get_power_info_5g(struct mt76x2_softc *sc,
    struct mt76x2_tx_power_info *t, int channel, int chain, int offset)
{
	enum mt76x2_cal_channel_group group;
	uint8_t data[5];
	uint16_t val;
	int delta_idx;

	group = mt76x2_get_cal_channel_group(channel);
	offset += group * MT76X2_TX_POWER_GROUP_SIZE_5G;

	if (channel >= 192)
		delta_idx = 4;
	else if (channel >= 184)
		delta_idx = 3;
	else if (channel < 44)
		delta_idx = 3;
	else if (channel < 52)
		delta_idx = 4;
	else if (channel < 58)
		delta_idx = 3;
	else if (channel < 98)
		delta_idx = 4;
	else if (channel < 106)
		delta_idx = 3;
	else if (channel < 116)
		delta_idx = 4;
	else if (channel < 130)
		delta_idx = 3;
	else if (channel < 149)
		delta_idx = 4;
	else if (channel < 157)
		delta_idx = 3;
	else
		delta_idx = 4;

	memcpy(data, &sc->eeprom[offset], sizeof(data));

	t->chain[chain].tssi_slope = data[0];
	t->chain[chain].tssi_offset = data[1];
	t->chain[chain].target_power = data[2];
	t->chain[chain].delta =
	    mt76x2_sign_extend_optional(data[delta_idx], 7);

	val = le16dec(&sc->eeprom[MT76X2_EE_RF_2G_RX_HIGH_GAIN]);
	t->target_power = val & 0xff;
}

/* Read per-channel TX power info (Linux mt76x2_get_power_info) */
static void
mt76x2_get_power_info(struct mt76x2_softc *sc,
    struct mt76x2_tx_power_info *t, struct ieee80211_channel *c)
{
	int is_5ghz = IEEE80211_IS_CHAN_5GHZ(c);
	int channel = ieee80211_chan2ieee(&sc->sc_ic, c);
	uint16_t bw40, bw80;

	memset(t, 0, sizeof(*t));

	bw40 = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_DELTA_BW40]);
	bw80 = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_DELTA_BW80]);

	if (is_5ghz) {
		bw40 >>= 8;
		mt76x2_get_power_info_5g(sc, t, channel, 0,
		    MT76X2_EE_TX_POWER_0_START_5G);
		mt76x2_get_power_info_5g(sc, t, channel, 1,
		    MT76X2_EE_TX_POWER_1_START_5G);
	} else {
		mt76x2_get_power_info_2g(sc, t, channel, 0,
		    MT76X2_EE_TX_POWER_0_START_2G);
		mt76x2_get_power_info_2g(sc, t, channel, 1,
		    MT76X2_EE_TX_POWER_1_START_2G);
	}

	if (t->target_power == 0 || t->target_power == 0xff)
		t->target_power = t->chain[0].target_power;

	t->delta_bw40 = mt76x2_rate_power_val(bw40);
	t->delta_bw80 = mt76x2_rate_power_val(bw80);
}

/* Read per-rate power offsets from EEPROM (Linux mt76x2_get_rate_power) */
static void
mt76x2_get_rate_power(struct mt76x2_softc *sc,
    struct mt76x2_rate_power *t, struct ieee80211_channel *c)
{
	int is_5ghz = IEEE80211_IS_CHAN_5GHZ(c);
	uint16_t val;

	memset(t, 0, sizeof(*t));

	val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_CCK]);
	t->cck[0] = t->cck[1] = mt76x2_rate_power_val(val);
	t->cck[2] = t->cck[3] = mt76x2_rate_power_val(val >> 8);

	if (is_5ghz)
		val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_OFDM_5G_6M]);
	else
		val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_OFDM_2G_6M]);
	t->ofdm[0] = t->ofdm[1] = mt76x2_rate_power_val(val);
	t->ofdm[2] = t->ofdm[3] = mt76x2_rate_power_val(val >> 8);

	if (is_5ghz)
		val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_OFDM_5G_24M]);
	else
		val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_OFDM_2G_24M]);
	t->ofdm[4] = t->ofdm[5] = mt76x2_rate_power_val(val);
	t->ofdm[6] = t->ofdm[7] = mt76x2_rate_power_val(val >> 8);

	val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_HT_MCS0]);
	t->ht[0] = t->ht[1] = mt76x2_rate_power_val(val);
	t->ht[2] = t->ht[3] = mt76x2_rate_power_val(val >> 8);

	val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_HT_MCS4]);
	t->ht[4] = t->ht[5] = mt76x2_rate_power_val(val);
	t->ht[6] = t->ht[7] = mt76x2_rate_power_val(val >> 8);

	val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_HT_MCS8]);
	t->ht[8] = t->ht[9] = mt76x2_rate_power_val(val);
	t->ht[10] = t->ht[11] = mt76x2_rate_power_val(val >> 8);

	val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_HT_MCS12]);
	t->ht[12] = t->ht[13] = mt76x2_rate_power_val(val);
	t->ht[14] = t->ht[15] = mt76x2_rate_power_val(val >> 8);

	val = le16dec(&sc->eeprom[MT76X2_EE_TX_POWER_VHT_MCS8]);
	if (!is_5ghz)
		val >>= 8;
	t->vht[0] = t->vht[1] = mt76x2_rate_power_val(val >> 8);
}

/* Pack 4 rate power values into register format (Linux mt76x02_tx_power_mask) */
static uint32_t
mt76x2_tx_power_mask(int8_t v1, int8_t v2, int8_t v3, int8_t v4)
{
	uint32_t val = 0;

	val |= (v1 & 0x3f) << 0;
	val |= (v2 & 0x3f) << 8;
	val |= (v3 & 0x3f) << 16;
	val |= (v4 & 0x3f) << 24;
	return val;
}

static int
mt76x2_get_min_rate_power(struct mt76x2_rate_power *r)
{
	int i;
	int8_t ret = 0;

	for (i = 0; i < (int)sizeof(r->all); i++) {
		if (!r->all[i])
			continue;
		if (ret)
			ret = MIN(ret, r->all[i]);
		else
			ret = r->all[i];
	}

	return ret;
}

static int
mt76x2_get_max_rate_power(struct mt76x2_rate_power *r)
{
	int8_t ret = 0;
	int i;

	for (i = 0; i < (int)sizeof(r->all); i++)
		ret = MAX(ret, r->all[i]);

	return ret;
}

/*
 * Set TX power registers — per-chain init power and per-rate offsets.
 * Matches Linux mt76x02_phy_set_txpower() from mt76x02_phy.c and
 * mt76x2_phy_set_txpower() from mt76x2/phy.c.
 */
static void
mt76x2_phy_set_txpower(struct mt76x2_softc *sc, struct ieee80211_channel *c)
{
	struct mt76x2_tx_power_info txp;
	struct mt76x2_rate_power t;
	int txp_0, txp_1, delta = 0;
	int base_power, gain;
	uint32_t val;

	mt76x2_get_power_info(sc, &txp, c);

	/* BW delta for wider channels */
	if (IEEE80211_IS_CHAN_VHT80(c))
		delta = txp.delta_bw80;
	else if (IEEE80211_IS_CHAN_HT40(c))
		delta = txp.delta_bw40;

	mt76x2_get_rate_power(sc, &t, c);

	/* Add target power + BW delta to all rates */
	{
		int i, offset = txp.target_power + delta;

		for (i = 0; i < (int)sizeof(t.all); i++)
			t.all[i] += offset;
	}

	/* Limit to regulatory */
	if (sc->txpower_conf > 0) {
		int i;

		for (i = 0; i < (int)sizeof(t.all); i++)
			if (t.all[i] > sc->txpower_conf)
				t.all[i] = sc->txpower_conf;
	}
	sc->txpower = mt76x2_get_max_rate_power(&t);

	base_power = mt76x2_get_min_rate_power(&t);
	delta = base_power - txp.target_power;
	txp_0 = txp.chain[0].target_power + txp.chain[0].delta + delta;
	txp_1 = txp.chain[1].target_power + txp.chain[1].delta + delta;

	gain = MIN(txp_0, txp_1);
	if (gain < 0) {
		base_power -= gain;
		txp_0 -= gain;
		txp_1 -= gain;
	} else if (gain > 0x2f) {
		base_power -= gain - 0x2f;
		txp_0 = 0x2f;
		txp_1 = 0x2f;
	}

	/* Subtract base_power so rates become deltas */
	{
		int i;

		for (i = 0; i < (int)sizeof(t.all); i++)
			t.all[i] -= base_power;
	}

	sc->target_power = txp.target_power;
	sc->target_power_delta[0] = txp_0 - txp.chain[0].target_power;
	sc->target_power_delta[1] = txp_1 - txp.chain[0].target_power;
	sc->rate_power = t;

	/* Write per-chain init power to TX_ALC_CFG_0 (RMW) */
	mt76x2_read(sc, MT76X2_TX_ALC_CFG_0, &val);
	val &= ~(MT76X2_TX_ALC_CFG_0_CH_INIT_0 |
	    MT76X2_TX_ALC_CFG_0_CH_INIT_1);
	val |= (txp_0 & 0x3f);
	val |= ((txp_1 & 0x3f) << 8);
	mt76x2_write(sc, MT76X2_TX_ALC_CFG_0, val);

	/* Write per-rate power registers */
	mt76x2_write(sc, MT76X2_TX_PWR_CFG_0,
	    mt76x2_tx_power_mask(t.cck[0], t.cck[2], t.ofdm[0], t.ofdm[2]));
	mt76x2_write(sc, MT76X2_TX_PWR_CFG_1,
	    mt76x2_tx_power_mask(t.ofdm[4], t.ofdm[6], t.ht[0], t.ht[2]));
	mt76x2_write(sc, MT76X2_TX_PWR_CFG_2,
	    mt76x2_tx_power_mask(t.ht[4], t.ht[6], t.ht[8], t.ht[10]));
	mt76x2_write(sc, MT76X2_TX_PWR_CFG_3,
	    mt76x2_tx_power_mask(t.ht[12], t.ht[14], t.ht[0], t.ht[2]));
	mt76x2_write(sc, MT76X2_TX_PWR_CFG_4,
	    mt76x2_tx_power_mask(t.ht[4], t.ht[6], 0, 0));
	mt76x2_write(sc, MT76X2_TX_PWR_CFG_7,
	    mt76x2_tx_power_mask(t.ofdm[7], t.vht[0], t.ht[7], t.vht[1]));
	mt76x2_write(sc, MT76X2_TX_PWR_CFG_8,
	    mt76x2_tx_power_mask(t.ht[14], 0, t.vht[0], t.vht[1]));
	mt76x2_write(sc, MT76X2_TX_PWR_CFG_9,
	    mt76x2_tx_power_mask(t.ht[7], 0, t.vht[0], t.vht[1]));
}

/*
 * ============================================================
 * LED control
 * ============================================================
 */

/*
 * LED control via hardware blink sequencer.  Matches Linux
 * mt76x02_led_set_config().  Uses LED pin 0, active-low.
 *
 * Note: NetGear A6210 has no LED on the PCB (confirmed by FCC teardown).
 * Other MT7612U adapters (ASUS USB-AC55, etc.) may have LEDs.
 */
static void
mt76x2_set_leds(struct mt76x2_softc *sc, uint8_t on)
{
	uint32_t val;
	uint8_t delay_on, delay_off;

	if (on) {
		delay_on = 0xff;
		delay_off = 0;
	} else {
		delay_on = 0;
		delay_off = 0xff;
	}

	val = (0xffU << 8) |
	    ((uint32_t)delay_off << 24) |
	    ((uint32_t)delay_on << 16);
	mt76x2_write(sc, MT76X2_LED_S0_BASE, val);
	mt76x2_write(sc, MT76X2_LED_S1_BASE, val);

	val = (1U << 0) | (1U << 1) | (1U << 7);  /* replay + polarity + kick */
	mt76x2_write(sc, MT76X2_LED_CTRL, val);
	mt76x2_write(sc, MT76X2_LED_CTRL, val & ~(1U << 7));  /* clear kick */
}

/*
 * Send a calibration command to the MCU firmware.
 * Matches Linux mt76x02_mcu_calibrate(dev, cal_id, val).
 */
static int
mt76x2_mcu_calibrate(struct mt76x2_softc *sc, int cal_id, uint32_t val)
{
	struct {
		uint32_t id;
		uint32_t value;
	} __packed msg;

	msg.id = htole32(cal_id);
	msg.value = htole32(val);

	return mt76x2_mcu_cmd(sc, MT76X2_MCU_CMD_CALIBRATION,
	    &msg, sizeof(msg));
}

/*
 * Perform PHY channel calibration after switching channel.
 * Matches Linux mt76x2u_phy_channel_calibrate() from usb_phy.c.
 * Without this, the receiver is deaf — the PHY cannot demodulate frames.
 */
static void
mt76x2_phy_channel_calibrate(struct mt76x2_softc *sc, int is_5ghz)
{

	/* LC calibration (5 GHz only) */
	if (is_5ghz)
		mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_LC, 0);

	/* TX LOFT calibration */
	mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_TX_LOFT, is_5ghz);

	/* TX IQ calibration */
	mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_TXIQ, is_5ghz);

	/* RX IQ calibration (FI variant for USB) */
	mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_RXIQC_FI, is_5ghz);

	/* Temperature sensor calibration */
	mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_TEMP_SENSOR, 0);

	/* TX shaping calibration */
	mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_TX_SHAPING, 0);
}

/*
 * ============================================================
 * TX setup
 * ============================================================
 */

static void
mt76x2_setup_tx_list(struct mt76x2_softc *sc,
    struct mt76x2_endpoint_queue *epq)
{
	struct mt76x2_tx_data *data;

	memset(epq, 0, sizeof(*epq));
	STAILQ_INIT(&epq->tx_qh);
	STAILQ_INIT(&epq->tx_fh);

	for (data = &epq->tx_data[0];
	    data < &epq->tx_data[MT76X2_TX_RING_COUNT]; data++) {
		data->sc = sc;
		STAILQ_INSERT_TAIL(&epq->tx_fh, data, next);
	}
	epq->tx_nfree = MT76X2_TX_RING_COUNT;
}

static void
mt76x2_unsetup_tx_list(struct mt76x2_softc *sc,
    struct mt76x2_endpoint_queue *epq)
{
	struct mt76x2_tx_data *data;

	epq->tx_nfree = 0;
	STAILQ_INIT(&epq->tx_fh);
	STAILQ_INIT(&epq->tx_qh);

	for (data = &epq->tx_data[0];
	    data < &epq->tx_data[MT76X2_TX_RING_COUNT]; data++) {
		if (data->m != NULL) {
			m_freem(data->m);
			data->m = NULL;
		}
		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}
	}
}

static void
mt76x2_set_txrts(struct mt76x2_softc *sc)
{
	mt76x2_write(sc, MT76X2_TX_RTY_CFG, 0x47f01f0f);
	/* TX_TIMEOUT_CFG is set by init table to 0x000a2290.
	 * Do not overwrite — the ACKTO field (bits 15:8) must be preserved.
	 * Linux only modifies ACKTO via mt76_rmw_field, never replaces. */
}

/*
 * Update HT/ERP protection mode registers.
 * Matches Linux mt76x02_mac_set_tx_protection().
 * Programs 6 HT protection regs + 3 VHT protection regs based on
 * current BSS state (legacy STAs present, HT operation mode).
 */
static void
mt76x2_updateprot(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint32_t prot[6], vht_prot[3];
	int i, mode, legacy_prot;
	uint16_t rts_thr;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (vap == NULL)
		return;

	/* Read current protection register values */
	for (i = 0; i < 6; i++) {
		mt76x2_read(sc, MT76X2_CCK_PROT_CFG + i * 4, &prot[i]);
		prot[i] &= ~MT76X2_PROT_CFG_CTRL;
		if (i >= 2)
			prot[i] &= ~MT76X2_PROT_CFG_RATE;
	}
	for (i = 0; i < 3; i++) {
		mt76x2_read(sc, MT76X2_TX_PROT_CFG6 + i * 4, &vht_prot[i]);
		vht_prot[i] &= ~(MT76X2_PROT_CFG_CTRL | MT76X2_PROT_CFG_RATE);
	}

	/* RTS threshold from VAP */
	rts_thr = vap->iv_rtsthreshold;

	/* Write RTS threshold to hardware */
	{
		uint32_t rts_val;
		mt76x2_read(sc, MT76X2_TX_RTS_CFG, &rts_val);
		rts_val &= ~(0xffffU << 8);
		rts_val |= ((uint32_t)rts_thr << 8);
		mt76x2_write(sc, MT76X2_TX_RTS_CFG, rts_val);
	}

	if (rts_thr != 0xffff)
		prot[0] |= MT76X2_PROT_CTRL_RTS_CTS;

	/* ERP protection (2.4 GHz legacy STA present) */
	legacy_prot = !!(ic->ic_flags & IEEE80211_F_USEPROT);
	if (legacy_prot) {
		prot[1] |= MT76X2_PROT_CTRL_CTS2SELF;
		for (i = 2; i < 6; i++)
			prot[i] |= MT76X2_PROT_RATE_CCK_11;
		for (i = 0; i < 3; i++)
			vht_prot[i] |= MT76X2_PROT_RATE_CCK_11;
	} else {
		if (rts_thr != 0xffff)
			prot[1] |= MT76X2_PROT_CTRL_RTS_CTS;
		prot[2] |= MT76X2_PROT_RATE_OFDM_24;
		prot[3] |= MT76X2_PROT_RATE_DUP_OFDM_24;
		prot[4] |= MT76X2_PROT_RATE_OFDM_24;
		prot[5] |= MT76X2_PROT_RATE_DUP_OFDM_24;
		vht_prot[0] |= MT76X2_PROT_RATE_OFDM_24;
		vht_prot[1] |= MT76X2_PROT_RATE_DUP_OFDM_24;
		vht_prot[2] |= MT76X2_PROT_RATE_SGI_OFDM_24;
	}

	/* HT protection mode from AP's HT Operation IE */
	mode = ic->ic_curhtprotmode & IEEE80211_HTINFO_OPMODE;
	switch (mode) {
	case IEEE80211_HTINFO_OPMODE_MIXED:
		/* Non-HT STAs present: protect all HT frames */
		for (i = 2; i < 6; i++)
			prot[i] |= MT76X2_PROT_CTRL_RTS_CTS;
		for (i = 0; i < 3; i++)
			vht_prot[i] |= MT76X2_PROT_CTRL_RTS_CTS;
		break;
	case IEEE80211_HTINFO_OPMODE_HT20PR:
		/* 20MHz-only protection: protect 40MHz frames */
		prot[3] |= MT76X2_PROT_CTRL_RTS_CTS;
		prot[5] |= MT76X2_PROT_CTRL_RTS_CTS;
		vht_prot[1] |= MT76X2_PROT_CTRL_RTS_CTS;
		vht_prot[2] |= MT76X2_PROT_CTRL_RTS_CTS;
		break;
	}

	/* Non-GF STAs present */
	if (ic->ic_curhtprotmode & IEEE80211_HTINFO_NONGF_PRESENT) {
		prot[4] |= MT76X2_PROT_CTRL_RTS_CTS;
		prot[5] |= MT76X2_PROT_CTRL_RTS_CTS;
	}

	/* Write back */
	for (i = 0; i < 6; i++)
		mt76x2_write(sc, MT76X2_CCK_PROT_CFG + i * 4, prot[i]);
	for (i = 0; i < 3; i++)
		mt76x2_write(sc, MT76X2_TX_PROT_CFG6 + i * 4, vht_prot[i]);
}

/*
 * Beacon update callback — called from net80211 when beacon content
 * changes (TIM update, CSA countdown, HT/ERP state change).
 * Schedules a cmdq task to re-upload the beacon template.
 * Matches if_run(4) run_update_beacon() pattern.
 */
static void
mt76x2_update_beacon(struct ieee80211vap *vap, int item)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_softc *sc = ic->ic_softc;
	uint32_t cnt;

	cnt = MT76X2_CMDQ_GET(&sc->cmdq_store);
	sc->cmdq[cnt].func = mt76x2_update_beacon_cb;
	sc->cmdq[cnt].arg0 = vap;
	ieee80211_runtask(ic, &sc->cmdq_task);
}

static void
mt76x2_update_beacon_cb(void *arg)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_softc *sc = ic->ic_softc;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (!sc->sc_running || sc->sc_detached)
		return;

	mt76x2_beacon_upload(sc, vap);

	/* CSA completion check — finalize channel switch when countdown ends */
	if (ic->ic_csa_newchan != NULL && ic->ic_csa_count == 0)
		ieee80211_csa_completeswitch(ic);
}

static void
mt76x2_abort_tsf_sync(struct mt76x2_softc *sc)
{
	uint32_t val;

	/*
	 * Beacon config init — matches Linux mt76x02_init_beacon_config():
	 * Disable beacon timer, TBTT, and beacon TX.
	 * Set sync mode. Set bypass mask to 0xffff.
	 */
	mt76x2_read(sc, MT76X2_BCN_TIME_CFG, &val);
	val &= ~(MT76X2_BCN_TIME_CFG_TIMER_EN |
	    MT76X2_BCN_TIME_CFG_TBTT_EN |
	    MT76X2_BCN_TIME_CFG_BCN_TX);
	val |= MT76X2_BCN_TIME_CFG_SYNC_MODE;
	mt76x2_write(sc, MT76X2_BCN_TIME_CFG, val);

	mt76x2_write(sc, MT76X2_BCN_BYPASS_MASK, 0xffff);
}

/*
 * Enable TSF synchronization — called when entering S_RUN.
 * Matches Linux mt76x02_init_beacon_config() + run_enable_tsf_sync().
 */
static void
mt76x2_enable_tsf_sync(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint32_t tmp;

	if (vap == NULL || vap->iv_bss == NULL)
		return;

	mt76x2_read(sc, MT76X2_BCN_TIME_CFG, &tmp);
	tmp &= ~(MT76X2_BCN_TIME_CFG_INTVAL_MASK |
	    MT76X2_BCN_TIME_CFG_SYNC_MODE |
	    MT76X2_BCN_TIME_CFG_TIMER_EN |
	    MT76X2_BCN_TIME_CFG_TBTT_EN |
	    MT76X2_BCN_TIME_CFG_BCN_TX);

	tmp |= vap->iv_bss->ni_intval * 16;
	tmp |= MT76X2_BCN_TIME_CFG_TIMER_EN | MT76X2_BCN_TIME_CFG_TBTT_EN;

	if (ic->ic_opmode == IEEE80211_M_STA)
		tmp |= (1U << MT76X2_BCN_TIME_CFG_SYNC_SHIFT);
	else if (ic->ic_opmode == IEEE80211_M_IBSS)
		tmp |= MT76X2_BCN_TIME_CFG_BCN_TX |
		    (2U << MT76X2_BCN_TIME_CFG_SYNC_SHIFT);
	else if (ic->ic_opmode == IEEE80211_M_HOSTAP)
		tmp |= MT76X2_BCN_TIME_CFG_BCN_TX |
		    (3U << MT76X2_BCN_TIME_CFG_SYNC_SHIFT);

	mt76x2_write(sc, MT76X2_BCN_TIME_CFG, tmp);
}

/*
 * Upload beacon template to hardware beacon buffer.
 * The MT7612U has a 512-byte beacon buffer at MT76X2_BCN_BASE (0xc000).
 * For HOSTAP/IBSS, we construct the beacon frame, prepend a TXWI,
 * and write it to the beacon buffer via register writes.
 *
 * Called from newstate(RUN) for HOSTAP/IBSS, and from iv_update_beacon
 * when TIM changes.  Matches Linux mt76x02_mac_set_beacon() pattern.
 */
static void
mt76x2_beacon_upload(struct mt76x2_softc *sc, struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_txwi txwi;
	struct mbuf *m;
	int len, txwi_len, total, pad;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	/* Let net80211 construct the beacon frame */
	m = ieee80211_beacon_alloc(vap->iv_bss);
	if (m == NULL)
		return;

	len = m->m_pkthdr.len;
	txwi_len = sizeof(struct mt76x2_txwi);
	total = txwi_len + len;

	/* Beacon buffer is 512 bytes */
	if (total > MT76X2_BCN_SIZE) {
		m_freem(m);
		return;
	}

	/* Build TXWI for beacon: OFDM 6Mbps on 5GHz, CCK 1Mbps on 2.4GHz */
	memset(&txwi, 0, txwi_len);
	if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))
		txwi.rate = htole16(MT76X2_TXWI_PHY_OFDM <<
		    MT76X2_TXWI_PHY_SHIFT);
	txwi.len_ctl = htole16(len);
	txwi.txstream = MT76X2_TXWI_TXSTREAM_E4;

	/* Write TXWI to beacon buffer */
	mt76x2_write_region_1(sc, MT76X2_BCN_BASE,
	    (const uint8_t *)&txwi, txwi_len);

	/* Write beacon frame body */
	mt76x2_write_region_1(sc, MT76X2_BCN_BASE + txwi_len,
	    mtod(m, const uint8_t *), len);

	/* Zero-pad to 4-byte alignment */
	pad = (4 - (total % 4)) % 4;
	if (pad > 0) {
		uint8_t zeros[4] = {0};
		mt76x2_write_region_1(sc,
		    MT76X2_BCN_BASE + total, zeros, pad);
	}

	m_freem(m);
}

/*
 * Multi-rate retry (MRR) fallback chains — matches run_enable_mrr().
 */
static void
mt76x2_enable_mrr(struct mt76x2_softc *sc)
{
#define	CCK(mcs)	(mcs)
#define	OFDM(mcs)	(1 << 3 | (mcs))
	mt76x2_write(sc, MT76X2_LG_FBK_CFG0,
	    OFDM(6) << 28 |	/* 54->48 */
	    OFDM(5) << 24 |	/* 48->36 */
	    OFDM(4) << 20 |	/* 36->24 */
	    OFDM(3) << 16 |	/* 24->18 */
	    OFDM(2) << 12 |	/* 18->12 */
	    OFDM(1) <<  8 |	/* 12-> 9 */
	    OFDM(0) <<  4 |	/*  9-> 6 */
	    OFDM(0));		/*  6-> 6 */

	mt76x2_write(sc, MT76X2_LG_FBK_CFG1,
	    CCK(2) << 12 |	/* 11->5.5 */
	    CCK(1) <<  8 |	/* 5.5-> 2 */
	    CCK(0) <<  4 |	/*   2-> 1 */
	    CCK(0));		/*   1-> 1 */
#undef OFDM
#undef CCK

	/*
	 * HT MCS fallback: MCS[n] falls back to MCS[n-1].
	 * HT_FBK_CFG0: MCS0-7 fallback, HT_FBK_CFG1: MCS8-15 fallback.
	 * Each 4-bit nibble: fallback MCS for the corresponding MCS index.
	 * Matches Linux mt76x2 init table default.
	 */
	mt76x2_write(sc, MT76X2_HT_FBK_CFG0, 0x65432100);
	mt76x2_write(sc, MT76X2_HT_FBK_CFG1, 0xedcba988);
}

/*
 * Set short preamble in auto-response config.
 */
static void
mt76x2_set_txpreamble(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;

	mt76x2_read(sc, MT76X2_AUTO_RSP_CFG, &tmp);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		tmp |= MT76X2_AUTO_RSP_CCK_SHORT;
	else
		tmp &= ~MT76X2_AUTO_RSP_CCK_SHORT;
	mt76x2_write(sc, MT76X2_AUTO_RSP_CFG, tmp);
}

/*
 * Set basic rate mask for auto-response (ACK/CTS).
 */
static void
mt76x2_set_basicrates(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	if (ic->ic_curmode == IEEE80211_MODE_11B)
		mt76x2_write(sc, MT76X2_LEGACY_BASIC_RATE, 0x003);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		mt76x2_write(sc, MT76X2_LEGACY_BASIC_RATE, 0x150);
	else	/* 11g, 11n, 11ac */
		mt76x2_write(sc, MT76X2_LEGACY_BASIC_RATE, 0x15f);
}

/*
 * ============================================================
 * Channel configuration
 * ============================================================
 */

static const uint8_t mt76x2_5ghz_chans[] = {
	36, 40, 44, 48, 52, 56, 60, 64,
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
	149, 153, 157, 161, 165,
};

static void
mt76x2_getradiocaps(struct ieee80211com *ic, int maxchans,
    int *nchans, struct ieee80211_channel chans[])
{
	uint8_t bands[IEEE80211_MODE_BYTES];
	int cbw_flags;

	*nchans = 0;

	/* HT40 support based on capability */
	cbw_flags = (ic->ic_htcaps & IEEE80211_HTCAP_CHWIDTH40) ?
	    NET80211_CBW_FLAG_HT40 : 0;

	/* 2.4 GHz band: b/g/n (with HT40 if capable) */
	memset(bands, 0, sizeof(bands));
	setbit(bands, IEEE80211_MODE_11B);
	setbit(bands, IEEE80211_MODE_11G);
	setbit(bands, IEEE80211_MODE_11NG);
	ieee80211_add_channels_default_2ghz(chans, maxchans, nchans,
	    bands, cbw_flags);

	/* 5 GHz band: a/n/ac (with HT40 + VHT80 if capable) */
	memset(bands, 0, sizeof(bands));
	setbit(bands, IEEE80211_MODE_11A);
	setbit(bands, IEEE80211_MODE_11NA);
	if (IEEE80211_CONF_VHT(ic)) {
		setbit(bands, IEEE80211_MODE_VHT_5GHZ);
		if (cbw_flags & NET80211_CBW_FLAG_HT40)
			cbw_flags |= NET80211_CBW_FLAG_VHT80;
	}
	ieee80211_add_channel_list_5ghz(chans, maxchans, nchans,
	    mt76x2_5ghz_chans, nitems(mt76x2_5ghz_chans), bands,
	    cbw_flags);
}

/*
 * Lightweight MAC stop for channel switching — matches Linux
 * mt76x2_mac_stop(dev, false) from mt76x2/mac.c.
 * Just disables TX+RX and briefly polls for idle (~300us).
 * Linux USB driver uses this (not the heavy USB drain) during
 * channel switches — see mt76x2u_set_channel() in usb_main.c.
 */
static void
mt76x2_mac_stop_light(struct mt76x2_softc *sc)
{
	uint32_t val;
	int i, ok;

	/* Disable TX and RX */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL, 0);

	/* Poll MAC_STATUS for TX+RX idle, up to ~300us */
	ok = 0;
	for (i = 0; i < 300; i++) {
		mt76x2_read(sc, MT76X2_MAC_STATUS, &val);
		if (val & (MT76X2_MAC_STATUS_TX_BUSY |
		    MT76X2_MAC_STATUS_RX_BUSY)) {
			DELAY(1);
			ok = 0;
			continue;
		}
		if (++ok > 10)
			break;
		DELAY(1);
	}
}

static int
mt76x2_set_chan(struct mt76x2_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int chan, center_chan;
	int error = 0;
	int is_5ghz;
	int ch_group_index, bw, bw_index;
	uint32_t val;

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE, "set_chan: %d MHz\n",
	    c ? c->ic_freq : 0);

	/*
	 * MCU SWITCH_CHANNEL message — 8-byte packed struct matching
	 * Linux mt76x2_mcu_set_channel().
	 */
	struct {
		uint8_t		idx;
		uint8_t		scan;
		uint8_t		bw;
		uint8_t		_pad0;
		uint16_t	chainmask;
		uint8_t		ext_chan;
		uint8_t		_pad1;
	} __packed msg;

	if (sc->sc_mcu_timeout)
		return (EIO);

	/* Reset per-channel calibration state */
	sc->dpd_cal_done = 0;
	sc->tssi_comp_pending = 0;
	sc->cal_low_gain = -1;
	sc->ed_trigger = 0;
	sc->ed_silent = 0;

	if (c == NULL || c == IEEE80211_CHAN_ANYC ||
	    c->ic_freq == 0) {
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
		    "set_chan: no channel set yet, skipping\n");
		return (0);
	}

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY) {
		device_printf(sc->sc_dev, "invalid channel %u\n", chan);
		return (EINVAL);
	}

	is_5ghz = IEEE80211_IS_CHAN_5GHZ(c);

	/*
	 * Compute center channel and bandwidth parameters matching
	 * Linux mt76x2u_phy_set_channel().  The MCU expects the center
	 * channel index, not the primary channel.
	 */
	center_chan = chan;
	ch_group_index = 0;
	bw = 0;
	bw_index = 0;

	if (IEEE80211_IS_CHAN_VHT80(c)) {
		/*
		 * VHT80: center channel from ic_vht_ch_freq1.
		 * ch_group_index = position of primary within 80MHz block.
		 * Matches Linux: ch_group_index = (freq - freq1 + 30) / 20
		 * which equals (chan - center + 6) / 4 in channel numbers.
		 */
		bw = 2;
		center_chan = c->ic_vht_ch_freq1;
		ch_group_index = (int)(chan - center_chan + 6) / 4;
		if (ch_group_index < 0 || ch_group_index > 3)
			ch_group_index = 0;
		bw_index = ch_group_index;
	} else if (IEEE80211_IS_CHAN_HT40(c) ||
	    IEEE80211_IS_CHAN_VHT40(c)) {
		bw = 1;
		if (IEEE80211_IS_CHAN_HT40U(c)) {
			/* HT40+: primary is lower, center = primary + 2 */
			center_chan = chan + 2;
			bw_index = 1;
			ch_group_index = 0;
		} else {
			/* HT40-: primary is upper, center = primary - 2 */
			center_chan = chan - 2;
			bw_index = 3;
			ch_group_index = 1;
		}
	}

	/*
	 * Same-channel fast-path: skip entire channel switch if we're
	 * returning to the same channel and bandwidth (e.g., scan_end
	 * restoring BSS channel, or double channel switch from
	 * sta_join1).  This avoids ~100ms+ of MCU commands that would
	 * otherwise cause AUTH timeout.
	 */
	if (center_chan == sc->sc_last_cal_chan && !sc->sc_scanning) {
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
		    "set_chan: same channel %d, fast skip\n", center_chan);
		return (0);
	}

	/*
	 * Block new TX, flush queued frames, and drain in-flight USB
	 * transfers.  Matches Linux: set_bit(MT76_RESET) +
	 * mt76_worker_disable() + wait_event_timeout(!tx_pending).
	 *
	 * Without draining, USB bulk OUT frames arriving at the device
	 * while MAC TX is disabled get NAKed by firmware.  After ~60
	 * channel switches the firmware's internal TX buffer fills
	 * and it stops accepting new frames entirely (USB_ERR_TIMEOUT).
	 */
	sc->sc_chan_changing = 1;
	{
		uint32_t dma_val;
		int j;
		for (j = 0; j < 100; j++) {
			mt76x2_read_cfg(sc, MT76X2_USB_U3DMA_CFG, &dma_val);
			if (!(dma_val & (1U << 31)))	/* TX_BUSY */
				break;
			DELAY(50);
		}
	}

	/*
	 * Lightweight MAC stop for channel switch — just disable TX+RX
	 * and poll for idle (~300us).  Linux mt76x2u_set_channel() uses
	 * the PCI-style mt76x2_mac_stop(dev, false), NOT the heavy USB
	 * mt76x2u_mac_stop() which drains USB DMA (550ms-10s).
	 */
	mt76x2_mac_stop_light(sc);

	/*
	 * Per-channel setup matching Linux mt76x2u_phy_set_channel():
	 * Read EEPROM RX gain, set TX power regs, configure TX delay.
	 * These must come before phy_set_band/bw and MCU channel switch.
	 */
	mt76x2_read_rx_gain(sc, chan, is_5ghz);
	mt76x2_phy_set_txpower_regs(sc, is_5ghz);
	mt76x2_configure_tx_delay(sc, is_5ghz, bw);

	/*
	 * Set per-channel TX power BEFORE MCU SWITCH_CHANNEL — matches
	 * Linux mt76x2u_phy_set_channel() line 124.  The firmware reads
	 * TX_ALC_CFG_0 during channel switch to configure the PA/LNA
	 * shared front-end.  Writing after SWITCH_CHANNEL is too late.
	 */
	mt76x2_phy_set_txpower(sc, c);

	/* Configure band and bandwidth in BBP before MCU channel switch */
	mt76x2_phy_set_band(sc, is_5ghz, ch_group_index & 1);
	mt76x2_phy_set_bw(sc, c, ch_group_index);

	/*
	 * EXT_CCA configuration — matches Linux ext_cca_chan[] table.
	 * For 20MHz (ch_group_index=0): CCA0=0,CCA1=1,CCA2=2,CCA3=3,MASK=BIT(0)
	 */
	{
		static const uint32_t ext_cca_chan[4] = {
			/* [0] = */ 0x000001e4,	/* CCA0=0 CCA1=1 CCA2=2 CCA3=3 MASK=BIT(0) */
			/* [1] = */ 0x000002e1,	/* CCA0=1 CCA1=0 CCA2=2 CCA3=3 MASK=BIT(1) */
			/* [2] = */ 0x0000041e,	/* CCA0=2 CCA1=3 CCA2=1 CCA3=0 MASK=BIT(2) */
			/* [3] = */ 0x0000081b,	/* CCA0=3 CCA1=2 CCA2=1 CCA3=0 MASK=BIT(3) */
		};
		mt76x2_read(sc, MT76X2_EXT_CCA_CFG, &val);
		val &= ~0xfff;	/* clear CCA0-3 + CCA_MASK (bits [11:0]) */
		val |= ext_cca_chan[ch_group_index];
		mt76x2_write(sc, MT76X2_EXT_CCA_CFG, val);
	}

	/*
	 * Issue MCU SWITCH_CHANNEL command (sent twice per Linux driver).
	 * First call: without ext_chan.  Second call: with ext_chan.
	 * Use CENTER channel, not primary.
	 */
	memset(&msg, 0, sizeof(msg));
	msg.idx = (uint8_t)center_chan;
	msg.scan = sc->sc_scanning ? 1 : 0;
	msg.bw = bw;
	msg.chainmask = htole16((sc->ntxchains << 8) | sc->nrxchains);

	/* First call — no ext_chan */
	error = mt76x2_mcu_cmd(sc, MT76X2_MCU_CMD_SWITCH_CHANNEL,
	    &msg, sizeof(msg));
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "channel switch MCU cmd failed: %d\n", error);
		goto resume;
	}

	/*
	 * Second SWITCH_CHANNEL with ext_chan — Linux ALWAYS sends
	 * both.  5ms pause between per Linux usleep_range.
	 */
	MT76X2_UNLOCK(sc);
	pause("mt76sw", hz / 200);	/* 5 ms */
	MT76X2_LOCK(sc);
	msg.ext_chan = 0xe0 + bw_index;
	error = mt76x2_mcu_cmd(sc, MT76X2_MCU_CMD_SWITCH_CHANNEL,
	    &msg, sizeof(msg));
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "channel switch MCU cmd (ext) failed: %d\n", error);
		goto resume;
	}

	/*
	 * INIT_GAIN — Linux always calls mt76x2_mcu_init_gain() with
	 * force=true (BIT(31) in channel field).
	 * gain_val = packed LNA gains from EEPROM, matching Linux
	 * mt76x2_read_rx_gain() → dev->cal.rx.mcu_gain.
	 * Use CENTER channel for INIT_GAIN too.
	 */
	{
		struct {
			uint32_t channel;
			uint32_t gain_val;
		} __packed gain_msg;
		uint32_t mcu_gain;

		mcu_gain = (sc->lna_gain_2g & 0xff) |
		    ((sc->lna_gain_5g[0] & 0xff) << 8) |
		    ((sc->lna_gain_5g[1] & 0xff) << 16) |
		    ((sc->lna_gain_5g[2] & 0xff) << 24);

		gain_msg.channel = htole32(center_chan | (1U << 31));
		gain_msg.gain_val = htole32(mcu_gain);
		mt76x2_mcu_cmd(sc, MT76X2_MCU_CMD_INIT_GAIN,
		    &gain_msg, sizeof(gain_msg));
	}

	/*
	 * Do NOT call phy_set_antenna() here — Linux USB driver does not.
	 * Our old function wrote bits [6:5] of AGC(0) which Linux never
	 * touches, corrupting the AGC and making the receiver deaf.
	 */

	/*
	 * First-time calibrations: R-cal and RC-cal.
	 * Must be done once after firmware load.
	 * Linux: lines 147-159 of usb_phy.c
	 * R-cal guarded by EEPROM BT_RCAL_RESULT != 0xff.
	 */
	if (!sc->sc_cal_done) {
		uint8_t rcal = sc->eeprom[MT76X2_EE_BT_RCAL_RESULT];

		if (rcal != 0xff)
			mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_R, 0);
	}

	/* RXDCOC calibration — Linux always does this (line 154) */
	mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_RXDCOC, center_chan);

	/* RC-cal — first time only (Linux line 157-158) */
	if (!sc->sc_cal_done) {
		mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_RC, 0);
		sc->sc_cal_done = 1;
	}

	/*
	 * AGC register configuration — matches Linux
	 * mt76x2u_phy_set_channel() lines 161-165.
	 */
	mt76x2_write(sc, MT76X2_BBP(AGC, 61), 0xff64a4e2);
	mt76x2_write(sc, MT76X2_BBP(AGC, 7),  0x08081010);
	mt76x2_write(sc, MT76X2_BBP(AGC, 11), 0x00000404);
	mt76x2_write(sc, MT76X2_BBP(AGC, 2),  0x00007070);
	mt76x2_write(sc, MT76X2_TXOP_CTRL_CFG, 0x04101b3f);

	/* Enable TXO bit 25 (Linux line 167) */
	mt76x2_read(sc, MT76X2_BBP(TXO, 4), &val);
	val |= (1U << 25);
	mt76x2_write(sc, MT76X2_BBP(TXO, 4), val);

	/* Enable LDPC RX BIT(10) + RXO BIT(8) in one RMW (Linux lines 145, 168) */
	mt76x2_read(sc, MT76X2_BBP(RXO, 13), &val);
	val |= (1U << 10) | (1U << 8);
	mt76x2_write(sc, MT76X2_BBP(RXO, 13), val);

	/*
	 * Configure antenna pins — RMW matching Linux
	 * mt76x02_edcca_tx_enable(dev, true).
	 * Linux ORs in TXANT|RXANT|RFTR_EN|TRSW_EN.
	 * Also set band-correct PA_PE enables for both chains.
	 */
	mt76x2_read(sc, MT76X2_TX_PIN_CFG, &val);
	val &= ~(MT76X2_TX_PIN_PA_PE_A0 | MT76X2_TX_PIN_PA_PE_A1 |
	    MT76X2_TX_PIN_PA_PE_G0 | MT76X2_TX_PIN_PA_PE_G1);
	val |= MT76X2_TX_PIN_CFG_TXANT |
	    MT76X2_TX_PIN_CFG_RXANT |
	    MT76X2_TX_PIN_RFTR_EN |
	    MT76X2_TX_PIN_TRSW_EN;
	if (is_5ghz)
		val |= MT76X2_TX_PIN_PA_PE_A0 | MT76X2_TX_PIN_PA_PE_A1;
	else
		val |= MT76X2_TX_PIN_PA_PE_G0 | MT76X2_TX_PIN_PA_PE_G1;
	mt76x2_write(sc, MT76X2_TX_PIN_CFG, val);

	/*
	 * Linux scan check is here (line 170): only channel_calibrate,
	 * init_agc_gain, and TSSI are skipped during scanning.
	 * Everything above runs on every channel switch.
	 *
	 * Also skip calibration when returning to a channel we already
	 * calibrated (e.g., scan_end returning to BSS channel).
	 * This avoids ~200-500ms of MCU commands that would cause
	 * AUTH timeout on the SCAN→AUTH transition.
	 */
	if (sc->sc_scanning)
		goto resume;

	/*
	 * Full channel calibration — matches Linux
	 * mt76x2u_phy_channel_calibrate(dev, true).
	 */
	mt76x2_phy_channel_calibrate(sc, is_5ghz);

	/*
	 * Apply per-channel RX gain adjustment.
	 * In Linux, this is inside mt76x2u_phy_channel_calibrate()
	 * after the MCU calibration commands complete.
	 */
	mt76x2_apply_gain_adj(sc);

	/*
	 * EDCCA init — matches Linux mt76x02_edcca_init()
	 * for the ed_monitor=false case.
	 */
	mt76x2_read(sc, MT76X2_TX_LINK_CFG, &val);
	val |= MT76X2_TX_LINK_CFG_CFACK_EN;
	mt76x2_write(sc, MT76X2_TX_LINK_CFG, val);
	/* TXOP_CTRL_CFG already written above with ED_CCA_EN clear */
	mt76x2_read(sc, MT76X2_TXOP_HLDR_ET, &val);
	val |= MT76X2_TXOP_HLDR_TX40M_BLK_EN;
	mt76x2_write(sc, MT76X2_TXOP_HLDR_ET, val);

	/* Clear EDCCA timer */
	mt76x2_read(sc, MT76X2_ED_CCA_TIMER, &val);

	/* Snapshot initial AGC gain for rate control tracking */
	mt76x2_init_agc_gain(sc);

	/* Remember this channel as calibrated */
	sc->sc_last_cal_chan = center_chan;

resume:
	/*
	 * CH_TIME_CFG: enable CCA timer and counters.
	 * Matches Linux mt76x02_mac_cc_reset().
	 */
	mt76x2_write(sc, 0x110c,	/* MT_CH_TIME_CFG */
	    (1U << 0) |			/* TIMER_EN */
	    (1U << 1) |			/* TX_AS_BUSY */
	    (1U << 2) |			/* RX_AS_BUSY */
	    (1U << 3) |			/* NAV_AS_BUSY */
	    (1U << 4) |			/* EIFS_AS_BUSY */
	    (1U << 8));			/* CH_TIMER_CLR */
	/* Read-clear channel busy/idle counters */
	mt76x2_read(sc, 0x1160, &val);	/* MT_CH_BUSY */
	mt76x2_read(sc, 0x1164, &val);	/* MT_CH_IDLE */

	/*
	 * Re-assert FCE PDMA config — the firmware may reset
	 * FCE_PDMA_GLOBAL_CONF during SWITCH_CHANNEL (similar to
	 * the known FUN_SET reset issue).  BIT(6) = TX_WB_DDONE and
	 * BIT(30) can get cleared.  RMW to restore them.
	 */
	mt76x2_read(sc, MT76X2_FCE_PDMA_GLOBAL_CONF, &val);
	val |= 0x44;
	mt76x2_write(sc, MT76X2_FCE_PDMA_GLOBAL_CONF, val);

	/*
	 * Resume MAC TX+RX — matches Linux mt76x2_mac_resume().
	 * This MUST happen after all PHY reconfiguration is complete.
	 */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL,
	    MT76X2_MAC_ENABLE_TX | MT76X2_MAC_ENABLE_RX);

	/*
	 * Resume TX submissions — matches Linux clear_bit(MT76_RESET)
	 * + mt76_worker_enable().  Kick all TX queues that may have
	 * data queued while channel switch was in progress.
	 */
	sc->sc_chan_changing = 0;
	{
		int j;
		for (j = 0; j < 4; j++)
			usbd_transfer_start(
			    sc->sc_xfer[mt76x2_wme_ac_xfer_map[j]]);
	}

	return (error);
}

static void
mt76x2_set_channel(struct ieee80211com *ic)
{
	struct mt76x2_softc *sc = ic->ic_softc;

	MT76X2_LOCK(sc);
	if (!sc->sc_detached)
		mt76x2_set_chan(sc, ic->ic_curchan);
	MT76X2_UNLOCK(sc);
}

static void
mt76x2_scan_start(struct ieee80211com *ic)
{
	struct mt76x2_softc *sc = ic->ic_softc;

	MT76X2_LOCK(sc);
	sc->sc_scanning = 1;
	/* Abort TSF sync during scan (matching if_run/if_mtw) */
	mt76x2_abort_tsf_sync(sc);
	/* Set BSSID to broadcast so all BSSes pass RX filter during scan */
	mt76x2_set_bssid(sc, ieee80211broadcastaddr);
	MT76X2_UNLOCK(sc);
}

static void
mt76x2_scan_end(struct ieee80211com *ic)
{
	struct mt76x2_softc *sc = ic->ic_softc;

	MT76X2_LOCK(sc);
	sc->sc_scanning = 0;
	/*
	 * Invalidate sc_last_cal_chan so the next set_chan() (triggered
	 * by net80211 switching to the BSS channel) does a full channel
	 * switch with scan=0 and full calibration.
	 *
	 * Do NOT call set_chan() here — ic->ic_curchan is still the
	 * last scan channel, not the BSS channel.  net80211 will call
	 * ic_set_channel() with the correct BSS channel after scan_end.
	 */
	sc->sc_last_cal_chan = 0;
	mt76x2_enable_tsf_sync(sc);
	mt76x2_set_bssid(sc, sc->sc_bssid);
	MT76X2_UNLOCK(sc);
}

/*
 * ============================================================
 * VAP management
 * ============================================================
 */

static struct ieee80211vap *
mt76x2_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ],
    int unit, enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct mt76x2_softc *sc = ic->ic_softc;
	struct mt76x2_vap *mvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))
		return (NULL);

	mvp = malloc(sizeof(struct mt76x2_vap), M_80211_VAP, M_WAITOK | M_ZERO);
	vap = &mvp->vap;

	if (ieee80211_vap_setup(ic, vap, name, unit, opmode,
	    flags, bssid) != 0) {
		free(mvp, M_80211_VAP);
		return (NULL);
	}

	/* Override state transition */
	mvp->newstate = vap->iv_newstate;
	vap->iv_newstate = mt76x2_newstate;

	/* Key management for WPA2/WEP */
	vap->iv_key_set = mt76x2_key_set;
	vap->iv_key_delete = mt76x2_key_delete;

	/* Track monitor mode in softc for RX filter decisions */
	sc->sc_monitor = (opmode == IEEE80211_M_MONITOR);

	if (opmode != IEEE80211_M_MONITOR) {
		ieee80211_ratectl_init(vap);
		ieee80211_ratectl_setinterval(vap, 1000);

		/* AMPDU parameters advertised in HT capabilities to peer */
		vap->iv_ampdu_rxmax = IEEE80211_HTCAP_MAXRXAMPDU_64K;
		vap->iv_ampdu_density = IEEE80211_HTCAP_MPDUDENSITY_4;
	}

	/* Beacon update callback for AP/IBSS (TIM, CSA, HT changes) */
	if (opmode == IEEE80211_M_HOSTAP || opmode == IEEE80211_M_IBSS)
		vap->iv_update_beacon = mt76x2_update_beacon;

	/* Complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status, mac);

	ic->ic_opmode = opmode;

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "vap created, opmode %d\n", opmode);

	return (vap);
}

static void
mt76x2_vap_delete(struct ieee80211vap *vap)
{
	struct mt76x2_vap *mvp = MT76X2_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_softc *sc = ic->ic_softc;

	/*
	 * Cancel any in-progress scan and wait for it to finish.
	 * ieee80211_cancel_anyscan() only signals the scan to stop;
	 * the scan task on ic->ic_tq may still hold node table locks.
	 * taskqueue_drain_all() blocks until the scan task completes,
	 * preventing page faults in ieee80211_node_table_reset().
	 */
	ieee80211_cancel_anyscan(vap);
	taskqueue_drain_all(ic->ic_tq);

	/* Stop callouts before draining their tasks */
	MT76X2_LOCK(sc);
	usb_callout_stop(&sc->ratectl_ch);
	usb_callout_stop(&sc->temp_cal_ch);
	MT76X2_UNLOCK(sc);

	ieee80211_draintask(ic, &sc->cmdq_task);
	ieee80211_draintask(ic, &sc->ratectl_task);
	ieee80211_draintask(ic, &sc->txstat_task);
	ieee80211_draintask(ic, &sc->temp_cal_task);

	/*
	 * Detach the VAP while hardware is still alive so that
	 * ieee80211_vap_detach() → ieee80211_stop() → sta_newstate()
	 * can transmit the DISASSOC/DEAUTH frame to the AP.
	 * Without this, kldunload leaves the AP with a stale STA
	 * entry, causing unicast RX failure on the next association
	 * until wpa_cli reassociate forces a fresh key exchange.
	 *
	 * The parent callback (mt76x2_parent) handles hardware stop
	 * when ic_nrunning drops to 0 — matching if_run(4) pattern.
	 */
	if (vap->iv_opmode != IEEE80211_M_MONITOR)
		ieee80211_ratectl_deinit(vap);
	sc->sc_monitor = 0;
	ieee80211_vap_detach(vap);
	free(mvp, M_80211_VAP);
}

/*
 * ============================================================
 * State machine
 * ============================================================
 */

static int
mt76x2_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct mt76x2_vap *mvp = MT76X2_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_softc *sc = ic->ic_softc;
	enum ieee80211_state ostate;

	ostate = vap->iv_state;
	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "%s -> %s\n",
	    ieee80211_state_name[ostate],
	    ieee80211_state_name[nstate]);

	IEEE80211_UNLOCK(ic);
	MT76X2_LOCK(sc);

	/* Bail out if device is detaching to prevent use-after-free */
	if (sc->sc_detached) {
		MT76X2_UNLOCK(sc);
		IEEE80211_LOCK(ic);
		return (mvp->newstate(vap, nstate, arg));
	}

	if (ostate == IEEE80211_S_RUN) {
		/* Leaving RUN state */
		mt76x2_abort_tsf_sync(sc);
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN)
			mt76x2_set_leds(sc, 0);
		break;

	case IEEE80211_S_SCAN:
		sc->sc_scanning = 1;
		mt76x2_set_chan(sc, ic->ic_curchan);
		break;

	case IEEE80211_S_AUTH:
		sc->sc_scanning = 0;
		/*
		 * Set BSSID for auth so the hardware knows which AP
		 * we're talking to.  Save it in sc_bssid so scan_end
		 * can restore it after a background scan.
		 */
		if (vap->iv_opmode != IEEE80211_M_MONITOR) {
			IEEE80211_ADDR_COPY(sc->sc_bssid,
			    vap->iv_bss->ni_bssid);
			mt76x2_set_bssid(sc, sc->sc_bssid);
			MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
			    "AUTH: BSSID=%6D chan=%d running=%d\n",
			    sc->sc_bssid, ":",
			    ieee80211_chan2ieee(ic, ic->ic_curchan),
			    sc->sc_running);
		}
		break;

	case IEEE80211_S_ASSOC:
		break;

	case IEEE80211_S_RUN:
		sc->sc_scanning = 0;

		if (vap->iv_opmode == IEEE80211_M_MONITOR) {
			/* Monitor mode: open RX filter to pass all frames */
			mt76x2_set_monitor_filter(sc);
		} else {
			/* Clear TX stat counters (read-to-clear) */
			{
				uint32_t sta;
				mt76x2_read(sc, 0x1700, &sta);
				mt76x2_read(sc, 0x1704, &sta);
				mt76x2_read(sc, 0x1708, &sta);
			}

			mt76x2_updateslot_cb(sc);
			mt76x2_enable_mrr(sc);
			mt76x2_set_txpreamble(sc);
			mt76x2_set_basicrates(sc);
			IEEE80211_ADDR_COPY(sc->sc_bssid,
			    vap->iv_bss->ni_bssid);
			mt76x2_set_bssid(sc, sc->sc_bssid);

			/* HT/ERP protection + RTS threshold */
			mt76x2_updateprot(sc);

			/* Upload beacon for AP/IBSS before enabling TSF */
			if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
			    vap->iv_opmode == IEEE80211_M_IBSS)
				mt76x2_beacon_upload(sc, vap);

			mt76x2_enable_tsf_sync(sc);
		}

		mt76x2_set_leds(sc, 1);
		break;

	default:
		break;
	}

	MT76X2_UNLOCK(sc);
	IEEE80211_LOCK(ic);

	return (mvp->newstate(vap, nstate, arg));
}

/*
 * ============================================================
 * Node management
 * ============================================================
 */

static struct ieee80211_node *
mt76x2_node_alloc(struct ieee80211vap *vap,
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	return malloc(sizeof(struct mt76x2_node), M_80211_NODE,
	    M_NOWAIT | M_ZERO);
}

static void
mt76x2_newassoc_cb(void *arg)
{
	struct mt76x2_cmdq *cmdq = arg;
	struct ieee80211_node *ni = cmdq->arg1;
	struct mt76x2_softc *sc = ni->ni_vap->iv_ic->ic_softc;
	uint8_t wcid = cmdq->wcid;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "newassoc_cb: wcid=%d mac=%6D\n", wcid,
	    ni->ni_macaddr, ":");

	/*
	 * Write AP's MAC address to WCID table entry using 32-bit
	 * register writes.  The WCID table requires 32-bit aligned
	 * writes via mt76x2_write(); byte-level write_region_1()
	 * does not work for this register range.
	 * Matches Linux mt76x02_mac_wcid_setup() → mt76_wr_copy().
	 */
	{
		uint32_t addr_lo, addr_hi;

		addr_lo = ni->ni_macaddr[0] |
		    ((uint32_t)ni->ni_macaddr[1] << 8) |
		    ((uint32_t)ni->ni_macaddr[2] << 16) |
		    ((uint32_t)ni->ni_macaddr[3] << 24);
		addr_hi = ni->ni_macaddr[4] |
		    ((uint32_t)ni->ni_macaddr[5] << 8);

		mt76x2_write(sc, MT76X2_WCID_ENTRY(wcid), addr_lo);
		mt76x2_write(sc, MT76X2_WCID_ENTRY(wcid) + 4, addr_hi);
	}

	/*
	 * Set WCID attribute: BSS index 0, preserve cipher mode.
	 * Use RMW to avoid clobbering cipher bits set by key_set_cb().
	 * newassoc fires multiple times (re-auth, re-assoc) and would
	 * overwrite WCID_ATTR=0 if we did a full write, clearing the
	 * AES-CCMP cipher mode and causing all encrypted TX to fail.
	 */
	{
		uint32_t attr;
		mt76x2_read(sc, MT76X2_WCID_ATTR(wcid), &attr);
		attr &= ~(0x7 << MT76X2_WCID_ATTR_BSS_IDX_SHIFT);
		mt76x2_write(sc, MT76X2_WCID_ATTR(wcid), attr);
	}
}

static void
mt76x2_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_softc *sc = ic->ic_softc;
	struct mt76x2_node *mn = MT76X2_NODE(ni);
	uint8_t rate, ridx, wcid;

	wcid = (vap->iv_opmode == IEEE80211_M_STA) ?
	    1 : IEEE80211_AID(ni->ni_associd);

	device_printf(sc->sc_dev,
	    "newassoc: isnew=%d associd=0x%x wcid=%d mac=%6D\n",
	    isnew, ni->ni_associd, wcid, ni->ni_macaddr, ":");

	if (wcid > 127) {
		device_printf(sc->sc_dev, "wcid=%d out of range\n", wcid);
		return;
	}

	/* Only interested in true associations */
	if (isnew && ni->ni_associd != 0) {
		uint32_t cnt = MT76X2_CMDQ_GET(&sc->cmdq_store);

		mn->wcid = wcid;
		sc->cmdq[cnt].func = mt76x2_newassoc_cb;
		sc->cmdq[cnt].arg0 = NULL;
		sc->cmdq[cnt].arg1 = ni;
		sc->cmdq[cnt].wcid = wcid;
		ieee80211_runtask(ic, &sc->cmdq_task);
	}

	/* Initialize management frame rate index */
	rate = vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)].mgmtrate;
	for (ridx = 0; ridx < MT76X2_RIDX_MAX; ridx++)
		if (mt76x2_rates[ridx].rate == rate)
			break;
	if (ridx >= MT76X2_RIDX_MAX)
		ridx = MT76X2_RIDX_OFDM6;
	mn->mgt_ridx = ridx;

	/* Start AMRR at OFDM 6 Mbps — algorithm will ramp up */
	mn->amrr_ridx = MT76X2_RIDX_OFDM6;

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE | MT76X2_DEBUG_RATE,
	    "newassoc isnew=%d associd=0x%x addr=%6D mgt_ridx=%d\n",
	    isnew, ni->ni_associd, ni->ni_macaddr, ":", mn->mgt_ridx);
}

/*
 * ============================================================
 * Key management (WPA2/WEP hardware encryption)
 * ============================================================
 */

static void
mt76x2_key_set_cb(void *arg)
{
	struct mt76x2_cmdq *cmdq = arg;
	struct ieee80211vap *vap = cmdq->arg1;
	struct ieee80211_key *k = &cmdq->key;
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_softc *sc = ic->ic_softc;
	struct ieee80211_node *ni;
	u_int cipher = k->wk_cipher->ic_cipher;
	uint32_t attr;
	uint16_t base;
	uint8_t mode, wcid, iv[8];

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		ni = ieee80211_find_vap_node(&ic->ic_sta, vap, cmdq->mac);
	else
		ni = vap->iv_bss;

	/* Map net80211 cipher to hardware mode */
	switch (cipher) {
	case IEEE80211_CIPHER_WEP:
		mode = (k->wk_keylen < 8) ?
		    MT76X2_CIPHER_WEP40 : MT76X2_CIPHER_WEP104;
		break;
	case IEEE80211_CIPHER_TKIP:
		mode = MT76X2_CIPHER_TKIP;
		break;
	case IEEE80211_CIPHER_AES_CCM:
		mode = MT76X2_CIPHER_AES_CCMP;
		break;
	default:
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
		    "key_set: unsupported cipher %d\n", cipher);
		if (ni != NULL && vap->iv_opmode == IEEE80211_M_HOSTAP)
			ieee80211_free_node(ni);
		return;
	}

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "key_set: keyix=%d mode=%d %s\n", k->wk_keyix, mode,
	    (k->wk_flags & IEEE80211_KEY_GROUP) ? "group" : "pairwise");

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		wcid = 0;
		base = MT76X2_SKEY(0, k->wk_keyix);
	} else {
		wcid = (vap->iv_opmode == IEEE80211_M_STA) ?
		    1 : IEEE80211_AID(ni != NULL ? ni->ni_associd : 0);
		base = MT76X2_WCID_KEY(wcid);
	}

	/* Write key material */
	if (cipher == IEEE80211_CIPHER_TKIP) {
		mt76x2_write_region_1(sc, base, k->wk_key, 16);
		mt76x2_write_region_1(sc, base + 16, &k->wk_key[16], 8);
		mt76x2_write_region_1(sc, base + 24, &k->wk_key[24], 8);
	} else {
		mt76x2_write_region_1(sc, base, k->wk_key,
		    (k->wk_keylen + 1) & ~1);
	}

	/* Set IV/EIV */
	if (!(k->wk_flags & IEEE80211_KEY_GROUP) ||
	    (k->wk_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV))) {
		if (cipher == IEEE80211_CIPHER_WEP) {
			memset(iv, 0, sizeof(iv));
			iv[3] = vap->iv_def_txkey << 6;
		} else {
			if (cipher == IEEE80211_CIPHER_TKIP) {
				iv[0] = k->wk_keytsc >> 8;
				iv[1] = (iv[0] | 0x20) & 0x7f;
				iv[2] = k->wk_keytsc;
			} else {
				iv[0] = k->wk_keytsc;
				iv[1] = k->wk_keytsc >> 8;
				iv[2] = 0;
			}
			iv[3] = k->wk_keyix << 6 | IEEE80211_WEP_EXTIV;
			iv[4] = k->wk_keytsc >> 16;
			iv[5] = k->wk_keytsc >> 24;
			iv[6] = k->wk_keytsc >> 32;
			iv[7] = k->wk_keytsc >> 40;
		}
		mt76x2_write_region_1(sc, MT76X2_IVEIV(wcid), iv, 8);
	}

	/* Set cipher mode in SKEY_MODE (group) or WCID_ATTR (pairwise) */
	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		mt76x2_read(sc, MT76X2_SKEY_MODE(0), &attr);
		attr &= ~(0xf << (k->wk_keyix * 4));
		attr |= mode << (k->wk_keyix * 4);
		mt76x2_write(sc, MT76X2_SKEY_MODE(0), attr);
	} else {
		mt76x2_read(sc, MT76X2_WCID_ATTR(wcid), &attr);
		attr = (attr & ~0xf) |
		    (mode << MT76X2_WCID_ATTR_CIPHER_SHIFT) |
		    MT76X2_WCID_ATTR_PAIRWISE;
		mt76x2_write(sc, MT76X2_WCID_ATTR(wcid), attr);
		/* Mark node as having hardware key */
		if (ni != NULL)
			MT76X2_NODE(ni)->hw_key = 1;
	}

	/* Store WCID for later key_delete */
	k->wk_pad = wcid;

	if (ni != NULL && vap->iv_opmode == IEEE80211_M_HOSTAP)
		ieee80211_free_node(ni);
}

static int
mt76x2_key_set(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_softc *sc = ic->ic_softc;
	uint32_t i;

	i = MT76X2_CMDQ_GET(&sc->cmdq_store);
	sc->cmdq[i].func = mt76x2_key_set_cb;
	sc->cmdq[i].arg0 = NULL;
	sc->cmdq[i].arg1 = vap;
	memcpy(&sc->cmdq[i].key, k, sizeof(*k));
	IEEE80211_ADDR_COPY(sc->cmdq[i].mac, k->wk_macaddr);
	ieee80211_runtask(ic, &sc->cmdq_task);

	return (1);
}

static void
mt76x2_key_delete_cb(void *arg)
{
	struct mt76x2_cmdq *cmdq = arg;
	struct mt76x2_softc *sc = cmdq->arg1;
	struct ieee80211_key *k = &cmdq->key;
	uint32_t attr;
	uint8_t wcid;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (k->wk_flags & IEEE80211_KEY_GROUP) {
		mt76x2_read(sc, MT76X2_SKEY_MODE(0), &attr);
		attr &= ~(0xf << (k->wk_keyix * 4));
		mt76x2_write(sc, MT76X2_SKEY_MODE(0), attr);
	} else {
		wcid = k->wk_pad;
		mt76x2_read(sc, MT76X2_WCID_ATTR(wcid), &attr);
		attr &= ~0xf;
		mt76x2_write(sc, MT76X2_WCID_ATTR(wcid), attr);
	}

	k->wk_pad = 0;
}

static int
mt76x2_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct mt76x2_softc *sc = ic->ic_softc;
	struct ieee80211_key *k0;
	uint32_t i;

	i = MT76X2_CMDQ_GET(&sc->cmdq_store);
	sc->cmdq[i].func = mt76x2_key_delete_cb;
	sc->cmdq[i].arg0 = NULL;
	sc->cmdq[i].arg1 = sc;
	k0 = &sc->cmdq[i].key;
	k0->wk_flags = k->wk_flags;
	k0->wk_keyix = k->wk_keyix;
	k0->wk_pad = k->wk_pad;
	ieee80211_runtask(ic, &sc->cmdq_task);

	return (1);
}

/*
 * ============================================================
 * Rate control
 * ============================================================
 */

static void
mt76x2_ratectl_to(void *arg)
{
	struct mt76x2_softc *sc = arg;

	/* Callout runs with sc_mtx held; sc_running is cleared under lock */
	if (!sc->sc_running)
		return;

	taskqueue_enqueue(taskqueue_thread, &sc->ratectl_task);
}

static void
mt76x2_ratectl_cb(void *arg, int pending)
{
	struct mt76x2_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_node *ni;
	struct mt76x2_node *mn;
	struct ieee80211_node_txrate txr;
	int ridx;
	uint8_t dot11rate;

	if (vap == NULL)
		return;

	MT76X2_LOCK(sc);
	if (!sc->sc_running || sc->sc_detached) {
		MT76X2_UNLOCK(sc);
		return;
	}

	/* Only update rate in RUN state */
	if (vap->iv_state != IEEE80211_S_RUN) {
		usb_callout_reset(&sc->ratectl_ch, hz,
		    mt76x2_ratectl_to, sc);
		MT76X2_UNLOCK(sc);
		return;
	}

	ni = ieee80211_ref_node(vap->iv_bss);

	/*
	 * Feed AMRR with TX stats from TX_STAT_FIFO.
	 *
	 * The TX_STAT_FIFO reports both AMPDU and non-AMPDU frames
	 * (AGGR bit distinguishes them).  txstat_drain accumulates
	 * ok/fail/retry counts which we feed to AMRR here.
	 *
	 * When the FIFO has no entries (idle period), supplement
	 * with USB completion counts as implicit successes — if the
	 * firmware accepted the frame and didn't report a FIFO
	 * failure, the frame was likely transmitted successfully.
	 */
	{
		struct ieee80211_ratectl_tx_stats txs;
		uint32_t completed;

		completed = sc->tx_total - sc->tx_total_last;
		sc->tx_total_last = sc->tx_total;

		/* Use FIFO stats accumulated by txstat_drain */
		txs.flags = IEEE80211_RATECTL_TX_STATS_NODE |
		    IEEE80211_RATECTL_TX_STATS_RETRIES;
		txs.ni = ni;
		txs.nsuccess = sc->sc_tx_ok;
		txs.nretries = sc->sc_tx_retry;
		txs.nframes = sc->sc_tx_ok + sc->sc_tx_fail;

		/*
		 * If FIFO reported nothing but USB completions happened,
		 * count USB completions as successes.  This gives AMRR
		 * enough data to probe up during AMPDU-heavy traffic
		 * where FIFO entries may be sparse.
		 */
		if (txs.nframes == 0 && completed > 0) {
			txs.nsuccess = completed;
			txs.nframes = completed;
			txs.nretries = 0;
		}

		sc->sc_tx_ok = 0;
		sc->sc_tx_fail = 0;
		sc->sc_tx_retry = 0;

		MT76X2_DPRINTF(sc, MT76X2_DEBUG_RATE,
		    "txstats: fifo_ok=%u fifo_fail=%u usb_tx=%u\n",
		    txs.nsuccess, txs.nframes - txs.nsuccess, completed);

		if (txs.nframes > 0)
			ieee80211_ratectl_tx_update(vap, &txs);
	}

	ieee80211_ratectl_rate(ni, NULL, 0);

	mn = MT76X2_NODE(ni);

	/* Convert AMRR-selected rate to hardware rate index */
	ieee80211_node_get_txrate(ni, &txr);
	if (txr.type == IEEE80211_NODE_TXRATE_VHT) {
		mn->amrr_ridx = MT76X2_RIDX_VHT;
		mn->vht_nss = txr.nss;
		mn->vht_mcs = txr.mcs;
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_RATE,
		    "ratectl: VHT NSS=%d MCS=%d total=%u\n",
		    txr.nss, txr.mcs, sc->tx_total);
	} else {
		dot11rate = txr.dot11rate;
		for (ridx = 0; ridx < MT76X2_RIDX_MAX; ridx++)
			if (mt76x2_rates[ridx].rate == dot11rate)
				break;
		if (ridx >= MT76X2_RIDX_MAX)
			ridx = MT76X2_RIDX_OFDM6;
		if (ridx > MT76X2_RIDX_MCS0 + mt76x2_max_mcs)
			ridx = MT76X2_RIDX_MCS0 + mt76x2_max_mcs;
		mn->amrr_ridx = ridx;
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_RATE,
		    "ratectl: dot11rate=0x%02x ridx=%d mcs=%d"
		    " phy=%d total=%u\n",
		    dot11rate, ridx, mt76x2_rates[ridx].mcs,
		    mt76x2_rates[ridx].phy, sc->tx_total);
	}

	ieee80211_free_node(ni);

	taskqueue_enqueue(taskqueue_thread, &sc->txstat_task);

	usb_callout_reset(&sc->ratectl_ch, hz / 10,
	    mt76x2_ratectl_to, sc);
	MT76X2_UNLOCK(sc);
}

/*
 * Process one TX_STAT_FIFO entry: accumulate ok/fail/retry counts.
 */
static void
mt76x2_txstat_process(struct mt76x2_softc *sc, uint32_t stat,
    uint32_t stat_ext)
{
	uint8_t wcid;

	wcid = (stat >> MT76X2_TX_STAT_FIFO_WCID_SHIFT) & 0xff;

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_XMIT,
	    "txstat: stat=0x%08x ext=0x%08x wcid=%u ok=%d "
	    "retry=%u\n", stat, stat_ext, wcid,
	    !!(stat & MT76X2_TX_STAT_FIFO_SUCCESS),
	    stat_ext & MT76X2_TX_STAT_FIFO_EXT_RETRY);

	/* Only count data frames (WCID>0 with ACK requested) */
	if (!(stat & MT76X2_TX_STAT_FIFO_ACKREQ) || wcid == 0)
		return;

	if (stat & MT76X2_TX_STAT_FIFO_SUCCESS) {
		sc->sc_tx_ok++;
		mt76x2_tx_hw_ok++;
	} else {
		sc->sc_tx_fail++;
		mt76x2_tx_hw_fail++;
	}
	if ((stat_ext & MT76X2_TX_STAT_FIFO_EXT_RETRY) > 0) {
		sc->sc_tx_retry++;
		mt76x2_tx_hw_retry++;
	}
}

/*
 * Drain TX_STAT_FIFO via MCU CMD_RANDOM_READ.
 * Reads up to 12 (EXT, FIFO) pairs in one bulk transfer instead of
 * up to 32 individual USB control transfers.  The MCU accesses the
 * same register bus as USB vendor reads, so FIFO POP semantics
 * (reading 0x1718 advances to next entry) are preserved.
 *
 * Returns 0 on success, non-zero to fall back to USB control reads.
 */
static int
mt76x2_txstat_drain_mcu(struct mt76x2_softc *sc)
{
	uint32_t regs[24], vals[24];
	int error, i, n;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * Build request: 12 × [EXT addr, FIFO addr] = 24 registers.
	 * EXT must be read before FIFO because FIFO read pops the entry.
	 * CMD_RANDOM_READ processes addresses sequentially in request order.
	 */
	n = 0;
	for (i = 0; i < 12; i++) {
		regs[n++] = MT76X2_TX_STAT_FIFO_EXT;
		regs[n++] = MT76X2_TX_STAT_FIFO;
	}

	error = mt76x2_mcu_random_read(sc, regs, vals, n);
	if (error != 0)
		return (error);

	/* Parse results: pairs of (ext, fifo) */
	for (i = 0; i < 12; i++) {
		uint32_t stat_ext = vals[i * 2];
		uint32_t stat = vals[i * 2 + 1];

		if (!(stat & MT76X2_TX_STAT_FIFO_VALID))
			break;

		mt76x2_txstat_process(sc, stat, stat_ext);
	}

	return (0);
}

/*
 * Drain TX_STAT_FIFO to prevent firmware TX stall.
 * The hardware writes a status entry for each transmitted frame.
 * If the 16-entry FIFO fills up, firmware stops transmitting.
 * Matches if_mtw(4) mtw_drain_fifo() and Linux mt76x02_mac_poll_tx_status().
 *
 * Uses MCU CMD_RANDOM_READ to batch all register reads into one USB
 * bulk transfer.  Falls back to individual USB control transfers if
 * the MCU is unavailable (timeout, not running).
 */
static void
mt76x2_txstat_drain(void *arg, int pending)
{
	struct mt76x2_softc *sc = arg;
	uint32_t stat, stat_ext;
	int i;

	MT76X2_LOCK(sc);
	if (!sc->sc_running || sc->sc_detached) {
		MT76X2_UNLOCK(sc);
		return;
	}

	/*
	 * Skip drain if no TX completions since last drain.
	 * No new FIFO entries can appear without TX activity,
	 * so this avoids wasting USB bus time during RX-heavy workloads.
	 */
	if (sc->tx_total == sc->tx_total_drain) {
		MT76X2_UNLOCK(sc);
		return;
	}
	sc->tx_total_drain = sc->tx_total;

	/*
	 * Try MCU bulk read first (1 bulk OUT + 1 bulk IN vs up to
	 * 32 control transfers).  Fall back to control reads if MCU
	 * is unavailable or timed out.
	 */
	if (!sc->sc_mcu_timeout) {
		if (mt76x2_txstat_drain_mcu(sc) == 0) {
			MT76X2_UNLOCK(sc);
			return;
		}
	}

	/* Fallback: individual USB control reads */
	for (i = 0; i < 16; i++) {
		/* EXT must be read BEFORE FIFO (reading FIFO pops entry) */
		mt76x2_read(sc, MT76X2_TX_STAT_FIFO_EXT, &stat_ext);
		mt76x2_read(sc, MT76X2_TX_STAT_FIFO, &stat);
		if (!(stat & MT76X2_TX_STAT_FIFO_VALID))
			break;

		mt76x2_txstat_process(sc, stat, stat_ext);
	}
	MT76X2_UNLOCK(sc);
}

/*
 * ============================================================
 * Temperature calibration
 * ============================================================
 */

/*
 * Periodic temperature sensor recalibration.
 * Prevents RF drift on long sessions.  The MCU's internal calibration
 * compensates for temperature changes in the PA and LNA.
 * Fires every 60 seconds (matching Linux mt76x2 watchdog interval).
 */
static void
mt76x2_temp_cal_to(void *arg)
{
	struct mt76x2_softc *sc = arg;

	if (!sc->sc_running)
		return;

	taskqueue_enqueue(taskqueue_thread, &sc->temp_cal_task);
}

/*
 * TSSI closed-loop TX power calibration.
 * Two-phase state machine matching Linux mt76x2_phy_tssi_compensate().
 * Phase 1: trigger TSSI measurement (cal_mode = BIT(0))
 * Phase 2: read result and compensate (cal_mode = BIT(1)), then DPD.
 */
static void
mt76x2_phy_tssi_compensate(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct mt76x2_tx_power_info txp;
	uint32_t val;
	int is_5ghz, ext_pa;
	uint16_t nic0;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	/* Only after first channel calibration */
	if (!sc->sc_cal_done)
		return;

	is_5ghz = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan);
	nic0 = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF0]);
	if (is_5ghz)
		ext_pa = !(nic0 & (1U << 8));
	else
		ext_pa = !(nic0 & (1U << 9));

	if (!sc->tssi_comp_pending) {
		/* Phase 1: trigger TSSI measurement */
		struct {
			uint32_t id;
			uint8_t  pa_mode;
			uint8_t  cal_mode;
			uint16_t pad;
			uint8_t  slope0, slope1;
			uint8_t  offset0, offset1;
		} __packed msg;

		memset(&msg, 0, sizeof(msg));
		msg.id = htole32(MT76X2_MCU_CAL_TSSI_COMP);
		msg.cal_mode = 1;	/* BIT(0) = trigger */
		mt76x2_mcu_cmd(sc, MT76X2_MCU_CMD_CALIBRATION,
		    &msg, sizeof(msg));
		sc->tssi_comp_pending = 1;
	} else {
		/* Phase 2: check completion, then compensate */
		mt76x2_read(sc, MT76X2_BBP(CORE, 34), &val);
		if (val & (1U << 4))
			return;		/* still in progress */

		sc->tssi_comp_pending = 0;

		mt76x2_get_power_info(sc, &txp, ic->ic_curchan);

		{
			struct {
				uint32_t id;
				uint8_t  pa_mode;
				uint8_t  cal_mode;
				uint16_t pad;
				uint8_t  slope0, slope1;
				uint8_t  offset0, offset1;
			} __packed msg;

			memset(&msg, 0, sizeof(msg));
			msg.id = htole32(MT76X2_MCU_CAL_TSSI_COMP);
			msg.pa_mode = ext_pa ? 1 : 0;
			msg.cal_mode = 2;	/* BIT(1) = compensate */
			msg.slope0 = txp.chain[0].tssi_slope;
			msg.slope1 = txp.chain[1].tssi_slope;
			msg.offset0 = txp.chain[0].tssi_offset;
			msg.offset1 = txp.chain[1].tssi_offset;
			mt76x2_mcu_cmd(sc, MT76X2_MCU_CMD_CALIBRATION,
			    &msg, sizeof(msg));
		}

		/* DPD calibration — once per channel, skip if ext PA */
		if (!ext_pa && !sc->dpd_cal_done) {
			int chan;

			chan = ieee80211_chan2ieee(ic, ic->ic_curchan);
			DELAY(10000);
			mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_DPD, chan);
			sc->dpd_cal_done = 1;
		}
	}
}

/*
 * Write AGC gain values to BBP(AGC, 8/9).
 * Matches Linux mt76x2_phy_set_gain_val().
 */
static void
mt76x2_phy_set_gain_val(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t upper;
	int gain_val0, gain_val1;
	int ext_lna, is_2g, is_ht40plus;
	uint16_t nic1;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	gain_val0 = sc->agc_gain_cur[0] - sc->agc_gain_adjust;
	gain_val1 = sc->agc_gain_cur[1] - sc->agc_gain_adjust;

	if (gain_val0 < 0) gain_val0 = 0;
	if (gain_val1 < 0) gain_val1 = 0;
	if (gain_val0 > 0x7f) gain_val0 = 0x7f;
	if (gain_val1 > 0x7f) gain_val1 = 0x7f;

	nic1 = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF1]);
	is_2g = IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan);
	is_ht40plus = IEEE80211_IS_CHAN_HT40(ic->ic_curchan);

	ext_lna = is_2g ?
	    !!(nic1 & MT76X2_EE_NIC_CONF1_LNA_EXT_2G) :
	    !!(nic1 & MT76X2_EE_NIC_CONF1_LNA_EXT_5G);

	upper = 0x1836;
	if (!ext_lna && is_ht40plus)
		upper = 0x1e42;
	if (ext_lna && is_2g && !is_ht40plus)
		upper = 0x0f36;

	mt76x2_write(sc, MT76X2_BBP(AGC, 8),
	    ((uint32_t)upper << 16) | 0xf8 |
	    ((uint32_t)(gain_val0 & 0x7f) << 8));
	mt76x2_write(sc, MT76X2_BBP(AGC, 9),
	    ((uint32_t)upper << 16) | 0xf8 |
	    ((uint32_t)(gain_val1 & 0x7f) << 8));
}

/*
 * AGC dynamic gain update — adjusts receiver gain based on RSSI
 * and false CCA count.  Matches Linux mt76x2_phy_update_channel_gain()
 * and mt76x02_phy_adjust_vga_gain().
 */
static void
mt76x2_phy_update_channel_gain(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	int rssi_dbm, nf, low_gain, gain_change;
	int thresh_high, thresh_low;
	int low_gain_delta, gain_delta, limit;
	uint32_t false_cca, val;
	int is_2g, is_ht40, ext_lna;
	uint16_t nic1;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (vap == NULL || vap->iv_state != IEEE80211_S_RUN)
		return;
	if (vap->iv_opmode == IEEE80211_M_MONITOR)
		return;

	/* Get RSSI from BSS node (net80211 LPF-averaged raw scale) */
	{
		int raw_rssi;

		raw_rssi = (vap->iv_bss != NULL) ?
		    IEEE80211_RSSI_GET(vap->iv_bss->ni_avgrssi) : 0;
		if (raw_rssi == 0 || raw_rssi == IEEE80211_RSSI_DUMMY_MARKER) {
			rssi_dbm = -75;
		} else {
			nf = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ?
			    -98 : -95;
			rssi_dbm = (raw_rssi / 2) + nf;
		}
	}

	is_ht40 = IEEE80211_IS_CHAN_HT40(ic->ic_curchan);

	/* BW-dependent thresholds (Linux mt76x02_get_rssi_gain_thresh) */
	thresh_high = is_ht40 ? -65 : -68;
	thresh_low = is_ht40 ? -79 : -82;

	/* Determine gain state: 0=normal, 1=medium, 2=low gain */
	low_gain = (rssi_dbm > thresh_high) + (rssi_dbm > thresh_low);

	gain_change = (sc->cal_low_gain < 0) ||
	    ((sc->cal_low_gain & 2) ^ (low_gain & 2));
	sc->cal_low_gain = low_gain;

	if (!gain_change) {
		/* No major gain change — try VGA fine-tuning */
		mt76x2_read(sc, MT76X2_RX_STAT_1, &val);
		false_cca = val & MT76X2_RX_STAT_1_CCA_ERRORS;
		sc->cal_false_cca = false_cca;

		limit = (low_gain > 0) ? 16 : 4;

		if (false_cca > 800 &&
		    sc->agc_gain_adjust < limit) {
			sc->agc_gain_adjust += 2;
			mt76x2_phy_set_gain_val(sc);
		} else if ((false_cca < 10 &&
		    sc->agc_gain_adjust > 0) ||
		    (sc->agc_gain_adjust >= limit &&
		    false_cca < 500)) {
			sc->agc_gain_adjust -= 2;
			mt76x2_phy_set_gain_val(sc);
		}
		return;
	}

	/* Major gain state change */
	is_2g = IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan);
	nic1 = le16dec(&sc->eeprom[MT76X2_EE_NIC_CONF1]);
	ext_lna = is_2g ?
	    !!(nic1 & MT76X2_EE_NIC_CONF1_LNA_EXT_2G) :
	    !!(nic1 & MT76X2_EE_NIC_CONF1_LNA_EXT_5G);

	low_gain_delta = ext_lna ? 10 : 14;

	if (low_gain == 2) {
		/* Low gain mode (strong signal) */
		mt76x2_write(sc, MT76X2_BBP(RXO, 18), 0xf000a990);
		mt76x2_write(sc, MT76X2_BBP(AGC, 35), 0x08080808);
		mt76x2_write(sc, MT76X2_BBP(AGC, 37), 0x08080808);
		gain_delta = low_gain_delta;
		sc->agc_gain_adjust = 0;
	} else {
		/* Normal/medium gain (weak signal) */
		uint32_t agc_35, agc_37;

		mt76x2_write(sc, MT76X2_BBP(RXO, 18), 0xf000a991);
		agc_37 = 0x2121262c;
		agc_35 = is_2g ? 0x11111516 : 0x11111116;
		mt76x2_write(sc, MT76X2_BBP(AGC, 35), agc_35);
		mt76x2_write(sc, MT76X2_BBP(AGC, 37), agc_37);
		gain_delta = 0;
		sc->agc_gain_adjust = low_gain_delta;
	}

	sc->agc_gain_cur[0] = sc->agc_gain_init[0] - gain_delta;
	sc->agc_gain_cur[1] = sc->agc_gain_init[1] - gain_delta;
	mt76x2_phy_set_gain_val(sc);

	/* Clear false CCA counter */
	mt76x2_read(sc, MT76X2_RX_STAT_1, &val);
}

/*
 * EDCCA monitoring — gate TX when channel is >92% busy.
 * Matches Linux mt76x02_edcca_check().
 */
static void
mt76x2_edcca_check(struct mt76x2_softc *sc)
{
	uint32_t timer_val, busy;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	mt76x2_read(sc, MT76X2_ED_CCA_TIMER, &timer_val);

	/* busy % over 60-second interval (timer counts microseconds) */
	busy = timer_val / 600000;	/* timer_val * 100 / 60000000 */
	if (busy > 100)
		busy = 100;

	if (busy > 92) {
		sc->ed_trigger++;
		sc->ed_silent = 0;
	} else {
		sc->ed_silent++;
		sc->ed_trigger = 0;
	}

	if (sc->ed_trigger > 2 && !sc->ed_tx_blocked) {
		/* Block TX */
		mt76x2_write(sc, MT76X2_MAC_SYS_CTRL,
		    MT76X2_MAC_ENABLE_RX);
		sc->ed_tx_blocked = 1;
	} else if (sc->ed_silent > 2 && sc->ed_tx_blocked) {
		/* Unblock TX */
		mt76x2_write(sc, MT76X2_MAC_SYS_CTRL,
		    MT76X2_MAC_ENABLE_TX | MT76X2_MAC_ENABLE_RX);
		sc->ed_tx_blocked = 0;
	}
}

/*
 * MAC error watchdog — detect stuck MAC and CSR reset.
 * Matches Linux mt76x02_check_mac_err() for non-beacon mode.
 */
static void
mt76x2_check_mac_err(struct mt76x2_softc *sc)
{
	uint32_t val;
	int ntries;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	mt76x2_read(sc, MT76X2_MAC_ERR_STATUS, &val);

	/* Check for stuck MAC condition */
	if (!(val & (1U << 29)) || !(val & ((1U << 7) | (1U << 5))))
		return;

	device_printf(sc->sc_dev, "MAC error detected, resetting\n");

	/* Stop MAC */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL, 0);

	/* Wait for TX/RX idle */
	for (ntries = 0; ntries < 100; ntries++) {
		uint32_t txq, rxq;

		mt76x2_read(sc, MT76X2_TXQ_STA, &txq);
		mt76x2_read(sc, MT76X2_RXQ_STA, &rxq);
		if (txq == 0 && rxq == 0)
			break;
		DELAY(100);
	}

	/* CSR reset pulse */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL, MT76X2_MAC_RESET_CSR);
	DELAY(10);

	/* Re-enable TX/RX */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL,
	    MT76X2_MAC_ENABLE_TX | MT76X2_MAC_ENABLE_RX);

	sc->mac_err_count++;
	mt76x2_mac_err_count = sc->mac_err_count;
}

/*
 * Per-frame TX power adjustment.
 * Returns 5-bit value for TXWI ctl2 field.
 * Matches Linux mt76x02_tx_get_txpwr_adj().
 */
static int
mt76x2_tx_get_txpwr_adj(struct mt76x2_softc *sc, int ridx)
{
	int8_t txpwr;
	int adj;

	/* Look up per-rate power from rate_power table */
	if (ridx <= MT76X2_RIDX_CCK11)
		txpwr = sc->rate_power.cck[ridx];
	else if (ridx <= MT76X2_RIDX_OFDM54)
		txpwr = sc->rate_power.ofdm[ridx - MT76X2_RIDX_OFDM6];
	else if (ridx <= MT76X2_RIDX_MCS15)
		txpwr = sc->rate_power.ht[ridx - MT76X2_RIDX_MCS0];
	else if (ridx == MT76X2_RIDX_VHT)
		txpwr = sc->rate_power.vht[0];	/* VHT MCS8-9 delta */
	else
		return (0);

	adj = txpwr - (sc->target_power + sc->target_power_delta[0]);

	if (adj >= 0)
		return (adj > 7 ? 7 : adj);
	else if (adj < -16)
		return (8);
	else
		return ((adj + 32) / 2);
}

static void
mt76x2_temp_cal_cb(void *arg, int pending)
{
	struct mt76x2_softc *sc = arg;

	MT76X2_LOCK(sc);
	if (!sc->sc_running || sc->sc_detached) {
		MT76X2_UNLOCK(sc);
		return;
	}

	/* Temperature sensor recalibration */
	mt76x2_mcu_calibrate(sc, MT76X2_MCU_CAL_TEMP_SENSOR, 0);
	sc->sc_temperature++;
	mt76x2_temp_cal_count = sc->sc_temperature;

	/* TSSI + DPD closed-loop TX power calibration */
	mt76x2_phy_tssi_compensate(sc);

	/* AGC dynamic gain + VGA false CCA adjustment */
	mt76x2_phy_update_channel_gain(sc);

	/* EDCCA channel busy monitoring */
	mt76x2_edcca_check(sc);

	/* MAC error watchdog */
	mt76x2_check_mac_err(sc);

	usb_callout_reset(&sc->temp_cal_ch, 60 * hz,
	    mt76x2_temp_cal_to, sc);
	MT76X2_UNLOCK(sc);
}

/*
 * ============================================================
 * TX path
 * ============================================================
 */

static void
mt76x2_tx_free(struct mt76x2_endpoint_queue *pq,
    struct mt76x2_tx_data *data, int txerr)
{
	if (data->ni != NULL && data->m != NULL)
		ieee80211_tx_complete(data->ni, data->m, txerr);
	else {
		if (data->m != NULL)
			m_freem(data->m);
		if (data->ni != NULL)
			ieee80211_free_node(data->ni);
	}
	data->m = NULL;
	data->ni = NULL;

	STAILQ_INSERT_TAIL(&pq->tx_fh, data, next);
	pq->tx_nfree++;
}

static int
mt76x2_tx(struct mt76x2_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni)
{
	struct mt76x2_tx_data *data;
	struct mt76x2_txwi *txwi;
	struct ieee80211_frame *wh;
	uint32_t txinfo;
	uint16_t rate_val;
	uint8_t type, mcs, phy, ridx;
	uint8_t qid;
	int xfer_len;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	/* Block new TX during channel switch to prevent USB DMA wedge */
	if (sc->sc_chan_changing)
		return (EBUSY);

	wh = mtod(m, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	/*
	 * Hardware crypto: the MT76x2 encrypts data frames using the
	 * WCID key table (programmed via key_set_cb).  We do NOT call
	 * ieee80211_crypto_encap() — hardware adds the CCMP header
	 * and encrypts the frame body when WIV=0 in the TXD.
	 */

	/* Select queue */
	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		uint8_t tid = ieee80211_gettid(wh);
		qid = TID_TO_WME_AC(tid);
	} else {
		qid = WME_AC_BE;
	}
	if (type == IEEE80211_FC0_TYPE_MGT)
		qid = WME_AC_BE;

	/*
	 * Route small data frames (TCP ACKs) to VO endpoint.
	 * VO has a shorter EDCA contention window than BE, giving
	 * ACKs faster airtime access and slightly lower latency.
	 *
	 * Note: multi-endpoint parallelism was tested but doesn't
	 * help for single-stream TCP because ACKs arrive ~12ms apart
	 * (one per 2 segments at ~2 Mbps) — longer than the ~5ms USB
	 * transfer time.  There is never queue contention.  The real
	 * fix requires FreeBSD USB framework parallel transfers.
	 */
	if (type == IEEE80211_FC0_TYPE_DATA &&
	    m->m_pkthdr.len < 200 &&
	    qid == WME_AC_BE &&
	    sc->sc_epq[WME_AC_VO].tx_nfree >= 2)
		qid = WME_AC_VO;

	/* Check for space */
	if (sc->sc_epq[qid].tx_nfree < 2) {
		mt76x2_tx_epq_full++;
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_XMIT,
		    "tx ring %d is full\n", qid);
		return (ENOBUFS);
	}

	/* Select rate index */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA ||
	    (m->m_flags & M_EAPOL)) {
		/* Multicast, mgmt, EAPOL: fixed OFDM 6 Mbps */
		ridx = MT76X2_RIDX_OFDM6;
	} else {
		struct ieee80211_node *tp_ni = ni;
		struct ieee80211vap *tp_vap = tp_ni->ni_vap;
		const struct ieee80211_txparam *tp;

		tp = &tp_vap->iv_txparms[
		    ieee80211_chan2mode(sc->sc_ic.ic_curchan)];
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_RATE,
		    "tx rate sel: ucastrate=%d amrr_ridx=%d\n",
		    tp->ucastrate, MT76X2_NODE(ni)->amrr_ridx);
		if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE) {
			/* Fixed rate configured by user */
			uint8_t r;
			int i;
			r = tp->ucastrate;
			ridx = MT76X2_RIDX_OFDM6;
			for (i = 0; i < MT76X2_RIDX_MAX; i++)
				if (mt76x2_rates[i].rate == r) {
					ridx = i;
					break;
				}
		} else {
			ridx = MT76X2_NODE(ni)->amrr_ridx;
			if (ridx != MT76X2_RIDX_VHT && ridx >= MT76X2_RIDX_MAX)
				ridx = MT76X2_RIDX_OFDM6;
		}
	}

	/*
	 * Translate rate index to TXWI mcs/phy fields.
	 * VHT rates use a separate encoding: PHY=4, MCS bits [3:0]=MCS,
	 * bits [5:4]=NSS-1.  Matches Linux mt76x02_mac.c line 186.
	 */
	if (ridx == MT76X2_RIDX_VHT) {
		struct mt76x2_node *mn = MT76X2_NODE(ni);
		phy = MT76X2_TXWI_PHY_VHT;
		mcs = ((mn->vht_nss - 1) << 4) | mn->vht_mcs;
	} else {
		mcs = mt76x2_rates[ridx].mcs;
		if (mt76x2_rates[ridx].phy == IEEE80211_T_DS)
			phy = MT76X2_TXWI_PHY_CCK;
		else if (mt76x2_rates[ridx].phy == IEEE80211_T_OFDM)
			phy = MT76X2_TXWI_PHY_OFDM;
		else
			phy = MT76X2_TXWI_PHY_HT;
	}

#ifdef MT76X2_DEBUG
	mt76x2_last_tx_ridx = ridx;
#endif

	/* Get a free TX data slot */
	data = STAILQ_FIRST(&sc->sc_epq[qid].tx_fh);
	STAILQ_REMOVE_HEAD(&sc->sc_epq[qid].tx_fh, next);
	sc->sc_epq[qid].tx_nfree--;

	/*
	 * Build TXD (TXINFO) + TXWI header.
	 *
	 * USB TX frame layout (matching Linux mt76x02u_skb_dma_info):
	 *   | 4B TXD | 20B TXWI | 802.11 frame | pad to 4B | 4B zero |
	 *
	 * TXD tells firmware: destination port (WLAN), queue select,
	 * and transfer length.
	 */
	memset(data->desc, 0, sizeof(data->desc));

	/* Build TXWI after TXD header */
	txwi = (struct mt76x2_txwi *)(data->desc + MT76X2_DMA_HDR_LEN);

	rate_val = (mcs & MT76X2_TXWI_MCS_MASK) |
	    ((uint16_t)phy << MT76X2_TXWI_PHY_SHIFT);

	if ((ridx >= MT76X2_RIDX_MCS0 || ridx == MT76X2_RIDX_VHT) &&
	    ni != NULL) {
		struct ieee80211com *ic = &sc->sc_ic;
		int is_vht = (phy == MT76X2_TXWI_PHY_VHT);

		/* SGI: check VHT cap for VHT80, HT cap otherwise */
		if (is_vht && IEEE80211_IS_CHAN_VHT80(ic->ic_curchan)) {
			if (ni->ni_vhtcap & IEEE80211_VHTCAP_SHORT_GI_80)
				rate_val |= MT76X2_TXWI_SGI;
		} else if (IEEE80211_IS_CHAN_HT40(ic->ic_curchan)) {
			if (ni->ni_htcap & IEEE80211_HTCAP_SHORTGI40)
				rate_val |= MT76X2_TXWI_SGI;
		} else {
			if (ni->ni_htcap & IEEE80211_HTCAP_SHORTGI20)
				rate_val |= MT76X2_TXWI_SGI;
		}

		/*
		 * BW: match channel bandwidth.  TX40M_BLK_EN (set in
		 * init) defers frames on extension channel busy.
		 */
		if (mt76x2_bw_mode != 1) {
			if (IEEE80211_IS_CHAN_VHT80(ic->ic_curchan))
				rate_val |= (2 << MT76X2_TXWI_BW_SHIFT);
			else if (IEEE80211_IS_CHAN_HT40(ic->ic_curchan))
				rate_val |= (1 << MT76X2_TXWI_BW_SHIFT);
		}

		/* STBC: 1-stream rates only (HT MCS0-7 or VHT NSS=1) */
		if (is_vht) {
			if (MT76X2_NODE(ni)->vht_nss == 1 &&
			    sc->ntxchains > 1 &&
			    (ni->ni_vhtcap & IEEE80211_VHTCAP_RXSTBC_MASK))
				rate_val |= MT76X2_TXWI_STBC;
		} else if (ridx <= MT76X2_RIDX_MCS0 + 7 &&
		    sc->ntxchains > 1 &&
		    (ni->ni_htcap & IEEE80211_HTCAP_RXSTBC))
			rate_val |= MT76X2_TXWI_STBC;

		/* LDPC: check VHT or HT cap */
		if (is_vht) {
			if (ni->ni_vhtcap & IEEE80211_VHTCAP_RXLDPC)
				rate_val |= MT76X2_TXWI_LDPC;
		} else if (ni->ni_htcap & IEEE80211_HTCAP_LDPC)
			rate_val |= MT76X2_TXWI_LDPC;
	}

	txwi->rate = htole16(rate_val);

	/* TX stream selection for 2T2R (matches Linux mt76x02_mac.c) */
	txwi->txstream = MT76X2_TXWI_TXSTREAM_E4;

	/*
	 * Timestamp insertion: hardware writes TSF into probe responses
	 * and beacons when MT76X2_TXWI_TS is set.
	 */
	if (type == IEEE80211_FC0_TYPE_MGT &&
	    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
	    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
		txwi->flags |= htole16(MT76X2_TXWI_TS);

	/*
	 * SMPS: signal MMPS when peer uses dynamic SM power save
	 * and we're sending a multi-stream frame (HT MCS8+ or VHT NSS>1).
	 * Matches Linux mt76x02_mac_write_txwi() MMPS handling.
	 */
	if (((ridx >= MT76X2_RIDX_MCS0 + 8) ||
	    (ridx == MT76X2_RIDX_VHT && MT76X2_NODE(ni)->vht_nss > 1)) &&
	    (ni->ni_htcap & IEEE80211_HTCAP_SMPS) ==
	    IEEE80211_HTCAP_SMPS_DYNAMIC)
		txwi->flags |= htole16(MT76X2_TXWI_MMPS);

	/* Per-frame TX power adjustment from rate_power table */
	txwi->ctl2 = mt76x2_tx_get_txpwr_adj(sc, ridx) & 0x1f;

	/* ACK policy and WCID */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    type != IEEE80211_FC0_TYPE_CTL) {
		txwi->ack_ctl = MT76X2_TXWI_ACK;
		/* Set WCID for unicast data — hardware needs it for key lookup */
		if (type == IEEE80211_FC0_TYPE_DATA)
			txwi->wcid = MT76X2_NODE(ni)->wcid;
	}

	/*
	 * AMPDU aggregation — set TXWI AMPDU flag, BA window, NSEQ,
	 * and MPDU density when net80211 marks frame as A-MPDU.
	 * Matches Linux mt76x02_mac_write_txwi() AMPDU handling.
	 */
	if (m->m_flags & M_AMPDU_MPDU) {
		int tid = ieee80211_gettid(wh);
		struct ieee80211_tx_ampdu *tap = &ni->ni_tx_ampdu[tid];
		uint8_t ba_wnd, density;

		txwi->flags |= MT76X2_TXWI_AMPDU;

		/*
		 * Hardware assigns sequence numbers for AMPDU subframes.
		 * net80211 zeros wh->i_seq for M_AMPDU_MPDU frames.
		 */
		txwi->ack_ctl |= MT76X2_TXWI_NSEQ_CTL;

		/* Negotiated BA window size, capped to HW max (6-bit) */
		ba_wnd = (IEEE80211_AMPDU_RUNNING(tap) &&
		    tap->txa_wnd > 0) ?
		    MIN(tap->txa_wnd, MT76X2_TXWI_BA_WINDOW_MAX) :
		    MT76X2_TXWI_BA_WINDOW_MAX;
		txwi->ack_ctl |= (ba_wnd << MT76X2_TXWI_BA_WINDOW_SHIFT);

		/* MPDU density: clamp to min 4us per Linux mt76x02_mac.c */
		density = ieee80211_ht_get_node_ampdu_density(ni);
		if (density < IEEE80211_HTCAP_MPDUDENSITY_4)
			density = IEEE80211_HTCAP_MPDUDENSITY_4;
		txwi->flags |= ((uint16_t)density <<
		    MT76X2_TXWI_MPDU_DENSITY_SHIFT);
	}

	/*
	 * TXWI len_ctl = MPDU length excluding L2 padding.
	 * With IEEE80211_F_DATAPAD, net80211 inserts 2 bytes of padding
	 * after the 802.11 header when it's not 32-bit aligned (QoS).
	 * Matches if_run(4)/if_mtw(4)/Linux mt76.
	 */
	{
		uint8_t pad = 0;
		if (IEEE80211_HAS_ADDR4(wh) != IEEE80211_QOS_HAS_SEQ(wh))
			pad = 2;
		txwi->len_ctl = htole16(m->m_pkthdr.len - pad);
	}

	/*
	 * Set pktid for TX status tracking.  Matches Linux encoding:
	 * BIT(7) | rate_index[4:0] | AC[6:5].  Hardware echoes this
	 * in TX_STAT_FIFO_EXT.  Set to 0 for no-ack frames (multicast).
	 */
	if (txwi->ack_ctl & MT76X2_TXWI_ACK)
		txwi->pktid = MT76X2_PKTID_HAS_RATE |
		    (ridx & MT76X2_PKTID_RATE_MASK) |
		    ((qid & 0x3) << MT76X2_PKTID_AC_SHIFT);

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_XMIT,
	    "tx: len=%d type=0x%02x prot=%d wcid=%d qid=%d wiv=%d "
	    "ridx=%d pktid=0x%02x ampdu=%d\n",
	    m->m_pkthdr.len, type, !!(wh->i_fc[1] & IEEE80211_FC1_PROTECTED),
	    txwi->wcid, qid,
	    !(wh->i_fc[1] & IEEE80211_FC1_PROTECTED),
	    ridx, txwi->pktid,
	    !!(m->m_flags & M_AMPDU_MPDU));

	/*
	 * Build TXD (TXINFO) at offset 0.
	 * Length field = TXWI + frame data, rounded up to 4 bytes.
	 * Port = WLAN_PORT (0), flags = 80211 | QSEL_EDCA.
	 *
	 * WIV=0 for encrypted data frames: hardware adds IV and encrypts
	 * using the WCID key table.
	 * WIV=1 for unencrypted frames: hardware does not touch crypto.
	 */
	xfer_len = roundup(sizeof(struct mt76x2_txwi) + m->m_pkthdr.len, 4);
	txinfo = (xfer_len & 0xffff) |
	    MT76X2_TXD_80211 |
	    MT76X2_TXD_QSEL_EDCA |
	    ((uint32_t)MT76X2_WLAN_PORT << MT76X2_TXD_DPORT_SHIFT);
	if (!(wh->i_fc[1] & IEEE80211_FC1_PROTECTED))
		txinfo |= MT76X2_TXD_WIV;
	{
		uint32_t txd = htole32(txinfo);
		memcpy(data->desc, &txd, MT76X2_DMA_HDR_LEN);
	}

	data->m = m;
	data->ni = ni;

	STAILQ_INSERT_TAIL(&sc->sc_epq[qid].tx_qh, data, next);
	usbd_transfer_start(sc->sc_xfer[mt76x2_wme_ac_xfer_map[qid]]);

	return (0);
}

static void
mt76x2_start(struct mt76x2_softc *sc)
{
	struct ieee80211_node *ni;
	struct mbuf *m;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	if (!sc->sc_running)
		return;

	while ((m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		if (mt76x2_tx(sc, m, ni) != 0) {
			/* TX ring full, put it back */
			mbufq_prepend(&sc->sc_snd, m);
			break;
		}
	}
}

static int
mt76x2_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct mt76x2_softc *sc = ic->ic_softc;
	int error;

	MT76X2_LOCK(sc);
	if (!sc->sc_running) {
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
		    "raw_xmit: sc_running=0, dropping frame\n");
		MT76X2_UNLOCK(sc);
		m_freem(m);
		return (ENETDOWN);
	}

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "raw_xmit: len=%d type=0x%02x\n",
	    m->m_pkthdr.len,
	    mtod(m, struct ieee80211_frame *)->i_fc[0]);

	error = mt76x2_tx(sc, m, ni);
	MT76X2_UNLOCK(sc);

	/*
	 * On error, free the mbuf only.  The caller (ieee80211_raw_output)
	 * owns the node reference and will free it when we return non-zero.
	 * Freeing the node here would double-free the reference, causing
	 * premature node destruction and page faults on the destroyed PSQ
	 * mutex.
	 */
	if (error != 0)
		m_freem(m);

	return (error);
}

static int
mt76x2_transmit(struct ieee80211com *ic, struct mbuf *m)
{
	struct mt76x2_softc *sc = ic->ic_softc;
	int error;

	MT76X2_LOCK(sc);
	if (!sc->sc_running) {
		MT76X2_UNLOCK(sc);
		return (ENXIO);
	}
	error = mbufq_enqueue(&sc->sc_snd, m);
	if (error) {
		mt76x2_tx_queue_full++;
		MT76X2_UNLOCK(sc);
		return (error);
	}

	mt76x2_start(sc);
	MT76X2_UNLOCK(sc);
	return (0);
}

/*
 * ============================================================
 * USB bulk callbacks
 * ============================================================
 */

static void
mt76x2_bulk_tx_callbackN(struct usb_xfer *xfer, usb_error_t error, u_int idx)
{
	struct mt76x2_softc *sc = usbd_xfer_softc(xfer);
	struct mt76x2_endpoint_queue *pq = &sc->sc_epq[idx];
	struct mt76x2_tx_data *data;
	struct usb_page_cache *pc;
	struct mbuf *m;
	usb_frlength_t size, fsize;
	int i;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		mt76x2_tx_complete++;
		/* Free all frames in the aggregated batch */
		for (i = 0; i < pq->tx_agg_cnt; i++) {
			data = pq->tx_agg[i];
			if (data != NULL) {
				sc->tx_total++;
				mt76x2_tx_free(pq, data, 0);
				pq->tx_agg[i] = NULL;
			}
		}
		pq->tx_agg_cnt = 0;
		usbd_xfer_set_priv(xfer, NULL);

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		/* Don't submit new USB TX during channel switch */
		if (sc->sc_chan_changing)
			break;

		data = STAILQ_FIRST(&pq->tx_qh);
		if (data == NULL)
			break;

		/*
		 * TX aggregation: pack multiple frames into one USB
		 * bulk transfer.  The firmware parses concatenated
		 * [TXD+TXWI+frame+pad] blocks using the NEXT_VLD
		 * bit (TXD bit 16) to know more frames follow.
		 *
		 * This is the only way to improve throughput on
		 * FreeBSD, which serializes 1 USB transfer per
		 * endpoint.  Confirmed by MediaTek vendor driver.
		 */
		/*
		 * Collect up to MT76X2_TX_AGG_COUNT frames, then
		 * decide which ones get NEXT_VLD before copying.
		 */
		pq->tx_agg_cnt = 0;
		size = 0;

		while (data != NULL &&
		    pq->tx_agg_cnt < MT76X2_TX_AGG_COUNT) {
			m = data->m;
			/* Per-frame size in USB buffer */
			fsize = roundup(MT76X2_DMA_HDR_LEN +
			    sizeof(struct mt76x2_txwi) +
			    m->m_pkthdr.len, 4);
			if (size + fsize + 4 > MT76X2_MAX_TXSZ &&
			    pq->tx_agg_cnt > 0)
				break;
			STAILQ_REMOVE_HEAD(&pq->tx_qh, next);
			pq->tx_agg[pq->tx_agg_cnt++] = data;
			size += fsize;
			data = STAILQ_FIRST(&pq->tx_qh);
		}

		if (pq->tx_agg_cnt == 0)
			break;

		/*
		 * Copy frames into USB buffer.  Set NEXT_VLD in the
		 * TXD of all frames except the last.
		 */
		pc = usbd_xfer_get_frame(xfer, 0);
		size = 0;
		for (i = 0; i < pq->tx_agg_cnt; i++) {
			uint32_t txd;

			data = pq->tx_agg[i];
			m = data->m;

			/* Set NEXT_VLD in TXD for all but last frame */
			memcpy(&txd, data->desc, 4);
			if (i < pq->tx_agg_cnt - 1)
				txd |= htole32(MT76X2_TXD_NEXT_VLD);
			usbd_copy_in(pc, size, &txd, MT76X2_DMA_HDR_LEN);

			/* Copy TXWI (bytes 4..23 of desc) */
			usbd_copy_in(pc, size + MT76X2_DMA_HDR_LEN,
			    data->desc + MT76X2_DMA_HDR_LEN,
			    sizeof(struct mt76x2_txwi));

			/* Copy 802.11 frame */
			usbd_m_copy_in(pc, size + MT76X2_DMA_HDR_LEN +
			    sizeof(struct mt76x2_txwi),
			    m, 0, m->m_pkthdr.len);

			/* Compute frame size, pad to 4 bytes */
			fsize = MT76X2_DMA_HDR_LEN +
			    sizeof(struct mt76x2_txwi) +
			    m->m_pkthdr.len;
			if (fsize & 3) {
				usbd_frame_zero(pc, size + fsize,
				    4 - (fsize & 3));
				fsize = roundup(fsize, 4);
			}

			size += fsize;
		}

		/* 4-byte zero terminator after last frame */
		usbd_frame_zero(pc, size, 4);
		size += 4;

		usbd_xfer_set_frame_len(xfer, 0, size);
		usbd_xfer_set_priv(xfer, pq->tx_agg[0]);
		mt76x2_tx_submit++;
		usbd_transfer_submit(xfer);

		mt76x2_start(sc);
		break;

	default:	/* error */
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_XMIT,
		    "TX[%u] transfer error: %s\n", idx,
		    usbd_errstr(error));

		if (error != USB_ERR_CANCELLED)
			counter_u64_add(sc->sc_ic.ic_oerrors, 1);

		/* Free all frames in the failed batch */
		for (i = 0; i < pq->tx_agg_cnt; i++) {
			data = pq->tx_agg[i];
			if (data != NULL) {
				mt76x2_tx_free(pq, data, error);
				pq->tx_agg[i] = NULL;
			}
		}
		pq->tx_agg_cnt = 0;
		usbd_xfer_set_priv(xfer, NULL);

		if (error != USB_ERR_CANCELLED) {
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		break;
	}
}

static void
mt76x2_bulk_tx_callback0(struct usb_xfer *xfer, usb_error_t error)
{
	mt76x2_bulk_tx_callbackN(xfer, error, 0);
}

static void
mt76x2_bulk_tx_callback1(struct usb_xfer *xfer, usb_error_t error)
{
	mt76x2_bulk_tx_callbackN(xfer, error, 1);
}

static void
mt76x2_bulk_tx_callback2(struct usb_xfer *xfer, usb_error_t error)
{
	mt76x2_bulk_tx_callbackN(xfer, error, 2);
}

static void
mt76x2_bulk_tx_callback3(struct usb_xfer *xfer, usb_error_t error)
{
	mt76x2_bulk_tx_callbackN(xfer, error, 3);
}

static void
mt76x2_bulk_tx_callback4(struct usb_xfer *xfer, usb_error_t error)
{
	mt76x2_bulk_tx_callbackN(xfer, error, 4);
}

static void
mt76x2_bulk_tx_callback5(struct usb_xfer *xfer, usb_error_t error)
{
	mt76x2_bulk_tx_callbackN(xfer, error, 5);
}

static void
mt76x2_fw_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct mt76x2_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_FIRMWARE,
		    "FW bulk transfer complete\n");
		sc->fw_chunk_done = 1;
		wakeup(&sc->fw_chunk_done);
		break;

	case USB_ST_SETUP:
		if (sc->fw_buf != NULL && sc->fw_buflen > 0) {
			MT76X2_DPRINTF(sc, MT76X2_DEBUG_FIRMWARE,
			    "FW submitting %d bytes\n", sc->fw_buflen);
			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_in(pc, 0, sc->fw_buf, sc->fw_buflen);
			usbd_xfer_set_frame_len(xfer, 0, sc->fw_buflen);
			usbd_xfer_set_frames(xfer, 1);
			usbd_transfer_submit(xfer);
		}
		break;

	default:	/* error */
		device_printf(sc->sc_dev,
		    "FW transfer error: %s\n", usbd_errstr(error));
		sc->fw_error = EIO;
		sc->fw_chunk_done = 1;
		wakeup(&sc->fw_chunk_done);
		break;
	}
}

/*
 * MCU command response callback.
 *
 * The firmware sends responses to MCU commands on the second bulk IN
 * endpoint (ep_index 1).  After sending each MCU command, mcu_cmd()
 * waits for the response by sleeping on mcu_resp_done.  This callback
 * signals that a response arrived, allowing the next command to proceed.
 *
 * This matches Linux mt76x02u_mcu_wait_resp() which synchronously
 * reads each response before sending the next command.  Without this,
 * the firmware's response buffer fills up after ~40 commands, causing
 * subsequent MCU commands to stall and eventually crashing the device.
 */
static void
mt76x2_mcu_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct mt76x2_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	uint32_t rxfce;
	uint8_t resp_seq, resp_evt;
	int actlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
		    "MCU resp: %d bytes\n", actlen);

		/*
		 * Validate the response matches our command.
		 * The first 4 bytes are FCE_INFO containing the sequence
		 * number (bits [19:16]) and event type (bits [23:20]).
		 * EVT_CMD_DONE = 0.  Non-matching responses (unsolicited
		 * firmware events like DFS/carrier detect) must be drained
		 * by resubmitting, up to 5 retries.  Matches Linux
		 * mt76x02u_mcu_wait_resp().
		 */
		if (actlen >= 4) {
			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_out(pc, 0, &rxfce, sizeof(rxfce));
			rxfce = le32toh(rxfce);

			resp_seq = (rxfce >> 16) & 0xf;
			resp_evt = (rxfce >> 20) & 0xf;

			MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
			    "MCU resp: seq=%d evt=%d (expect seq=%d)\n",
			    resp_seq, resp_evt, sc->mcu_seq);

			if (resp_seq == sc->mcu_seq && resp_evt == 0) {
				/*
				 * Matching CMD_DONE response.
				 * Copy full response data for commands
				 * that need it (e.g., CMD_RANDOM_READ).
				 */
				sc->mcu_resp_len = min(actlen,
				    (int)sizeof(sc->mcu_resp_buf));
				usbd_copy_out(pc, 0, sc->mcu_resp_buf,
				    sc->mcu_resp_len);
				sc->mcu_resp_done = 1;
				wakeup(&sc->mcu_resp_done);
			} else {
				/* Non-matching — drain and read next */
				sc->mcu_resp_retries++;
				if (sc->mcu_resp_retries < 5) {
					usbd_xfer_set_frame_len(xfer, 0, 512);
					usbd_xfer_set_frames(xfer, 1);
					usbd_transfer_submit(xfer);
				} else {
					MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
					    "MCU resp: 5 retries exhausted\n");
					sc->mcu_resp_done = 1;
					wakeup(&sc->mcu_resp_done);
				}
			}
		} else {
			/* Runt response — accept it */
			sc->mcu_resp_done = 1;
			wakeup(&sc->mcu_resp_done);
		}
		break;

	case USB_ST_SETUP:
		usbd_xfer_set_frame_len(xfer, 0, 512);
		usbd_xfer_set_frames(xfer, 1);
		usbd_transfer_submit(xfer);
		break;

	default:	/* error */
		if (error != USB_ERR_CANCELLED) {
			MT76X2_DPRINTF(sc, MT76X2_DEBUG_CMD,
			    "MCU RX error: %s\n", usbd_errstr(error));
			sc->mcu_resp_done = 1;
			wakeup(&sc->mcu_resp_done);
		}
		break;
	}
}

static void
mt76x2_rx_frame(struct mt76x2_softc *sc, uint8_t *buf, int dmalen)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;
	struct mt76x2_rxwi *rxwi;
	uint32_t rxinfo, ctl;
	int len, rssi;

	/*
	 * Per-frame format: [RXWI (32 bytes)][802.11 frame][FCE_INFO (4 bytes)]
	 * dmalen = total payload from DMA header (RXWI + frame)
	 * Metadata (CRC, L2PAD, MPDU len) is in the RXWI, not the trailer.
	 */

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_RECV,
	    "rx_frame: dmalen=%d\n", dmalen);

	/* (RX debug removed — AUTH response confirmed received) */

	if (dmalen < (int)sizeof(struct mt76x2_rxwi)) {
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}

	rxwi = (struct mt76x2_rxwi *)buf;
	rxinfo = le32toh(rxwi->rxinfo);
	ctl = le32toh(rxwi->ctl);

	/* Check CRC error in RXWI rxinfo (pass in monitor mode) */
	if ((rxinfo & MT76X2_RXINFO_CRC_ERR) && !sc->sc_monitor) {
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_RECV, "RX CRC error\n");
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}

	/* Drop frames with ICV or MIC errors (pass in monitor mode) */
	if ((rxinfo & (MT76X2_RXINFO_ICVERR | MT76X2_RXINFO_MICERR)) &&
	    !sc->sc_monitor) {
		MT76X2_DPRINTF(sc, MT76X2_DEBUG_RECV,
		    "RX decrypt error (ICV/MIC)\n");
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}

	/*
	 * MPDU length from RXWI ctl field, bits [29:16].
	 * This is the 802.11 frame length (without RXWI or L2PAD).
	 */
	len = (ctl & MT76X2_RXWI_CTL_MPDU_LEN_MASK) >>
	    MT76X2_RXWI_CTL_MPDU_LEN_SHIFT;

	/*
	 * When L2PAD is set, the hardware inserted 2 bytes of padding
	 * after the 802.11 header to align the payload to a 4-byte
	 * boundary.  MPDU_LEN does not include the padding, so we must
	 * add 2 to account for the extra bytes in the buffer.
	 *
	 * We do NOT strip the padding here — because IEEE80211_F_DATAPAD
	 * is set on ic_flags, ieee80211_hdrspace() in the net80211 stack
	 * will account for it when decapsulating the frame.  This matches
	 * how if_run(4) handles L2PAD.
	 */
	if (rxinfo & MT76X2_RXINFO_L2PAD)
		len += 2;

	if (len == 0 || len > dmalen - (int)sizeof(struct mt76x2_rxwi))
		len = dmalen - (int)sizeof(struct mt76x2_rxwi);

	if (len < (int)sizeof(struct ieee80211_frame_min)) {
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}

	/* Allocate mbuf for the 802.11 frame (after RXWI) */
	m = m_get2(len, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		counter_u64_add(ic->ic_ierrors, 1);
		return;
	}

	memcpy(mtod(m, uint8_t *), buf + sizeof(struct mt76x2_rxwi), len);
	m->m_pkthdr.len = m->m_len = len;

	/*
	 * Handle hardware-decrypted frames.  When RXINFO_DECRYPT is set,
	 * the hardware has decrypted the frame body, verified/stripped the
	 * MIC, but left the CCMP header (IV, 8 bytes) in the buffer after
	 * the 802.11 header.  We must strip the CCMP header so that
	 * net80211 finds the LLC/SNAP header at the correct offset.
	 *
	 * Linux mt76x02_mac_process_rx() does the same via:
	 *   pn_len = FIELD_GET(MT_RXINFO_PN_LEN, rxinfo);
	 *   pad_len += pn_len << 2;  len -= pn_len << 2;
	 *   mt76x02_remove_hdr_pad(skb, pad_len);
	 *
	 * We keep L2PAD intact since IEEE80211_F_DATAPAD is set and
	 * net80211 expects it.
	 */
	{
		struct ieee80211_frame *wh;
		wh = mtod(m, struct ieee80211_frame *);

		if (rxinfo & MT76X2_RXINFO_DECRYPT) {
			int pn_len, iv_len, hdrlen, l2pad, keep;

			pn_len = (rxinfo & MT76X2_RXINFO_PN_LEN_MASK) >>
			    MT76X2_RXINFO_PN_LEN_SHIFT;
			iv_len = pn_len << 2;	/* CCMP: 2*4 = 8 bytes */

			if (iv_len > 0 && iv_len < m->m_len) {
				hdrlen = ieee80211_hdrsize(wh);
				l2pad = (rxinfo & MT76X2_RXINFO_L2PAD) ? 2 : 0;
				keep = hdrlen + l2pad;

				if (keep + iv_len <= m->m_len) {
					memmove(mtod(m, uint8_t *) + iv_len,
					    mtod(m, uint8_t *), keep);
					m_adj(m, iv_len);
				}
			}

			/* Refresh wh after possible memmove + m_adj */
			wh = mtod(m, struct ieee80211_frame *);
			wh->i_fc[1] &= ~IEEE80211_FC1_PROTECTED;
			m->m_flags |= M_WEP;
		} else if ((wh->i_fc[1] & IEEE80211_FC1_PROTECTED) &&
		    !sc->sc_monitor) {
			/*
			 * Encrypted frame that hardware didn't decrypt.
			 * This is a frame from an AP we have no key for
			 * (e.g., neighboring encrypted networks).  Drop it
			 * to prevent net80211 from triggering spurious
			 * MIC failure events that block association via
			 * TKIP countermeasures.  Pass in monitor mode.
			 */
			MT76X2_DPRINTF(sc, MT76X2_DEBUG_RECV,
			    "rx: dropping undecrypted frame "
			    "(rxinfo=0x%08x)\n", rxinfo);
			counter_u64_add(ic->ic_ierrors, 1);
			m_freem(m);
			return;
		}
	}

	/*
	 * Calibrate RSSI: apply per-chain EEPROM offset and LNA gain.
	 * Use MAX of both chains for 2x2 MIMO, matching Linux
	 * mt76x02_mac_process_rx() signal selection.
	 *
	 * FreeBSD net80211 convention (see ifconfig's scan display):
	 *   signal_dBm = (rssi / 2) + nf
	 * So we convert our calibrated dBm to the raw RSSI scale:
	 *   rssi = 2 * (calibrated_dBm - nf)
	 * Noise floor: -95 dBm for 2.4 GHz, -98 dBm for 5 GHz.
	 */
	{
		int rssi0, rssi1, sig, nf;

		rssi0 = mt76x2_get_rssi(sc, (int8_t)rxwi->rssi[0], 0);
		rssi1 = mt76x2_get_rssi(sc, (int8_t)rxwi->rssi[1], 1);
		sig = (rssi0 > rssi1) ? rssi0 : rssi1;
		nf = IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ? -98 : -95;

		/* Convert dBm to net80211 raw RSSI scale (0.5 dBm units) */
		rssi = 2 * (sig - nf);
		if (rssi < 0)
			rssi = 0;
		if (rssi > 127)
			rssi = 127;

		{
			struct ieee80211_frame *rwh;
			rwh = mtod(m, struct ieee80211_frame *);
			MT76X2_DPRINTF(sc, MT76X2_DEBUG_RECV,
			    "rx: len=%d fc=0x%02x%02x rssi=%d\n",
			    m->m_pkthdr.len,
			    rwh->i_fc[0], rwh->i_fc[1], rssi);
		}

		/* Fill radiotap header for monitor mode / packet capture */
		{
			uint16_t rxrate;
			uint8_t phy, mcs, flags_rt;

			rxrate = le16toh(rxwi->rate);
			phy = (rxrate & MT76X2_RXWI_PHY_MASK) >>
			    MT76X2_RXWI_PHY_SHIFT;
			mcs = (rxrate & MT76X2_RXWI_MCS_MASK) >>
			    MT76X2_RXWI_MCS_SHIFT;
			flags_rt = 0;
			if (rxinfo & MT76X2_RXINFO_CRC_ERR)
				flags_rt |= IEEE80211_RADIOTAP_F_BADFCS;
			if (rxrate & MT76X2_RXWI_SGI)
				flags_rt |= IEEE80211_RADIOTAP_F_SHORTGI;

			sc->sc_rxtap.wr_flags = flags_rt;
			sc->sc_rxtap.wr_dbm_antsignal = sig;
			sc->sc_rxtap.wr_antenna = 0;
			sc->sc_rxtap.wr_antsignal = rssi;
			sc->sc_rxtap.wr_chan_freq =
			    htole16(ic->ic_curchan->ic_freq);
			sc->sc_rxtap.wr_chan_flags =
			    htole16(ic->ic_curchan->ic_flags);

			/* Convert PHY+MCS to radiotap rate */
			switch (phy) {
			case MT76X2_TXWI_PHY_CCK:
				if (mcs < 4) {
					static const uint8_t cck_rates[] =
					    { 2, 4, 11, 22 };
					sc->sc_rxtap.wr_rate =
					    cck_rates[mcs];
				}
				sc->sc_rxtap.wr_vht_known = 0;
				break;
			case MT76X2_TXWI_PHY_OFDM:
				if (mcs < 8) {
					static const uint8_t ofdm_rates[] =
					    { 12, 18, 24, 36, 48, 72,
					      96, 108 };
					sc->sc_rxtap.wr_rate =
					    ofdm_rates[mcs];
				}
				sc->sc_rxtap.wr_vht_known = 0;
				break;
			case MT76X2_TXWI_PHY_VHT: {
				/*
				 * VHT: RXWI MCS [3:0]=MCS, [5:4]=NSS-1.
				 * Rate field is 0; VHT radiotap carries it.
				 */
				uint8_t vht_mcs = mcs & 0x0f;
				uint8_t vht_nss = ((mcs >> 4) & 0x03) + 1;
				uint8_t bw = (rxrate >>
				    MT76X2_RXWI_BW_SHIFT) & 0x3;

				sc->sc_rxtap.wr_rate = 0;
				sc->sc_rxtap.wr_vht_known = htole16(
				    IEEE80211_RADIOTAP_VHT_HAVE_STBC |
				    IEEE80211_RADIOTAP_VHT_HAVE_GI |
				    IEEE80211_RADIOTAP_VHT_HAVE_BW);
				sc->sc_rxtap.wr_vht_flags = 0;
				if (rxrate & MT76X2_RXWI_SGI)
					sc->sc_rxtap.wr_vht_flags |=
					    IEEE80211_RADIOTAP_VHT_SGI;
				if (rxrate & MT76X2_RXWI_STBC)
					sc->sc_rxtap.wr_vht_flags |=
					    IEEE80211_RADIOTAP_VHT_STBC;
				if (bw == 2)
					sc->sc_rxtap.wr_vht_bandwidth =
					    IEEE80211_RADIOTAP_VHT_BW_80;
				else if (bw == 1)
					sc->sc_rxtap.wr_vht_bandwidth =
					    IEEE80211_RADIOTAP_VHT_BW_40;
				else
					sc->sc_rxtap.wr_vht_bandwidth =
					    IEEE80211_RADIOTAP_VHT_BW_20;
				sc->sc_rxtap.wr_vht_mcs_nss[0] =
				    (vht_nss & IEEE80211_RADIOTAP_VHT_NSS) |
				    ((vht_mcs << 4) &
				    IEEE80211_RADIOTAP_VHT_MCS);
				memset(&sc->sc_rxtap.wr_vht_mcs_nss[1], 0, 3);
				sc->sc_rxtap.wr_vht_coding =
				    (rxrate & MT76X2_RXWI_LDPC) ?
				    IEEE80211_RADIOTAP_VHT_CODING_LDPC : 0;
				sc->sc_rxtap.wr_vht_group_id = 0;
				sc->sc_rxtap.wr_vht_partial_aid = 0;
				break;
			}
			default:
				/* HT: rate field = MCS index | 0x80 */
				sc->sc_rxtap.wr_rate = mcs | 0x80;
				sc->sc_rxtap.wr_vht_known = 0;
				break;
			}
		}

		MT76X2_UNLOCK(sc);

		ni = ieee80211_find_rxnode(ic,
		    mtod(m, struct ieee80211_frame_min *));
		if (ni != NULL) {
			(void)ieee80211_input(ni, m, rssi, nf);
			ieee80211_free_node(ni);
		} else {
			(void)ieee80211_input_all(ic, m, rssi, nf);
		}
	}

	MT76X2_LOCK(sc);
}

static void
mt76x2_bulk_rx_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct mt76x2_softc *sc = usbd_xfer_softc(xfer);
	uint8_t *buf;
	uint32_t dmalen;
	int totlen;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (sc->sc_detached)
			goto tr_setup;

		usbd_xfer_status(xfer, &totlen, NULL, NULL, NULL);

		MT76X2_DPRINTF(sc, MT76X2_DEBUG_RECV,
		    "RX xfer: totlen=%d rx_m=%p\n", totlen,
		    sc->rx_m);

		if (sc->rx_m == NULL)
			goto tr_setup;

		buf = mtod(sc->rx_m, uint8_t *);

		/*
		 * USB RX frames have a 4-byte DMA header per sub-frame.
		 * Format: [4-byte DMA_LEN][RXWI + frame = dmalen bytes]
		 *         [4-byte RXD]
		 * Total per sub-frame: dmalen + 8, aligned to 4 bytes.
		 * Multiple sub-frames may be aggregated in one USB xfer.
		 * Matches if_run.c and Linux mt76x02u_rx_next_len().
		 */
		while (totlen > 4) {
			dmalen = le32toh(*(uint32_t *)buf) & 0xffff;
			if (dmalen == 0 || (int)(dmalen + 8) > totlen)
				break;

			MT76X2_DPRINTF(sc, MT76X2_DEBUG_RECV,
			    "RX agg: dmalen=%u totlen=%d\n", dmalen, totlen);

			/*
			 * Skip status packets (dmalen too small for
			 * RXWI + minimum 802.11 frame).
			 */
			if (dmalen >= sizeof(struct mt76x2_rxwi) +
			    sizeof(struct ieee80211_frame_min))
				mt76x2_rx_frame(sc, buf + 4, dmalen);

			/* Advance to next aggregated frame */
			dmalen = roundup2(dmalen, 4);
			buf += dmalen + 8;
			totlen -= dmalen + 8;
		}

		/* Free the RX mbuf — individual frames got their own */
		m_freem(sc->rx_m);
		sc->rx_m = NULL;
		/* FALLTHROUGH */

	case USB_ST_SETUP:
tr_setup:
		if (sc->rx_m == NULL) {
			sc->rx_m = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR,
			    MT76X2_MAX_RXSZ);
		}
		if (sc->rx_m == NULL) {
			MT76X2_DPRINTF(sc, MT76X2_DEBUG_RECV,
			    "could not allocate RX mbuf\n");
			usbd_xfer_set_stall(xfer);
			usbd_xfer_set_frames(xfer, 0);
		} else {
			usbd_xfer_set_frame_data(xfer, 0,
			    mtod(sc->rx_m, caddr_t), MT76X2_MAX_RXSZ);
			usbd_xfer_set_frames(xfer, 1);
		}
		usbd_transfer_submit(xfer);
		break;

	default:	/* error */
		if (error != USB_ERR_CANCELLED) {
			MT76X2_DPRINTF(sc, MT76X2_DEBUG_USB,
			    "RX error: %s\n", usbd_errstr(error));
			usbd_xfer_set_stall(xfer);
			goto tr_setup;
		}
		if (sc->rx_m != NULL) {
			m_freem(sc->rx_m);
			sc->rx_m = NULL;
		}
		break;
	}
}

/*
 * ============================================================
 * Miscellaneous ic callbacks
 * ============================================================
 */

static void
mt76x2_update_mcast(struct ieee80211com *ic)
{
	/* Accept all multicast for now */
}

static void
mt76x2_update_promisc_locked(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t filter;

	/*
	 * Toggle DROP_UA (unicast not to me) for promiscuous mode.
	 * Never use DROP_NOT_MYBSS — matching if_run/if_mtw/Linux mt76.
	 */
	filter = sc->rxfilter;
	if (ic->ic_promisc > 0)
		filter &= ~MT76X2_RX_FILTR_DROP_UA;
	else
		filter |= MT76X2_RX_FILTR_DROP_UA;

	mt76x2_write(sc, MT76X2_RX_FILTR_CFG, filter);
}

static void
mt76x2_update_promisc(struct ieee80211com *ic)
{
	struct mt76x2_softc *sc = ic->ic_softc;

	MT76X2_LOCK(sc);
	mt76x2_update_promisc_locked(sc);
	MT76X2_UNLOCK(sc);
}

/*
 * Set RX filter for monitor mode: accept all frames including
 * those with CRC errors, PHY errors, and non-matching BSSID.
 * This lets packet capture tools see raw wireless traffic.
 */
static void
mt76x2_set_monitor_filter(struct mt76x2_softc *sc)
{

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * Clear all drop bits to pass everything.
	 * Keep DROP_VER_ERR to avoid malformed frame processing.
	 */
	mt76x2_write(sc, MT76X2_RX_FILTR_CFG,
	    MT76X2_RX_FILTR_DROP_VER_ERR);
}

static int
mt76x2_wme_update(struct ieee80211com *ic)
{
	struct mt76x2_softc *sc = ic->ic_softc;
	struct chanAccParams chp;
	const struct wmeParams *ac;
	int aci, error = 0;

	ieee80211_wme_ic_getparams(ic, &chp);
	ac = chp.cap_wmeParams;

	MT76X2_LOCK(sc);
	/* Per-AC EDCA configuration registers */
	for (aci = 0; aci < WME_NUM_AC; aci++) {
		error = mt76x2_write(sc, MT76X2_EDCA_CFG_AC(aci),
		    ac[aci].wmep_logcwmax << 16 |
		    ac[aci].wmep_logcwmin << 12 |
		    ac[aci].wmep_aifsn    <<  8 |
		    ac[aci].wmep_txopLimit);
		if (error)
			goto done;
	}
	/* SCH/DMA WMM registers (redundant, written for compatibility) */
	mt76x2_write(sc, MT76X2_WMM_AIFSN,
	    ac[WME_AC_VO].wmep_aifsn  << 12 |
	    ac[WME_AC_VI].wmep_aifsn  <<  8 |
	    ac[WME_AC_BK].wmep_aifsn  <<  4 |
	    ac[WME_AC_BE].wmep_aifsn);
	mt76x2_write(sc, MT76X2_WMM_CWMIN,
	    ac[WME_AC_VO].wmep_logcwmin << 12 |
	    ac[WME_AC_VI].wmep_logcwmin <<  8 |
	    ac[WME_AC_BK].wmep_logcwmin <<  4 |
	    ac[WME_AC_BE].wmep_logcwmin);
	mt76x2_write(sc, MT76X2_WMM_CWMAX,
	    ac[WME_AC_VO].wmep_logcwmax << 12 |
	    ac[WME_AC_VI].wmep_logcwmax <<  8 |
	    ac[WME_AC_BK].wmep_logcwmax <<  4 |
	    ac[WME_AC_BE].wmep_logcwmax);
	mt76x2_write(sc, MT76X2_WMM_TXOP0,
	    ac[WME_AC_BK].wmep_txopLimit << 16 |
	    ac[WME_AC_BE].wmep_txopLimit);
	mt76x2_write(sc, MT76X2_WMM_TXOP1,
	    ac[WME_AC_VO].wmep_txopLimit << 16 |
	    ac[WME_AC_VI].wmep_txopLimit);
done:
	MT76X2_UNLOCK(sc);
	return (error);
}

static void
mt76x2_updateslot_cb(void *arg)
{
	struct mt76x2_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t val, slottime;

	/*
	 * Slot time goes in BKOFF_SLOT_CFG (0x1104) bits [7:0].
	 * NOT XIFS_TIME_CFG — that register holds SIFS/EIFS timing.
	 * Matches Linux mt76_rmw_field(MT_BKOFF_SLOT_CFG, SLOTTIME).
	 */
	slottime = (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;
	mt76x2_read(sc, 0x1104, &val);
	val = (val & ~0xffU) | slottime;
	mt76x2_write(sc, 0x1104, val);
}

static void
mt76x2_updateslot(struct ieee80211com *ic)
{
	struct mt76x2_softc *sc = ic->ic_softc;

	MT76X2_LOCK(sc);
	mt76x2_updateslot_cb(sc);
	MT76X2_UNLOCK(sc);
}

static void
mt76x2_update_chw(struct ieee80211com *ic)
{
	struct mt76x2_softc *sc = ic->ic_softc;

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "update_chw: reconfiguring channel width\n");

	mt76x2_set_channel(ic);
}

static int
mt76x2_ampdu_enable(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap)
{
	struct mt76x2_softc *sc = ni->ni_ic->ic_softc;

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "ampdu_enable: tid=%d flags=0x%x\n",
	    tap->txa_tid, tap->txa_flags);
	return (1);
}

static int
mt76x2_addba_request(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap,
    int dialogtoken, int baparamset, int batimeout)
{
	struct mt76x2_softc *sc = ni->ni_ic->ic_softc;

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "addba_request: tid=%d token=%d baparamset=0x%x\n",
	    tap->txa_tid, dialogtoken, baparamset);
	return sc->sc_addba_request(ni, tap, dialogtoken, baparamset,
	    batimeout);
}

static int
mt76x2_addba_response(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap,
    int status, int baparamset, int batimeout)
{
	struct mt76x2_softc *sc = ni->ni_ic->ic_softc;

	MT76X2_DPRINTF(sc, MT76X2_DEBUG_STATE,
	    "addba_response: tid=%d status=%d baparamset=0x%x wnd=%d\n",
	    tap->txa_tid, status, baparamset, tap->txa_wnd);
	return sc->sc_addba_response(ni, tap, status, baparamset, batimeout);
}

static void
mt76x2_addba_stop(struct ieee80211_node *ni,
    struct ieee80211_tx_ampdu *tap)
{
	struct mt76x2_softc *sc = ni->ni_ic->ic_softc;

	/* Default clears AGGR_RUNNING */
	sc->sc_addba_stop(ni, tap);
}

/*
 * ============================================================
 * TX/RX enable/disable
 * ============================================================
 */

static int
mt76x2_txrx_enable(struct mt76x2_softc *sc)
{
	uint32_t tmp;
	int ntries;

	/*
	 * Matches Linux mt76x02u_mac_start():
	 *   1. Enable TX only first
	 *   2. Wait for WPDMA idle
	 *   3. Set RX filter
	 *   4. Enable TX+RX
	 *   5. Wait for WPDMA idle again
	 *
	 * Linux does NOT write to 0x0238 (PCI WPDMA DMA_CFG) here.
	 * USB DMA is configured via 0x9018 (CFG space) in usb_dma_setup().
	 */

	/*
	 * Reset MAC stat counters — matches Linux mt76x02_mac_reset_counters().
	 * Reading these registers clears them.
	 */
	mt76x2_read(sc, 0x1700, &tmp);	/* RX_STAT_0 */
	mt76x2_read(sc, 0x1704, &tmp);	/* RX_STAT_1 */
	mt76x2_read(sc, 0x1708, &tmp);	/* RX_STAT_2 */
	mt76x2_read(sc, MT76X2_TX_STA_CNT0, &tmp);
	mt76x2_read(sc, MT76X2_TX_STA_CNT1, &tmp);
	mt76x2_read(sc, MT76X2_TX_STA_CNT2, &tmp);

	/* Flush TX_STAT_FIFO — matches Linux mt76x02_mac_reset_counters */
	{
		int j;
		for (j = 0; j < 16; j++)
			mt76x2_read(sc, MT76X2_TX_STAT_FIFO, &tmp);
	}

	/* Enable TX only first (Linux mt76x02u_mac_start step 1) */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL, MT76X2_MAC_ENABLE_TX);

	/* Wait for WPDMA TX/RX not busy (Linux wait_for_wpdma, 200000 loops) */
	for (ntries = 0; ntries < 1000; ntries++) {
		mt76x2_read(sc, MT76X2_WPDMA_GLO_CFG, &tmp);
		if (!(tmp & (MT76X2_WPDMA_TX_DMA_BUSY |
		    MT76X2_WPDMA_RX_DMA_BUSY)))
			break;
		DELAY(200);
	}

	/*
	 * Set RX filter — matches Linux init table value 0x00015f97.
	 * This drops CRC/PHY errors, dupes, control frames, etc.
	 * but accepts data, management, and beacons from other BSSes.
	 */
	/*
	 * Use Linux init table default filter: accept data + mgmt,
	 * drop CRC/PHY errors, duplicates, control frames from other BSS.
	 * Filter value 0 (accept all) may not work on all firmware.
	 */
	sc->rxfilter = MT76X2_RX_FILTR_CFG_DEFAULT;
	mt76x2_write(sc, MT76X2_RX_FILTR_CFG, sc->rxfilter);

	/* Enable both TX+RX MAC */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL,
	    MT76X2_MAC_ENABLE_TX | MT76X2_MAC_ENABLE_RX);

	/* Brief WPDMA settle wait (Linux wait_for_wpdma, 50 loops) */
	for (ntries = 0; ntries < 50; ntries++) {
		mt76x2_read(sc, MT76X2_WPDMA_GLO_CFG, &tmp);
		if (!(tmp & (MT76X2_WPDMA_TX_DMA_BUSY |
		    MT76X2_WPDMA_RX_DMA_BUSY)))
			break;
		DELAY(200);
	}

	/*
	 * FCE PDMA config — read-modify-write to preserve firmware-set
	 * bits (bit 0 and bit 30).  Overwriting with 0x44 clears these
	 * bits, which kills MCU responses on the USB IN endpoint.
	 */
	mt76x2_read(sc, MT76X2_FCE_PDMA_GLOBAL_CONF, &tmp);
	tmp |= 0x44;
	mt76x2_write(sc, MT76X2_FCE_PDMA_GLOBAL_CONF, tmp);
	mt76x2_write(sc, MT76X2_FCE_SKIP_FS, 0x03);

	/*
	 * Do NOT write to 0x0238 (MAC space USB_DMA_CFG).
	 * Linux mt76x2 never touches this register — only mt76x0 does.
	 * USB DMA is fully configured via 0x9018 (CFG space) above.
	 * Writing to 0x0238 may misconfigure the internal PCI/WPDMA
	 * DMA engine, preventing received frames from reaching USB.
	 */

	/* Start USB RX transfers */
	usbd_transfer_start(sc->sc_xfer[MT76X2_BULK_RX]);

	return (0);
}

static void
mt76x2_drain_mbufq(struct mt76x2_softc *sc)
{
	struct mbuf *m;
	struct ieee80211_node *ni;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);
	while ((m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		m->m_pkthdr.rcvif = NULL;
		ieee80211_free_node(ni);
		m_freem(m);
	}
}

/*
 * ============================================================
 * Init / stop
 * ============================================================
 */

/*
 * One-time hardware init — called from attach after firmware load.
 * Matches Linux mt76x2u_init_hardware(): MAC reset, default register
 * writes, WCID/key table clears, MCU calibration register load, and
 * PHY chain configuration.  These are expensive over USB (~100+ vendor
 * requests) and only need to be done once, not on every ifconfig up.
 */
static int
mt76x2_hw_init(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	int i, ntries;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * WLAN reset and power_on were already done in attach() before
	 * firmware load.  Linux mt76x2u_init_hardware() only calls
	 * reset_wlan + power_on once at the start — no second reset
	 * after firmware load.  Doing an RF reset pulse here could
	 * undo the RF power state established by power_on().
	 */

	/* Verify ASIC is alive before proceeding */
	if (mt76x2_wait_for_mac(sc) != 0)
		return (EIO);

	/* USB DMA setup — enable TX/RX bulk via CFG register (0x9018) */
	mt76x2_usb_dma_setup(sc);

	/*
	 * Do NOT write to 0x0238 (MAC space USB_DMA_CFG).
	 * Linux mt76x2 never touches this register — only mt76x0 does.
	 * USB DMA is fully configured via 0x9018 (CFG space) in
	 * mt76x2_usb_dma_setup() above.  The old comment about MCU
	 * responses needing this was wrong — that was caused by
	 * FCE_PDMA_GLOBAL_CONF overwrite (fixed separately).
	 */

	/*
	 * MCU init — matches Linux mt76x2u_mcu_init():
	 *   1. FUN_SET(Q_SELECT=1) routes RX frames to USB host
	 *   2. RADIO_ON via CMD_POWER_SAVING — turns on RF
	 *
	 * Linux sends both with wait=false (seq=0, no response expected).
	 * We now match this exactly via mcu_cmd_nowait().
	 */
	{
		struct {
			uint32_t id;
			uint32_t value;
		} __packed fun_msg;
		int error;

		fun_msg.id = htole32(1);	/* Q_SELECT */
		fun_msg.value = htole32(1);
		error = mt76x2_mcu_cmd_nowait(sc, MT76X2_MCU_CMD_FUN_SET,
		    &fun_msg, sizeof(fun_msg));
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "MCU FUN_SET(Q_SELECT) failed: %d\n", error);
			return (error);
		}
	}

	{
		struct {
			uint32_t mode;
			uint32_t level;
		} __packed msg;
		int error;

		msg.mode = htole32(0x31);	/* RADIO_ON */
		msg.level = htole32(0);
		error = mt76x2_mcu_cmd_nowait(sc, MT76X2_MCU_CMD_POWER_SAVING,
		    &msg, sizeof(msg));
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "MCU RADIO_ON failed: %d\n", error);
			return (error);
		}
	}

	/*
	 * WPDMA config — matches Linux mt76x2u_mac_reset() which writes
	 * 0x30 (BIT(4)|BIT(5) = burst size 3) as a full overwrite.
	 * This clears TX/RX DMA enable and busy bits.  WPDMA is the PCI
	 * DMA engine, not used on USB, but the MAC requires this config.
	 */
	mt76x2_write(sc, MT76X2_WPDMA_GLO_CFG, 0x30);

	/*
	 * PBF TX/RX max page counts — must be written BEFORE init table.
	 * Matches Linux mt76x2u_mac_reset() order.
	 */
	mt76x2_write(sc, 0x0408, 0xefef3f1f);	/* PBF_TX_MAX_PCNT */
	mt76x2_write(sc, 0x040c, 0x0000febf);	/* PBF_RX_MAX_PCNT */

	/*
	 * Apply default MAC values — complete init table matching Linux
	 * mt76_write_mac_initvals() exactly.  Includes PBF, FCE, MAC,
	 * TX config, RX filter, rate tables, protection, and TX power.
	 * MAC_SYS_CTRL is written as 0 (no BBP reset) per Linux.
	 */
	for (i = 0; i < (int)nitems(mt7612_def_mac); i++)
		mt76x2_write(sc, mt7612_def_mac[i].reg,
		    mt7612_def_mac[i].val);

	/*
	 * Post-init-table writes from Linux mt76x2u_mac_reset().
	 * These override or supplement init table values.
	 */

	/* AUTO_RSP_CFG — Linux writes after init table */
	mt76x2_write(sc, 0x1404, 0x00000013);	/* AUTO_RSP_CFG */

	/* Override MAX_LEN_CFG — Linux mt76x2u_mac_reset() uses 0x2f00 */
	mt76x2_write(sc, 0x1018, 0x2f00);	/* MAX_LEN_CFG */

	/* WMM scheduling parameters */
	mt76x2_write(sc, MT76X2_WMM_AIFSN, 0x00002273);
	mt76x2_write(sc, MT76X2_WMM_CWMIN, 0x00002344);
	mt76x2_write(sc, MT76X2_WMM_CWMAX, 0x000034aa);

	/*
	 * Clear MAC/BBP reset bits — safety clear matching Linux:
	 * mt76_clear(dev, MT_MAC_SYS_CTRL, RESET_CSR | RESET_BBP).
	 * Init table already wrote 0 to SYS_CTRL, so this is redundant
	 * but safe.
	 */
	mt76x2_read(sc, MT76X2_MAC_SYS_CTRL, &tmp);
	tmp &= ~(MT76X2_MAC_RESET_CSR | MT76X2_MAC_RESET_BBP);
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL, tmp);

	/* MT7612: disable coexistence (BT coex not used on USB) */
	mt76x2_read(sc, 0x0040, &tmp);	/* COEXCFG0 */
	tmp &= ~(1U << 0);		/* clear COEX_EN */
	mt76x2_write(sc, 0x0040, tmp);

	/* EXT_CCA_CFG: set extension channel CCA bits [15:12] */
	mt76x2_read(sc, MT76X2_EXT_CCA_CFG, &tmp);
	tmp |= 0xf000;
	mt76x2_write(sc, MT76X2_EXT_CCA_CFG, tmp);

	/* TX_ALC_CFG_4: clear BIT(31) — disable low-gain channel enable */
	mt76x2_read(sc, 0x13c0, &tmp);	/* TX_ALC_CFG_4 */
	tmp &= ~(1U << 31);
	mt76x2_write(sc, 0x13c0, tmp);

	/*
	 * Crystal oscillator calibration — matches Linux mt76x2u_mac_fixup_xtal().
	 * Without this, the radio's reference clock is misconfigured and the
	 * receiver cannot function at all.
	 */
	{
		uint16_t xtal_trim2, xtal_trim1, nic_conf2;
		int8_t offset = 0;
		uint8_t trim_val;

		/* Read XTAL trim from EEPROM */
		xtal_trim2 = le16dec(&sc->eeprom[0x9e]);
		offset = xtal_trim2 & 0x7f;
		if ((xtal_trim2 & 0xff) == 0xff)
			offset = 0;
		else if (xtal_trim2 & 0x80)
			offset = 0 - offset;

		trim_val = (xtal_trim2 >> 8) & 0xff;
		if (trim_val == 0x00 || trim_val == 0xff) {
			xtal_trim1 = le16dec(&sc->eeprom[0x3a]);
			trim_val = xtal_trim1 & 0xff;
			if (trim_val == 0x00 || trim_val == 0xff)
				trim_val = 0x14;
		}
		trim_val &= 0x7f;

		/* XO_CTRL5: set C2_VAL field [14:8] — must use CFG space */
		mt76x2_read_cfg(sc, 0x0114, &tmp);
		tmp &= ~(0x7fU << 8);
		tmp |= (uint32_t)(trim_val + offset) << 8;
		mt76x2_write_cfg(sc, 0x0114, tmp);

		/* XO_CTRL6: set C2_CTRL field [14:8] — must use CFG space */
		mt76x2_read_cfg(sc, 0x0118, &tmp);
		tmp |= (0x7fU << 8);
		mt76x2_write_cfg(sc, 0x0118, tmp);

		/* RF calibration pulse */
		mt76x2_write(sc, 0x0504, 0x06000000);
		mt76x2_write(sc, 0x050c, 0x08800000);
		DELAY(5000);	/* 5 ms */
		mt76x2_write(sc, 0x0504, 0x00000000);

		/* OFDM SIFS timing: field [15:8] = 0x0d */
		mt76x2_read(sc, MT76X2_XIFS_TIME_CFG, &tmp);
		tmp &= ~(0xffU << 8);
		tmp |= (0x0dU << 8);
		mt76x2_write(sc, MT76X2_XIFS_TIME_CFG, tmp);

		/* Backoff slot CC delay: field [11:8] = 1 */
		mt76x2_read(sc, 0x1104, &tmp);	/* BKOFF_SLOT_CFG */
		tmp &= ~(0xfU << 8);
		tmp |= (1U << 8);
		mt76x2_write(sc, 0x1104, tmp);

		/* FCE_L2 MPDU length write cleared below after beacon config */

		/* XO_CTRL7 based on XTAL_OPTION from NIC_CONF_2 */
		nic_conf2 = le16dec(&sc->eeprom[0x42]);
		switch ((nic_conf2 >> 9) & 0x03) {	/* XTAL_OPTION bits [10:9] */
		case 0:
			mt76x2_write(sc, 0x011c, 0x5c1fee80);
			break;
		case 1:
			mt76x2_write(sc, 0x011c, 0x5c1feed0);
			break;
		default:
			break;
		}

	}

	/* Wait for MAC TX/RX idle */
	for (ntries = 0; ntries < 100; ntries++) {
		mt76x2_read(sc, MT76X2_MAC_STATUS, &tmp);
		if (!(tmp & (MT76X2_MAC_STATUS_TX_BUSY |
		    MT76X2_MAC_STATUS_RX_BUSY)))
			break;
		DELAY(1000);
	}

	/* Set MAC address */
	mt76x2_set_macaddr(sc, ic->ic_macaddr);

	/* Read back RX filter (Linux does this) */
	mt76x2_read(sc, MT76X2_RX_FILTR_CFG, &sc->rxfilter);

	/*
	 * Clear WCID table — matches Linux mt76x2u_init_hardware():
	 *   for (i = 0; i < 256; i++)
	 *       mt76x02_mac_wcid_setup(dev, i, 0, NULL);
	 *
	 * Each WCID gets its ATTR register zeroed (clears cipher mode).
	 * For idx < 128, the WCID_ADDR (MAC address) is also zeroed.
	 * Without this, stale cipher modes cause the MAC to attempt
	 * decryption on received frames, fail, and silently drop them.
	 */
	for (i = 0; i < 256; i++) {
		mt76x2_write(sc, MT76X2_WCID_ATTR(i), 0);
		if (i < 128) {
			mt76x2_write(sc, MT76X2_WCID_ENTRY(i), 0);
			mt76x2_write(sc, MT76X2_WCID_ENTRY(i) + 4, 0);
		}
	}
	/*
	 * Clear shared key table — matches Linux:
	 *   for (i = 0; i < 16; i++)
	 *       for (k = 0; k < 4; k++)
	 *           mt76x02_mac_shared_key_setup(dev, i, k, NULL);
	 *
	 * Zeroes cipher mode in SKEY_MODE registers for all BSS/key combos.
	 */
	{
		int k;

		for (i = 0; i < 16; i++) {
			mt76x2_read(sc, MT76X2_SKEY_MODE(i), &tmp);
			for (k = 0; k < 4; k++) {
				int shift = 4 * (k + 4 * (i & 1));

				tmp &= ~(0xfU << shift);
			}
			mt76x2_write(sc, MT76X2_SKEY_MODE(i), tmp);
		}
	}

	/*
	 * Beacon config — matches Linux mt76x02u_init_beacon_config()
	 * → mt76x02_init_beacon_config().
	 */
	mt76x2_read(sc, MT76X2_BCN_TIME_CFG, &tmp);
	tmp &= ~(MT76X2_BCN_TIME_CFG_TIMER_EN |
	    MT76X2_BCN_TIME_CFG_TBTT_EN |
	    MT76X2_BCN_TIME_CFG_BCN_TX);
	tmp |= MT76X2_BCN_TIME_CFG_SYNC_MODE;
	mt76x2_write(sc, MT76X2_BCN_TIME_CFG, tmp);
	/* BCN_BYPASS_MASK also written by abort_tsf_sync() below */
	/* Beacon offsets: 5 slots, slot_size = (8192/5)&~63 = 1600 = 0x640
	 * offset[i] = i * 1600 / 64 = i * 25 */
	{
		uint32_t bcn_regs[4] = { 0, 0, 0, 0 };
		int s;

		for (s = 0; s < 5; s++)
			bcn_regs[s / 4] |= ((s * 25) & 0xff) << (8 * (s % 4));
		for (s = 0; s < 4; s++)
			mt76x2_write(sc, MT76X2_BCN_OFFSET_BASE + s * 4,
			    bcn_regs[s]);
	}

	/* Setup FCE L2 — clear MPDU length write (matches Linux) */
	mt76x2_read(sc, MT76X2_FCE_L2_STUFF, &tmp);
	tmp &= ~MT76X2_FCE_L2_WR_MPDU_LEN_EN;
	mt76x2_write(sc, MT76X2_FCE_L2_STUFF, tmp);

	/* RTS config */
	mt76x2_set_txrts(sc);

	/* Clear MCU mailbox */
	mt76x2_write(sc, MT76X2_MCU_COM_REG1, 0);

	/* Abort TSF sync */
	mt76x2_abort_tsf_sync(sc);

	/* US cycle count — matches Linux mt76x2u_init_hardware() */
	mt76x2_read(sc, MT76X2_US_CYC_CFG, &tmp);
	tmp = (tmp & ~0xff) | 0x1e;
	mt76x2_write(sc, MT76X2_US_CYC_CFG, tmp);

	/* TXOP truncation — Linux init_hardware writes 0x583f */
	mt76x2_write(sc, MT76X2_TXOP_CTRL_CFG, 0x0000583f);

	/*
	 * Load calibration registers via MCU.  This is critical —
	 * without LOAD_CR the RF/BBP has no calibration data and
	 * cannot receive.  Matches Linux mt76x2u_init_hardware().
	 */
	{
		int error;

		error = mt76x2_mcu_load_cr(sc, 2 /* RF_BBP_CR */, 0, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "MCU LOAD_CR failed: %d\n", error);
			return (error);
		}
	}

	/* Configure RX/TX PHY chain count in BBP */
	mt76x2_phy_set_rxpath(sc);
	mt76x2_phy_set_txdac(sc);

	/*
	 * Do NOT call mt76x2_phy_set_antenna() here.
	 * Linux USB driver (mt76x2u_init_hardware) does NOT call any
	 * antenna config after phy_set_rxpath()/phy_set_txdac().
	 * Our old phy_set_antenna() was writing bits [6:5] of AGC(0)
	 * which Linux NEVER touches, potentially corrupting the AGC
	 * and making the receiver deaf.
	 */

	/*
	 * Stop MAC at end of hw init — matches Linux mt76x2u_init_hardware()
	 * which calls mt76x2u_mac_stop() as its last step, leaving the
	 * hardware initialized but idle until start() (ifconfig up).
	 */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL, 0);

	sc->sc_hw_init_done = 1;

	device_printf(sc->sc_dev, "hardware init done\n");
	return (0);
}

/*
 * Lightweight per-up init — called from parent callback (ifconfig up).
 * Only sets up DMA, enables TX/RX, sets channel, and turns radio on.
 * Heavy one-time init (table clears, MAC defaults) is done in hw_init().
 */
static void
mt76x2_init_locked(struct mt76x2_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	int i;

	if (sc->sc_running)
		return;

	mt76x2_stop(sc);

	for (i = 0; i < MT76X2_EP_QUEUES; i++)
		mt76x2_setup_tx_list(sc, &sc->sc_epq[i]);

	/* Clear MCU timeout and calibration state for fresh start */
	sc->sc_mcu_timeout = 0;
	sc->cal_low_gain = -1;
	sc->tssi_comp_pending = 0;
	sc->dpd_cal_done = 0;
	sc->ed_tx_blocked = 0;

	/*
	 * Brief settle delay after stop — the USB endpoints need
	 * time to fully drain before new transfers start.
	 */
	MT76X2_UNLOCK(sc);
	pause("mt76up", hz / 10);	/* 100 ms */
	MT76X2_LOCK(sc);

	/* One-time HW init if not done yet (safety fallback) */
	if (!sc->sc_hw_init_done) {
		if (mt76x2_hw_init(sc) != 0)
			return;
	}

	/* Set MAC address (may have changed via vap) */
	mt76x2_set_macaddr(sc,
	    vap ? vap->iv_myaddr : ic->ic_macaddr);

	mt76x2_set_leds(sc, 1);

	/* Re-apply USB DMA config — may be reset by stop/MCU */
	mt76x2_usb_dma_setup(sc);

	/*
	 * Enable TX/RX MAC — matches Linux mt76x02u_mac_start().
	 * FUN_SET and RADIO_ON were sent once in hw_init (attach time).
	 * Do NOT re-send here — FUN_SET resets FCE_PDMA_GLOBAL_CONF,
	 * clearing BIT(0) (global DMA enable) and BIT(30).
	 */
	if (mt76x2_txrx_enable(sc) != 0)
		return;

	/* Set initial channel (after DMA/MAC enabled) */
	mt76x2_set_chan(sc, ic->ic_curchan);

	sc->sc_running = 1;

	/* Start rate control */
	usb_callout_reset(&sc->ratectl_ch, hz, mt76x2_ratectl_to, sc);

	/* Start temperature calibration (60-second interval) */
	usb_callout_reset(&sc->temp_cal_ch, 60 * hz,
	    mt76x2_temp_cal_to, sc);
}

static void
mt76x2_stop(void *arg)
{
	struct mt76x2_softc *sc = (struct mt76x2_softc *)arg;
	int i;

	MT76X2_LOCK_ASSERT(sc, MA_OWNED);

	sc->sc_running = 0;

	/* Stop rate control and temperature calibration callouts */
	usb_callout_stop(&sc->ratectl_ch);
	usb_callout_stop(&sc->temp_cal_ch);

	/* Disable TX/RX MAC */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL, 0);

	/* Stop USB transfers */
	for (i = 0; i < MT76X2_N_XFER; i++)
		usbd_transfer_stop(sc->sc_xfer[i]);

	/* Drain pending TX */
	for (i = 0; i < MT76X2_EP_QUEUES; i++)
		mt76x2_unsetup_tx_list(sc, &sc->sc_epq[i]);

	mt76x2_drain_mbufq(sc);
}

/*
 * ============================================================
 * Device probe / attach / detach
 * ============================================================
 */

static void
mt76x2_autoinst(void *arg, struct usb_device *udev,
    struct usb_attach_arg *uaa)
{
	struct usb_interface *iface;
	struct usb_interface_descriptor *id;

	if (uaa->dev_state != UAA_DEV_READY)
		return;

	iface = usbd_get_iface(udev, 0);
	if (iface == NULL)
		return;
	id = iface->idesc;
	if (id == NULL || id->bInterfaceClass != UICLASS_MASS)
		return;
	if (usbd_lookup_id_by_uaa(mt76x2_devs, sizeof(mt76x2_devs), uaa))
		return;

	if (usb_msc_eject(udev, 0, MSC_EJECT_STOPUNIT) == 0)
		uaa->dev_state = UAA_DEV_EJECTING;
}

static int
mt76x2_driver_loaded(struct module *mod, int what, void *arg)
{
	switch (what) {
	case MOD_LOAD:
		mt76x2_etag = EVENTHANDLER_REGISTER(usb_dev_configured,
		    mt76x2_autoinst, NULL, EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(usb_dev_configured, mt76x2_etag);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static int
mt76x2_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa == NULL)
		return (ENXIO);
	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != 0)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != 0)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(mt76x2_devs, sizeof(mt76x2_devs), uaa));
}

static int
mt76x2_attach(device_t self)
{
	struct mt76x2_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct ieee80211com *ic = &sc->sc_ic;
	int error;
	uint8_t iface_index;

	device_set_usb_desc(self);
	sc->sc_udev = uaa->device;
	sc->sc_dev = self;
	sc->sc_sent = 0;

	mtx_init(&sc->sc_mtx, device_get_nameunit(sc->sc_dev),
	    MTX_NETWORK_LOCK, MTX_DEF);

	iface_index = 0;

	error = usbd_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
	    mt76x2_config, MT76X2_N_XFER, sc, &sc->sc_mtx);
	if (error) {
		device_printf(sc->sc_dev,
		    "could not allocate USB transfers, err=%s\n",
		    usbd_errstr(error));
		goto detach_early;
	}

	/* Check if MCU response endpoint was found */
	if (sc->sc_xfer[MT76X2_BULK_MCU_RX] == NULL)
		device_printf(sc->sc_dev,
		    "warning: no MCU response endpoint\n");

	MT76X2_LOCK(sc);
	sc->sc_idx = 0;
	mbufq_init(&sc->sc_snd, ifqmaxlen);

	/*
	 * Flush firmware TX DMA buffer from prior driver load.
	 * USB_DMA_CFG BIT(16) = UDMA_TX_WL_DROP drops all queued TX
	 * frames.  Without this, stale frames from a previous kldload
	 * cycle wedge the TX bulk OUT endpoint.
	 */
	{
		uint32_t val;
		mt76x2_read(sc, MT76X2_USB_DMA_CFG, &val);
		val |= (1U << 16);	/* UDMA_TX_WL_DROP */
		mt76x2_write(sc, MT76X2_USB_DMA_CFG, val);
		DELAY(1000);
		val &= ~(1U << 16);
		mt76x2_write(sc, MT76X2_USB_DMA_CFG, val);
	}

	/*
	 * WLAN init — matches Linux mt76x2u_init_hardware() steps 1-4:
	 *   1. mt76x2_reset_wlan(dev, true) — WLAN reset + RF reset pulse
	 *   2. mt76x2u_power_on(dev) — MTCMOS, AD/DA, BBP, RF power-on
	 *   3. mt76x02_wait_for_mac(dev) — poll MAC CSR
	 *   4. mt76x2u_mcu_fw_init(dev) — firmware load
	 *
	 * mt76x2_power_on() powers up the MTCMOS power domain (0x0148),
	 * turns on AD/DA (0x1204), releases BBP software reset (0x0064),
	 * and powers on both RF units (0x0130).  Without this, the radio
	 * hardware is physically unpowered and no frames can be received.
	 */
	{
		uint32_t val;

		/*
		 * MCU firmware reset.  After kldunload + re-enumeration,
		 * the firmware may still be running but in a corrupted
		 * state.  Force a clean reload.
		 */
		mt76x2_vendor_request(sc, MT76X2_VEND_DEV_MODE,
		    UT_WRITE_VENDOR_DEVICE, 0x01, 0, NULL, 0);
		DELAY(5000);

		mt76x2_write(sc, MT76X2_MCU_COM_REG0, 0);

		/*
		 * Step 1: WLAN reset — matches Linux mt76x2_reset_wlan().
		 * Clear antenna force, pulse RF reset, then disable/re-enable.
		 *
		 * WLAN_FUN_CTRL (0x0080) is in CFG space per Linux, but
		 * on MT7612U USB, MAC and CFG spaces appear aliased for
		 * this register.  Use MAC-space access here to avoid
		 * disrupting USB connectivity during WLAN disable.
		 * power_on() will set the definitive state via CFG space.
		 */
		mt76x2_read(sc, MT76X2_WLAN_FUN_CTRL, &val);
		val &= ~MT76X2_FRC_WL_ANT_SEL;

		/* Pulse RF reset if WLAN is already enabled */
		if (val & MT76X2_WLAN_EN) {
			val |= MT76X2_WLAN_RESET_RF;
			mt76x2_write(sc, MT76X2_WLAN_FUN_CTRL, val);
			DELAY(20);
			val &= ~MT76X2_WLAN_RESET_RF;
		}

		/* Disable WLAN */
		val &= ~(MT76X2_WLAN_EN | MT76X2_WLAN_CLK_EN);
		mt76x2_write(sc, MT76X2_WLAN_FUN_CTRL, val);
		DELAY(20);

		/* Re-enable WLAN */
		val |= MT76X2_WLAN_EN | MT76X2_WLAN_CLK_EN;
		mt76x2_write(sc, MT76X2_WLAN_FUN_CTRL, val);
		DELAY(20);

		/*
		 * Step 2: Power on RF hardware — matches Linux
		 * mt76x2u_power_on().  This must happen BEFORE
		 * wait_for_mac and firmware load.
		 */
		mt76x2_power_on(sc);

		/* Wait for MAC */
		mt76x2_wait_for_mac(sc);

		/*
		 * Clear MCU mailbox to prepare for firmware loading.
		 * The IVB trigger (VEND_DEV_MODE, wValue=0x12) is
		 * sent by the firmware loader after upload, not here.
		 */
		mt76x2_write(sc, MT76X2_MCU_COM_REG1, 0);
	}

	sc->asic_ver = 0x7612;
	sc->asic_rev = 0x0044;

	/* Load firmware */
	if (mt76x2_load_microcode(sc) != 0) {
		device_printf(sc->sc_dev,
		    "failed to load firmware\n");
		goto detach;
	}

	/* Read EEPROM/EFUSE */
	if (mt76x2_read_eeprom(sc) != 0) {
		device_printf(sc->sc_dev, "failed to read EEPROM\n");
		goto detach;
	}

	/*
	 * Wait for WPDMA idle and MAC ready after firmware load.
	 * Matches Linux mt76x2u_init_hardware() steps 5-6:
	 *   mt76x02_wait_for_wpdma(100ms) + mt76x02_wait_for_mac()
	 */
	{
		uint32_t tmp;
		int ntries;

		for (ntries = 0; ntries < 1000; ntries++) {
			mt76x2_read(sc, MT76X2_WPDMA_GLO_CFG, &tmp);
			if (!(tmp & (MT76X2_WPDMA_TX_DMA_BUSY |
			    MT76X2_WPDMA_RX_DMA_BUSY)))
				break;
			DELAY(100);
		}
	}
	mt76x2_wait_for_mac(sc);

	/* One-time hardware init (MAC defaults, table clears, PHY config) */
	if (mt76x2_hw_init(sc) != 0) {
		device_printf(sc->sc_dev, "hardware init failed\n");
		goto detach;
	}

	device_printf(sc->sc_dev,
	    "MAC/BBP MT%04X (rev 0x%04X), %dT%dR, address %6D\n",
	    sc->asic_ver, sc->asic_rev,
	    sc->ntxchains, sc->nrxchains,
	    ic->ic_macaddr, ":");

	MT76X2_UNLOCK(sc);

	/* Set up ieee80211com */
	ic->ic_softc = sc;
	ic->ic_name = device_get_nameunit(self);
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;

	ic->ic_caps = IEEE80211_C_STA |
	    IEEE80211_C_MONITOR |
	    IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP |
	    IEEE80211_C_WDS |
	    IEEE80211_C_MBSS |
	    IEEE80211_C_SHPREAMBLE |
	    IEEE80211_C_SHSLOT |
	    IEEE80211_C_WME |
	    IEEE80211_C_WPA |
	    IEEE80211_C_PMGT;

	/* 802.11n HT capabilities */
	ic->ic_htcaps = IEEE80211_HTC_HT |
	    IEEE80211_HTC_AMPDU |
	    IEEE80211_HTC_AMSDU |
	    IEEE80211_HTCAP_MAXAMSDU_3839 |
	    IEEE80211_HTCAP_SMPS_OFF |
	    IEEE80211_HTCAP_CHWIDTH40 |
	    IEEE80211_HTCAP_SHORTGI20 |
	    IEEE80211_HTCAP_SHORTGI40 |
	    IEEE80211_HTCAP_LDPC |
	    IEEE80211_HTC_TXLDPC |
	    IEEE80211_HTCAP_RXSTBC_2STREAM;
	if (sc->ntxchains > 1)
		ic->ic_htcaps |= IEEE80211_HTCAP_TXSTBC;

	ic->ic_rxstream = sc->nrxchains;
	ic->ic_txstream = sc->ntxchains;

	/* 802.11ac VHT capabilities (5GHz only, up to VHT80) */
	ic->ic_flags_ext |= IEEE80211_FEXT_VHT;
	ic->ic_vht_cap.vht_cap_info =
	    IEEE80211_VHTCAP_MAX_MPDU_LENGTH_3895 |
	    IEEE80211_VHTCAP_RXLDPC |
	    IEEE80211_VHTCAP_SHORT_GI_80 |
	    IEEE80211_VHTCAP_RXSTBC_1 |
	    _IEEE80211_SHIFTMASK(7,
		IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK);
	if (sc->ntxchains > 1)
		ic->ic_vht_cap.vht_cap_info |= IEEE80211_VHTCAP_TXSTBC;
	{
		uint16_t mcs_map = 0;
		int i;

		for (i = 0; i < 8; i++) {
			if (i < sc->nrxchains)
				mcs_map |= (IEEE80211_VHT_MCS_SUPPORT_0_9
				    << (i * 2));
			else
				mcs_map |= (IEEE80211_VHT_MCS_NOT_SUPPORTED
				    << (i * 2));
		}
		ic->ic_vht_cap.supp_mcs.rx_mcs_map = htole16(mcs_map);
		ic->ic_vht_cap.supp_mcs.rx_highest = 0;

		mcs_map = 0;
		for (i = 0; i < 8; i++) {
			if (i < sc->ntxchains)
				mcs_map |= (IEEE80211_VHT_MCS_SUPPORT_0_9
				    << (i * 2));
			else
				mcs_map |= (IEEE80211_VHT_MCS_NOT_SUPPORTED
				    << (i * 2));
		}
		ic->ic_vht_cap.supp_mcs.tx_mcs_map = htole16(mcs_map);
		ic->ic_vht_cap.supp_mcs.tx_highest = 0;
	}

	ic->ic_cryptocaps = IEEE80211_CRYPTO_WEP |
	    IEEE80211_CRYPTO_AES_CCM |
	    IEEE80211_CRYPTO_TKIP |
	    IEEE80211_CRYPTO_TKIPMIC;

	ic->ic_flags |= IEEE80211_F_DATAPAD;
	ic->ic_flags_ext |= IEEE80211_FEXT_SWBMISS;

	mt76x2_getradiocaps(ic, IEEE80211_CHAN_MAX, &ic->ic_nchans,
	    ic->ic_channels);

	ieee80211_ifattach(ic);
	sc->sc_ic_setup = 1;

	ic->ic_scan_start = mt76x2_scan_start;
	ic->ic_scan_end = mt76x2_scan_end;
	ic->ic_set_channel = mt76x2_set_channel;
	ic->ic_getradiocaps = mt76x2_getradiocaps;
	ic->ic_node_alloc = mt76x2_node_alloc;
	ic->ic_newassoc = mt76x2_newassoc;
	ic->ic_update_mcast = mt76x2_update_mcast;
	ic->ic_updateslot = mt76x2_updateslot;
	ic->ic_wme.wme_update = mt76x2_wme_update;
	ic->ic_raw_xmit = mt76x2_raw_xmit;
	ic->ic_update_promisc = mt76x2_update_promisc;
	ic->ic_vap_create = mt76x2_vap_create;
	ic->ic_vap_delete = mt76x2_vap_delete;
	ic->ic_transmit = mt76x2_transmit;
	ic->ic_parent = mt76x2_parent;
	ic->ic_update_chw = mt76x2_update_chw;
	ic->ic_ampdu_enable = mt76x2_ampdu_enable;
	sc->sc_addba_request = ic->ic_addba_request;
	ic->ic_addba_request = mt76x2_addba_request;
	sc->sc_addba_response = ic->ic_addba_response;
	ic->ic_addba_response = mt76x2_addba_response;
	sc->sc_addba_stop = ic->ic_addba_stop;
	ic->ic_addba_stop = mt76x2_addba_stop;

	ieee80211_radiotap_attach(ic, &sc->sc_txtap.wt_ihdr,
	    sizeof(sc->sc_txtap), MT76X2_TX_RADIOTAP_PRESENT,
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
	    MT76X2_RX_RADIOTAP_PRESENT);

	TASK_INIT(&sc->cmdq_task, 0, mt76x2_cmdq_cb, sc);
	TASK_INIT(&sc->ratectl_task, 0, mt76x2_ratectl_cb, sc);
	TASK_INIT(&sc->txstat_task, 0, mt76x2_txstat_drain, sc);
	TASK_INIT(&sc->temp_cal_task, 0, mt76x2_temp_cal_cb, sc);
	usb_callout_init_mtx(&sc->ratectl_ch, &sc->sc_mtx, 0);
	usb_callout_init_mtx(&sc->temp_cal_ch, &sc->sc_mtx, 0);

	if (bootverbose)
		ieee80211_announce(ic);

	return (0);

detach:
	MT76X2_UNLOCK(sc);
detach_early:
	mt76x2_detach(self);
	return (ENXIO);
}

static int
mt76x2_detach(device_t self)
{
	struct mt76x2_softc *sc = device_get_softc(self);
	struct ieee80211com *ic = &sc->sc_ic;
	int i;

	/*
	 * Signal detach state first (under lock), matching run(4) pattern.
	 * Then disable MAC/DMA hardware so the device is in a clean state
	 * for re-probe.  Finally, unsetup USB transfers outside the lock —
	 * usbd_transfer_unsetup drains all pending callbacks.
	 */
	MT76X2_LOCK(sc);
	sc->sc_running = 0;

	/*
	 * Tell firmware to power down radio before teardown.
	 * Matches Linux mt76x2u_cleanup() → mt76x02_mcu_set_radio_state(false).
	 * Without this, firmware stays in active state after kldunload,
	 * causing USB DMA wedge on next kldload.  Fire-and-forget —
	 * if the USB device is already gone, the transfer just fails.
	 */
	{
		struct {
			uint32_t mode;
			uint32_t level;
		} __packed msg;

		msg.mode = htole32(0x30);	/* RADIO_OFF */
		msg.level = htole32(0);
		mt76x2_mcu_cmd_nowait(sc, MT76X2_MCU_CMD_POWER_SAVING,
		    &msg, sizeof(msg));
	}

	/*
	 * Flush firmware TX DMA buffer and stop MAC.
	 * RADIO_OFF alone doesn't clear the firmware's internal TX
	 * buffer; without flushing, stale frames persist across
	 * kldunload/kldload and wedge the TX endpoint.
	 *
	 * USB_DMA_CFG BIT(16) = UDMA_TX_WL_DROP — drops all queued
	 * TX frames in the USB DMA engine.  Pulse it on then off.
	 * Matches Linux mt76x0 firmware reset path.
	 */
	mt76x2_write(sc, MT76X2_MAC_SYS_CTRL, 0);
	{
		uint32_t val;

		mt76x2_read(sc, MT76X2_USB_DMA_CFG, &val);
		val |= (1U << 16);	/* UDMA_TX_WL_DROP */
		mt76x2_write(sc, MT76X2_USB_DMA_CFG, val);
		DELAY(1000);
		val &= ~(1U << 16);
		mt76x2_write(sc, MT76X2_USB_DMA_CFG, val);
	}

	sc->sc_detached = 1;

	/* Stop USB transfers under lock first */
	for (i = 0; i < MT76X2_N_XFER; i++)
		usbd_transfer_stop(sc->sc_xfer[i]);

	for (i = 0; i < MT76X2_EP_QUEUES; i++)
		mt76x2_unsetup_tx_list(sc, &sc->sc_epq[i]);
	mt76x2_drain_mbufq(sc);
	MT76X2_UNLOCK(sc);

	/* Unsetup USB transfers outside the lock (drains callbacks) */
	usbd_transfer_unsetup(sc->sc_xfer, MT76X2_N_XFER);

	/* Drain callouts and tasks (must be outside the lock) */
	usb_callout_drain(&sc->ratectl_ch);
	usb_callout_drain(&sc->temp_cal_ch);
	if (sc->sc_ic_setup) {
		ieee80211_draintask(ic, &sc->ratectl_task);
		ieee80211_draintask(ic, &sc->txstat_task);
		ieee80211_draintask(ic, &sc->cmdq_task);
		ieee80211_draintask(ic, &sc->temp_cal_task);

		/*
		 * Drain callouts again — tasks that ran during the
		 * drain above could have re-armed callouts before
		 * seeing sc_running == 0.
		 */
		usb_callout_drain(&sc->ratectl_ch);
		usb_callout_drain(&sc->temp_cal_ch);

		/*
		 * Cancel any in-progress scan and wait for it to finish.
		 * ieee80211_cancel_anyscan() signals the scan to stop but
		 * does not wait — the scan task runs on ic->ic_tq and may
		 * still hold node table locks.  We must drain the entire
		 * taskqueue to ensure the scan task has fully completed
		 * before ieee80211_ifdetach() tears down the node table.
		 * Without this, ieee80211_node_table_reset() hits a
		 * contested mutex with no owner → page fault.
		 */
		if (!TAILQ_EMPTY(&ic->ic_vaps)) {
			struct ieee80211vap *vap;
			vap = TAILQ_FIRST(&ic->ic_vaps);
			if (vap != NULL)
				ieee80211_cancel_anyscan(vap);
		}
		taskqueue_drain_all(ic->ic_tq);

		ieee80211_ifdetach(ic);
		sc->sc_ic_setup = 0;
	} else {
		taskqueue_drain(taskqueue_thread, &sc->ratectl_task);
		taskqueue_drain(taskqueue_thread, &sc->txstat_task);
		taskqueue_drain(taskqueue_thread, &sc->cmdq_task);
	}

	if (sc->fw_buf != NULL) {
		free(sc->fw_buf, M_USBDEV);
		sc->fw_buf = NULL;
	}

	if (sc->rx_m != NULL) {
		m_freem(sc->rx_m);
		sc->rx_m = NULL;
	}

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

/*
 * ============================================================
 * Parent callback (ifconfig up/down)
 * ============================================================
 */

static void
mt76x2_parent(struct ieee80211com *ic)
{
	struct mt76x2_softc *sc = ic->ic_softc;
	int startall = 0;

	MT76X2_LOCK(sc);
	if (sc->sc_detached) {
		MT76X2_UNLOCK(sc);
		return;
	}
	if (ic->ic_nrunning > 0) {
		if (!sc->sc_running) {
			mt76x2_init_locked(sc);
			startall = sc->sc_running;
		}
	} else {
		if (sc->sc_running)
			mt76x2_stop(sc);
	}
	MT76X2_UNLOCK(sc);

	if (startall)
		ieee80211_start_all(ic);
}

/*
 * ============================================================
 * Command queue callback
 * ============================================================
 */

static void
mt76x2_cmdq_cb(void *arg, int pending)
{
	struct mt76x2_softc *sc = arg;
	uint8_t i;

	MT76X2_LOCK(sc);
	for (i = sc->cmdq_exec; sc->cmdq[i].func && pending;
	    i = sc->cmdq_exec, pending--) {
		if (sc->sc_detached)
			break;
		if (sc->cmdq[i].arg0)
			sc->cmdq[i].func(sc->cmdq[i].arg0);
		else
			sc->cmdq[i].func(&sc->cmdq[i]);
		sc->cmdq[i].arg0 = NULL;
		sc->cmdq[i].func = NULL;
		sc->cmdq_exec++;
		sc->cmdq_exec &= MT76X2_CMDQ_MASQ;
	}
	MT76X2_UNLOCK(sc);
}

/*
 * ============================================================
 * Utility
 * ============================================================
 */

static void __unused
mt76x2_delay(struct mt76x2_softc *sc, u_int ms)
{
	usb_pause_mtx(mtx_owned(&sc->sc_mtx) ? &sc->sc_mtx : NULL,
	    USB_MS_TO_TICKS(ms));
}

/*
 * ============================================================
 * Module / driver registration
 * ============================================================
 */

static device_method_t mt76x2_methods[] = {
	DEVMETHOD(device_probe, mt76x2_match),
	DEVMETHOD(device_attach, mt76x2_attach),
	DEVMETHOD(device_detach, mt76x2_detach),
	DEVMETHOD_END
};

static driver_t mt76x2_driver = {
	.name = "mt76x2",
	.methods = mt76x2_methods,
	.size = sizeof(struct mt76x2_softc)
};

DRIVER_MODULE(mt76x2, uhub, mt76x2_driver, mt76x2_driver_loaded, NULL);
MODULE_DEPEND(mt76x2, wlan, 1, 1, 1);
MODULE_DEPEND(mt76x2, usb, 1, 1, 1);
MODULE_DEPEND(mt76x2, firmware, 1, 1, 1);
MODULE_VERSION(mt76x2, 1);
USB_PNP_HOST_INFO(mt76x2_devs);
