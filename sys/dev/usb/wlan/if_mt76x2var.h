/*-
 * Copyright (c) 2008,2009 Damien Bergamini <damien.bergamini@free.fr>
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

#ifndef _IF_MT76X2VAR_H_
#define _IF_MT76X2VAR_H_

#define MT76X2_MAX_RXSZ		16384	/* RX bulk aggregation: up to 8 frames */
#define MT76X2_TX_TIMEOUT	5000	/* ms */
#define MT76X2_VAP_MAX		8
#define MT76X2_RX_RING_COUNT	1
#define MT76X2_TX_RING_COUNT	32
#define MT76X2_TX_AGG_COUNT	8	/* max frames per USB transfer */

#define MT76X2_RXQ_COUNT	2
#define MT76X2_TXQ_COUNT	6

#define MT76X2_WCID_MAX		128
#define MT76X2_AID2WCID(aid)	(1 + ((aid) & 0x7f))

/* Radiotap headers */
struct mt76x2_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_dbm_antsignal;
	uint8_t		wr_antenna;
	uint8_t		wr_antsignal;
	/* VHT field requires 2-byte alignment — 1 byte padding */
	uint8_t		wr_vht_pad;
	uint16_t	wr_vht_known;
	uint8_t		wr_vht_flags;
	uint8_t		wr_vht_bandwidth;
	uint8_t		wr_vht_mcs_nss[4];
	uint8_t		wr_vht_coding;
	uint8_t		wr_vht_group_id;
	uint16_t	wr_vht_partial_aid;
} __packed;

#define MT76X2_RX_RADIOTAP_PRESENT				\
	(1 << IEEE80211_RADIOTAP_FLAGS |			\
	 1 << IEEE80211_RADIOTAP_RATE |				\
	 1 << IEEE80211_RADIOTAP_CHANNEL |			\
	 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL |		\
	 1 << IEEE80211_RADIOTAP_ANTENNA |			\
	 1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL |			\
	 1 << IEEE80211_RADIOTAP_VHT)

struct mt76x2_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
	uint8_t		wt_hwqueue;
} __packed;

#define MT76X2_TX_RADIOTAP_PRESENT				\
	(1 << IEEE80211_RADIOTAP_FLAGS |			\
	 1 << IEEE80211_RADIOTAP_RATE |				\
	 1 << IEEE80211_RADIOTAP_CHANNEL)

struct mt76x2_softc;

/* TX power info — per-channel data from EEPROM */
struct mt76x2_tx_power_info {
	uint8_t		target_power;
	int8_t		delta_bw40;
	int8_t		delta_bw80;
	struct {
		int8_t	tssi_slope;
		int8_t	tssi_offset;
		int8_t	target_power;
		int8_t	delta;
	} chain[2];
};

/* Per-rate power offsets (0.5 dBm units) */
struct mt76x2_rate_power {
	union {
		struct {
			int8_t	cck[4];
			int8_t	ofdm[8];
			int8_t	ht[16];
			int8_t	vht[2];
		};
		int8_t	all[30];
	};
};

/* 5 GHz calibration channel groups (matches Linux mt76x2/eeprom.h) */
enum mt76x2_cal_channel_group {
	MT76X2_CH_5G_JAPAN,
	MT76X2_CH_5G_UNII_1,
	MT76X2_CH_5G_UNII_2,
	MT76X2_CH_5G_UNII_2E_1,
	MT76X2_CH_5G_UNII_2E_2,
	MT76X2_CH_5G_UNII_3,
};

/* TX data structures */
struct mt76x2_tx_data {
	STAILQ_ENTRY(mt76x2_tx_data)	next;
	struct mbuf		*m;
	struct mt76x2_softc	*sc;
	struct ieee80211_node	*ni;
	uint8_t			ridx;
	uint8_t			desc[4 + sizeof(struct mt76x2_txwi)];
};

struct mt76x2_node {
	struct ieee80211_node	ni;
	uint8_t			ridx[IEEE80211_RATE_MAXSIZE];
	uint8_t			ctl_ridx;
	uint8_t			amrr_ridx;	/* current AMRR rate index */
	uint8_t			mgt_ridx;	/* management frame rate index */
	uint8_t			wcid;
	uint8_t			hw_key;	/* hardware key installed */
	uint8_t			vht_nss;	/* VHT spatial streams (1-2) */
	uint8_t			vht_mcs;	/* VHT MCS (0-9) */
};

struct mt76x2_cmdq {
	void			*arg0;
	void			*arg1;
	void			(*func)(void *);
	struct ieee80211_key	key;
	uint8_t			mac[IEEE80211_ADDR_LEN];
	uint8_t			wcid;
};

struct mt76x2_tx_ring {
	struct mt76x2_tx_data	data[MT76X2_TX_RING_COUNT];
	int			cur;
	int			queued;
};

STAILQ_HEAD(mt76x2_tx_data_head, mt76x2_tx_data);

struct mt76x2_endpoint_queue {
	struct mt76x2_tx_data		tx_data[MT76X2_TX_RING_COUNT];
	struct mt76x2_tx_data_head	tx_qh;	/* queued for xmit */
	struct mt76x2_tx_data_head	tx_fh;	/* free list */
	int			tx_nfree;
	int			tx_cnt;
	/* TX aggregation: frames packed in current USB transfer */
	struct mt76x2_tx_data	*tx_agg[MT76X2_TX_AGG_COUNT];
	int			tx_agg_cnt;
};

/* USB transfer indices */
enum {
	MT76X2_BULK_TX_BE,	/* best-effort */
	MT76X2_BULK_TX_BK,	/* background */
	MT76X2_BULK_TX_VI,	/* video */
	MT76X2_BULK_TX_VO,	/* voice */
	MT76X2_BULK_TX_HCCA,
	MT76X2_BULK_FW,		/* firmware/MCU cmd (6th OUT → EP 0x09) */
	MT76X2_BULK_RX,
	MT76X2_BULK_MCU_RX,	/* MCU command responses (2nd IN endpoint) */
	MT76X2_BULK_TX_PRIO,	/* optional, may have no endpoint */
	MT76X2_N_XFER,
};

#define MT76X2_EP_QUEUES	6

/* Command queue */
#define MT76X2_CMDQ_SIZE	16
#define MT76X2_CMDQ_MASQ	(MT76X2_CMDQ_SIZE - 1)
#define MT76X2_CMDQ_GET(c)	(atomic_fetchadd_32((c), 1) & MT76X2_CMDQ_MASQ)

enum {
	MT76X2_CMD_WCID_SET,
	MT76X2_CMD_KEY_SET,
	MT76X2_CMD_STATE,
	MT76X2_CMD_LED,
};

struct mt76x2_vap {
	struct ieee80211vap	vap;
	int			(*newstate)(struct ieee80211vap *,
				    enum ieee80211_state, int);
	uint8_t			bssid[IEEE80211_ADDR_LEN];
	uint8_t			wcid;
};

#define MT76X2_VAP(vap)	((struct mt76x2_vap *)(vap))
#define MT76X2_NODE(ni)	((struct mt76x2_node *)(ni))

/* Softc - main driver structure */
struct mt76x2_softc {
	struct ieee80211com	sc_ic;
	struct mbufq		sc_snd;
	device_t		sc_dev;
	struct usb_device	*sc_udev;
	struct mtx		sc_mtx;

	struct usb_xfer		*sc_xfer[MT76X2_N_XFER];
	int			sc_idx;
	int			sc_sent;
	int			sc_running;
	int			sc_detached;
	int			sc_ic_setup;	/* ieee80211 ifattach done */
	int			sc_cal_done;	/* first-time R/RC cal done */
	int			sc_hw_init_done; /* one-time HW init done */

	/* RX mbuf for DMA */
	struct mbuf		*rx_m;

	uint32_t		asic_ver;
	uint32_t		asic_rev;
	uint32_t		mac_rev;
	uint32_t		rf_rev;
	uint8_t			rf_type;

	int			ntxchains;
	int			nrxchains;

	uint8_t			sc_bssid[IEEE80211_ADDR_LEN];
	uint8_t			eeprom[MT76X2_EE_SIZE];
	int			sc_txtimer;

	/* TX queues */
	struct mt76x2_endpoint_queue sc_epq[MT76X2_EP_QUEUES];

	/* Firmware upload state (bulk transfer) */
	uint8_t			*fw_buf;
	int			fw_buflen;
	int			fw_bufsz;	/* allocated size of fw_buf */
	int			fw_chunk_done;
	int			fw_error;

	/* LED state */
	uint8_t			leds;

	/* Calibration */
	int			txpower;
	int			txpower_conf;	/* regulatory limit (0.5 dBm) */
	int8_t			txpower_delta;
	int8_t			target_power;
	int8_t			target_power_delta[2];
	struct mt76x2_rate_power rate_power;
	int8_t			rssi_offset_2g[2];
	int8_t			rssi_offset_5g[2];
	int8_t			lna_gain_2g;
	int8_t			lna_gain_5g[3];
	int8_t			lna_gain;	/* active LNA gain for RSSI cal */
	int8_t			high_gain[2];	/* per-chain RX high gain adj */
	uint8_t			agc_gain_init[2]; /* initial AGC gain snapshot */

	/* Channel-related */
	uint8_t			rf_freq_offset;
	uint32_t		rf_pa_mode_2g[4];
	uint32_t		rf_pa_mode_5g[4];

	/* MCU state */
	uint8_t			mcu_seq;
	int			sc_mcu_timeout;	/* MCU cmd timed out */
	int			mcu_resp_done;	/* MCU response received */
	int			mcu_resp_retries; /* non-matching resp count */
	uint8_t			mcu_resp_buf[256]; /* MCU response data */
	int			mcu_resp_len;	/* MCU response length */

	/* Command queue */
	struct mt76x2_cmdq	cmdq[MT76X2_CMDQ_SIZE];
	struct task		cmdq_task;
	uint32_t		cmdq_exec;
	uint32_t		cmdq_store;

	/* Rate control */
	struct task		ratectl_task;
	struct usb_callout	ratectl_ch;
	uint32_t		tx_total;	/* lifetime TX completion count */
	uint32_t		tx_total_last;	/* tx_total at last ratectl */
	uint32_t		tx_total_drain;	/* tx_total at last FIFO drain */

	/* Per-WCID TX stats from TX_STAT_FIFO (non-AMPDU frames only) */
	uint32_t		sc_tx_ok;
	uint32_t		sc_tx_fail;
	uint32_t		sc_tx_retry;

	/* TX status FIFO drain task */
	struct task		txstat_task;

	/* TX/RX radiotap */
	struct mt76x2_rx_radiotap_header sc_rxtap;
	struct mt76x2_tx_radiotap_header sc_txtap;

	/* RX filter */
	uint32_t		rxfilter;

	/* Saved net80211 ADDBA callbacks (for chaining) */
	int	(*sc_addba_request)(struct ieee80211_node *,
		    struct ieee80211_tx_ampdu *, int, int, int);
	int	(*sc_addba_response)(struct ieee80211_node *,
		    struct ieee80211_tx_ampdu *, int, int, int);
	void	(*sc_addba_stop)(struct ieee80211_node *,
		    struct ieee80211_tx_ampdu *);

	/* Scanning state */
	int			sc_scanning;
	int			sc_chan_changing; /* block TX during chan switch */
	int			sc_last_cal_chan; /* last calibrated channel # */

	/* Temperature calibration */
	struct usb_callout	temp_cal_ch;
	struct task		temp_cal_task;
	int32_t			sc_temperature;	/* temp cal cycle count */

	/* Monitor mode */
	int			sc_monitor;	/* vap is in monitor mode */

	/* TSSI / DPD calibration state */
	int			tssi_comp_pending; /* TSSI trigger sent, waiting */
	int			dpd_cal_done;	/* DPD done for current channel */

	/* AGC dynamic gain control */
	int			agc_gain_cur[2]; /* current AGC gain per chain */
	int			agc_gain_adjust; /* VGA gain offset (2dB steps) */
	int			cal_low_gain;	/* gain state: -1/0/1/2 */
	uint32_t		cal_false_cca;	/* last false CCA count */

	/* EDCCA monitoring */
	int			ed_trigger;	/* consecutive busy triggers */
	int			ed_silent;	/* consecutive quiet periods */
	int			ed_tx_blocked;	/* TX currently gated */

	/* MAC watchdog */
	uint32_t		mac_err_count;	/* MAC error recovery count */

};

#define MT76X2_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define MT76X2_UNLOCK(sc)	mtx_unlock(&(sc)->sc_mtx)
#define MT76X2_LOCK_ASSERT(sc, what)	mtx_assert(&(sc)->sc_mtx, (what))

#endif /* _IF_MT76X2VAR_H_ */
