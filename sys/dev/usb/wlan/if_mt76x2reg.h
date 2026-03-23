/*-
 * Copyright (c) 2018 Lorenzo Bianconi <lorenzo.bianconi83@gmail.com>
 * Copyright (c) 2016 Felix Fietkau <nbd@nbd.name>
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

#ifndef _IF_MT76X2REG_H_
#define _IF_MT76X2REG_H_

/*
 * MediaTek MT7612U register definitions.
 *
 * Based on Linux mt76x2u driver register map (BSD-3-Clause).
 */

/* Chip identification */
#define MT76X2_ASIC_VER			0x0000
#define MT76X2_REV_E3			0x22
#define MT76X2_REV_E4			0x33

/* Vendor request addresses */
#define MT76X2_VEND_ADDR_CFG		0x410000
#define MT76X2_VEND_ADDR_EEPROM		0x400000
#define MT76X2_VEND_ADDR(type, off)	(MT76X2_VEND_ADDR_##type + (off))

/* USB vendor requests */
#define MT76X2_VEND_DEV_MODE		0x01
#define MT76X2_VEND_WRITE		0x02
#define MT76X2_VEND_MULTI_WRITE		0x06
#define MT76X2_VEND_MULTI_READ		0x07
#define MT76X2_VEND_READ_EEPROM	0x09
#define MT76X2_VEND_WRITE_FCE		0x42
#define MT76X2_VEND_WRITE_CFG		0x46
#define MT76X2_VEND_READ_CFG		0x47

/* CMB control */
#define MT76X2_CMB_CTRL			0x0020
#define MT76X2_CMB_CTRL_XTAL_RDY	(1U << 22)
#define MT76X2_CMB_CTRL_PLL_LD		(1U << 23)

/* EFUSE */
#define MT76X2_EFUSE_CTRL		0x0024
#define MT76X2_EFUSE_CTRL_AOUT		0x0000003f
#define MT76X2_EFUSE_CTRL_MODE		0x000000c0
#define MT76X2_EFUSE_CTRL_AIN		0x03ff0000
#define MT76X2_EFUSE_CTRL_KICK		(1U << 30)
#define MT76X2_EFUSE_CTRL_SEL		(1U << 31)
#define MT76X2_EFUSE_DATA_BASE		0x0028
#define MT76X2_EFUSE_DATA(n)		(MT76X2_EFUSE_DATA_BASE + ((n) << 2))

/* WLAN function control */
#define MT76X2_WLAN_FUN_CTRL		0x0080
#define MT76X2_WLAN_EN			(1U << 0)
#define MT76X2_WLAN_CLK_EN		(1U << 1)
#define MT76X2_WLAN_RESET_RF		(1U << 2)
#define MT76X2_CSR_F20M_CKEN		(1U << 3)
#define MT76X2_PCIE_CLK_REQ		(1U << 4)
#define MT76X2_FRC_WL_ANT_SEL		(1U << 5)
#define MT76X2_THERM_RST		(1U << 8)
#define MT76X2_THERM_CKEN		(1U << 9)

/*
 * USB DMA config — TWO SEPARATE registers with DIFFERENT bit layouts:
 *
 * 0x9018 (CFG space, MT76X2_USB_U3DMA_CFG):
 *   Bit positions match Linux mt76x02_regs.h MT_USB_DMA_CFG_*.
 *   Used by Linux mt76x2u_dma_enable() for USB DMA configuration.
 */
#define MT76X2_USB_U3DMA_CFG		0x9018
#define MT76X2_USB_DMA_CFG_RX_AGG_TOUT_SHIFT	0	/* bits [7:0] */
#define MT76X2_USB_DMA_CFG_RX_AGG_LMT_SHIFT	8	/* bits [15:8] */
#define MT76X2_USB_DMA_CFG_RX_DROP_PAD	(1U << 18)
#define MT76X2_USB_DMA_CFG_RX_AGG_EN	(1U << 21)
#define MT76X2_USB_DMA_CFG_RX_BULK_EN	(1U << 22)
#define MT76X2_USB_DMA_CFG_TX_BULK_EN	(1U << 23)

/*
 * 0x0238 (MAC space) — PCI-era WPDMA DMA config.
 * MT76x2 USB driver must NOT write to this register (only mt76x0 uses it).
 * The USB DMA is configured exclusively via 0x9018 (CFG space) above.
 */
#define MT76X2_USB_DMA_CFG		0x0238

/* WLAN MTC control */
#define MT76X2_WLAN_MTC_CTRL		0x10148
#define MT76X2_MTC_PWR_UP		(1U << 0)
#define MT76X2_MTC_PWR_ACK		(1U << 12)
#define MT76X2_MTC_PWR_ACK_S		(1U << 13)
#define MT76X2_MTC_STATE_UP		(1U << 28)

/* DMA/scheduler registers */
#define MT76X2_INT_SOURCE		0x0200
#define MT76X2_INT_MASK			0x0204
#define MT76X2_WPDMA_GLO_CFG		0x0208
#define MT76X2_WPDMA_TX_DMA_EN		(1U << 0)
#define MT76X2_WPDMA_TX_DMA_BUSY	(1U << 1)
#define MT76X2_WPDMA_RX_DMA_EN		(1U << 2)
#define MT76X2_WPDMA_RX_DMA_BUSY	(1U << 3)
#define MT76X2_WPDMA_TX_WB_DDONE	(1U << 6)

#define MT76X2_WMM_AIFSN		0x0214
#define MT76X2_WMM_CWMIN		0x0218
#define MT76X2_WMM_CWMAX		0x021c
#define MT76X2_WMM_TXOP0		0x0220
#define MT76X2_WMM_TXOP1		0x0224

/* FCE registers */
#define MT76X2_FCE_PSE_CTRL		0x0800
#define MT76X2_FCE_L2_STUFF		0x080c
#define MT76X2_FCE_L2_WR_MPDU_LEN_EN	(1U << 4)
#define MT76X2_TX_CPU_FROM_FCE_BASE	0x09a0
#define MT76X2_TX_CPU_FROM_FCE_MAX_CNT	0x09a4
#define MT76X2_TX_CPU_FROM_FCE_CPU_IDX	0x09a8
#define MT76X2_FCE_PDMA_GLOBAL_CONF	0x09c4
#define MT76X2_FCE_SKIP_FS		0x0a6c

#define MT76X2_FCE_DMA_ADDR		0x0230
#define MT76X2_FCE_DMA_LEN		0x0234

/* US cycle count */
#define MT76X2_US_CYC_CFG		0x02a4
#define MT76X2_US_CYC_CNT		0x000000ff

/* PBF/MAC registers */
#define MT76X2_PBF_SYS_CTRL		0x0400
#define MT76X2_PBF_MCU_RESET		(1U << 0)
#define MT76X2_PBF_DMA_RESET		(1U << 1)
#define MT76X2_PBF_MAC_RESET		(1U << 2)
#define MT76X2_PBF_PBF_RESET		(1U << 3)
#define MT76X2_PBF_CFG			0x0404
#define MT76X2_BCN_OFFSET_BASE		0x041c
#define MT76X2_BCN_BYPASS_MASK		0x108c
#define MT76X2_BCN_TIME_CFG		0x1114
#define MT76X2_BCN_TIME_CFG_TIMER_EN	(1U << 16)
#define MT76X2_BCN_TIME_CFG_TBTT_EN	(1U << 19)
#define MT76X2_BCN_TIME_CFG_BCN_TX	(1U << 20)
#define MT76X2_BCN_TIME_CFG_SYNC_MODE	0x00060000	/* bits [18:17] */
#define MT76X2_BCN_TIME_CFG_SYNC_SHIFT	17
#define MT76X2_BCN_TIME_CFG_INTVAL_MASK	0x0000ffff

/* RF registers */
#define MT76X2_RF_CSR_CFG		0x0500
#define MT76X2_RF_CSR_DATA		0x000000ff
#define MT76X2_RF_CSR_REG_ID		0x00007f00
#define MT76X2_RF_CSR_REG_BANK		0x00038000
#define MT76X2_RF_CSR_WR		(1U << 30)
#define MT76X2_RF_CSR_KICK		(1U << 31)

#define MT76X2_RF_BYPASS0		0x0504
#define MT76X2_RF_BYPASS1		0x0508
#define MT76X2_RF_SETTING0		0x050c
#define MT76X2_RF_CTRL			0x0528
#define MT76X2_RF_CTRL_ADDR		0x00000fff
#define MT76X2_RF_CTRL_WRITE		(1U << 12)
#define MT76X2_RF_CTRL_BUSY		(1U << 13)
#define MT76X2_RF_CTRL_IDX		(1U << 16)
#define MT76X2_RF_DATA_WRITE		0x0524
#define MT76X2_RF_DATA_READ		0x052c

/* MCU registers */
#define MT76X2_MCU_COM_REG0		0x0730
#define MT76X2_MCU_COM_REG1		0x0734
#define MT76X2_MCU_COM_REG2		0x0738
#define MT76X2_MCU_COM_REG3		0x073c
#define MT76X2_MCU_SEMAPHORE_00		0x07b0
#define MT76X2_MCU_SEMAPHORE_03		0x07bc
#define MT76X2_MCU_CLOCK_CTL		0x0708
#define MT76X2_MCU_RESET_CTL		0x070c
#define MT76X2_MCU_INT_LEVEL		0x0718

/* LED registers */
#define MT76X2_LED_CTRL			0x0770
#define MT76X2_LED_TX_BLINK_0		0x0774
#define MT76X2_LED_TX_BLINK_1		0x0778
#define MT76X2_LED_S0_BASE		0x077c
#define MT76X2_LED_S1_BASE		0x0780
#define MT76X2_LED_CFG			0x102c

/* MAC registers */
#define MT76X2_MAC_CSR0			0x1000
#define MT76X2_MAC_SYS_CTRL		0x1004
#define MT76X2_MAC_RESET_CSR		(1U << 0)
#define MT76X2_MAC_RESET_BBP		(1U << 1)
#define MT76X2_MAC_ENABLE_TX		(1U << 2)
#define MT76X2_MAC_ENABLE_RX		(1U << 3)

#define MT76X2_MAC_ADDR_DW0		0x1008
#define MT76X2_MAC_ADDR_DW1		0x100c
#define MT76X2_MAC_BSSID_DW0		0x1010
#define MT76X2_MAC_BSSID_DW1		0x1014
#define MT76X2_MAC_BSSID_DW1_MBSS_MODE	0x00300000
#define MT76X2_MAC_BSSID_DW1_MBSS_LOCAL	(1U << 22)
#define MT76X2_MAC_BSSID_DW1_MBEACON_N	0x07000000

#define MT76X2_MAX_LEN_CFG		0x1018
#define MT76X2_MAX_LEN_AMPDU		0x00003000

#define MT76X2_AMPDU_MAX_LEN_20M1S	0x1030
#define MT76X2_AMPDU_MAX_LEN_20M2S	0x1034
#define MT76X2_AMPDU_MAX_LEN_40M1S	0x1038
#define MT76X2_AMPDU_MAX_LEN_40M2S	0x103c
#define MT76X2_AMPDU_MAX_LEN		0x1040

/* XIFS timing */
#define MT76X2_XIFS_TIME_CFG		0x1100

/* EDCA parameters */
#define MT76X2_EDCA_CFG_AC(n)		(0x1300 + ((n) << 2))

/* TX pin configuration — controls PA, LNA, and antenna switch pins */
#define MT76X2_TX_PIN_CFG		0x1328
#define MT76X2_TX_PIN_CFG_TXANT		0x0000000f	/* bits [3:0] */
#define MT76X2_TX_PIN_CFG_RXANT		0x00000f00	/* bits [11:8] */
#define MT76X2_TX_PIN_PA_PE_A0		(1U << 12)
#define MT76X2_TX_PIN_PA_PE_A1		(1U << 13)
#define MT76X2_TX_PIN_PA_PE_G0		(1U << 14)
#define MT76X2_TX_PIN_PA_PE_G1		(1U << 15)
#define MT76X2_TX_PIN_RFTR_EN		(1U << 16)
#define MT76X2_TX_PIN_TRSW_EN		(1U << 18)

/* TX timing */
#define MT76X2_TX_TIMEOUT_CFG		0x1348
#define MT76X2_TX_RTY_CFG		0x134c
#define MT76X2_TX_LINK_CFG		0x1350
#define MT76X2_TX_LINK_CFG_CFACK_EN	(1U << 12)
#define MT76X2_HT_FBK_CFG0		0x1354
#define MT76X2_HT_FBK_CFG1		0x1358
#define MT76X2_LG_FBK_CFG0		0x135c
#define MT76X2_LG_FBK_CFG1		0x1360
#define MT76X2_CCK_PROT_CFG		0x1364
#define MT76X2_OFDM_PROT_CFG		0x1368
#define MT76X2_MM20_PROT_CFG		0x136c
#define MT76X2_MM40_PROT_CFG		0x1370
#define MT76X2_GF20_PROT_CFG		0x1374
#define MT76X2_GF40_PROT_CFG		0x1378
#define MT76X2_TX_PROT_CFG6		0x13e0
#define MT76X2_TX_PROT_CFG7		0x13e4
#define MT76X2_TX_PROT_CFG8		0x13e8

/* Protection config register bit fields */
#define MT76X2_PROT_CFG_RATE		0x0000ffff	/* bits [15:0] */
#define MT76X2_PROT_CFG_CTRL		0x00030000	/* bits [17:16] */
#define MT76X2_PROT_CTRL_RTS_CTS	(1U << 16)
#define MT76X2_PROT_CTRL_CTS2SELF	(1U << 17)
#define MT76X2_PROT_RATE_CCK_11	0x0003
#define MT76X2_PROT_RATE_OFDM_24	0x2004
#define MT76X2_PROT_RATE_DUP_OFDM_24	0x2084
#define MT76X2_PROT_RATE_SGI_OFDM_24	0x2104

#define MT76X2_EXP_CTS_TIME		0x137c
#define MT76X2_EXP_ACK_TIME		0x1380
#define MT76X2_TXOP_CTRL_CFG		0x1340
#define MT76X2_TXOP_ED_CCA_EN		(1U << 20)
#define MT76X2_TXOP_HLDR_ET		0x1608
#define MT76X2_TXOP_HLDR_TX40M_BLK_EN	(1U << 1)
#define MT76X2_ED_CCA_TIMER		0x1140

/* TX status counters (clear on read) */
#define MT76X2_TX_STA_CNT0		0x170c	/* [15:0]=fail count */
#define MT76X2_TX_STA_CNT1		0x1710	/* [15:0]=success, [31:16]=retry */
#define MT76X2_TX_STA_CNT2		0x1714	/* [15:0]=zero-len AMPDU, [31:16]=ok */

/* TX status FIFO (per-frame) */
#define MT76X2_TX_STAT_FIFO		0x1718
#define MT76X2_TX_STAT_FIFO_VALID	(1U << 0)
#define MT76X2_TX_STAT_FIFO_SUCCESS	(1U << 5)
#define MT76X2_TX_STAT_FIFO_AGGR	(1U << 6)
#define MT76X2_TX_STAT_FIFO_ACKREQ	(1U << 7)
#define MT76X2_TX_STAT_FIFO_WCID_SHIFT	8
#define MT76X2_TX_STAT_FIFO_RATE_SHIFT	16
#define MT76X2_TX_STAT_FIFO_EXT		0x1798
#define MT76X2_TX_STAT_FIFO_EXT_RETRY	0x00ff
#define MT76X2_TX_STAT_FIFO_EXT_PKTID_SHIFT 8

/* TX pktid encoding (matches Linux MT_PACKET_ID_*) */
#define MT76X2_PKTID_NO_ACK		0	/* no TX status */
#define MT76X2_PKTID_HAS_RATE		0x80	/* BIT(7): rate encoded */
#define MT76X2_PKTID_RATE_MASK		0x1f	/* [4:0]: rate index */
#define MT76X2_PKTID_AC_SHIFT		5	/* [6:5]: AC queue */

/* RX filter config */
#define MT76X2_RX_FILTR_CFG		0x1400
#define MT76X2_RX_FILTR_DROP_CRC	(1U << 0)
#define MT76X2_RX_FILTR_DROP_PHY	(1U << 1)
#define MT76X2_RX_FILTR_DROP_UA		(1U << 2)
#define MT76X2_RX_FILTR_DROP_NOT_MYBSS	(1U << 3)
#define MT76X2_RX_FILTR_DROP_VER_ERR	(1U << 4)
#define MT76X2_RX_FILTR_DROP_MC		(1U << 5)
#define MT76X2_RX_FILTR_DROP_BC		(1U << 6)
#define MT76X2_RX_FILTR_DROP_DUPL	(1U << 7)
#define MT76X2_RX_FILTR_DROP_CFACK	(1U << 8)
#define MT76X2_RX_FILTR_DROP_CFEND	(1U << 9)
#define MT76X2_RX_FILTR_DROP_ACK	(1U << 10)
#define MT76X2_RX_FILTR_DROP_CTS	(1U << 11)
#define MT76X2_RX_FILTR_DROP_RTS	(1U << 12)
#define MT76X2_RX_FILTR_DROP_PSPOLL	(1U << 13)
#define MT76X2_RX_FILTR_DROP_BA		(1U << 14)
#define MT76X2_RX_FILTR_DROP_BAR	(1U << 15)
#define MT76X2_RX_FILTR_DROP_CTRL_RSV	(1U << 16)

/* Linux init table default: drop errors, dupes, and most control frames */
#define MT76X2_RX_FILTR_CFG_DEFAULT		\
	(MT76X2_RX_FILTR_DROP_CRC |		\
	 MT76X2_RX_FILTR_DROP_PHY |		\
	 MT76X2_RX_FILTR_DROP_UA |		\
	 MT76X2_RX_FILTR_DROP_VER_ERR |		\
	 MT76X2_RX_FILTR_DROP_DUPL |		\
	 MT76X2_RX_FILTR_DROP_CFACK |		\
	 MT76X2_RX_FILTR_DROP_CFEND |		\
	 MT76X2_RX_FILTR_DROP_ACK |		\
	 MT76X2_RX_FILTR_DROP_CTS |		\
	 MT76X2_RX_FILTR_DROP_RTS |		\
	 MT76X2_RX_FILTR_DROP_BA |		\
	 MT76X2_RX_FILTR_DROP_CTRL_RSV)

/* MAC status */
#define MT76X2_MAC_STATUS		0x1200
#define MT76X2_MAC_STATUS_TX_BUSY	(1U << 0)
#define MT76X2_MAC_STATUS_RX_BUSY	(1U << 1)

/* Auto-responder config */
#define MT76X2_AUTO_RSP_CFG		0x1404
#define MT76X2_AUTO_RSP_CCK_SHORT	(1U << 4)
#define MT76X2_AUTO_RSP_EN		(1U << 0)
#define MT76X2_BAC_ACK_POLICY		(1U << 2)
#define MT76X2_AUTO_RSP_PREAMBLE	(1U << 4)
#define MT76X2_CTS_40M_MODE		(1U << 8)
#define MT76X2_CTS_40M_REF		(1U << 9)

/* Legacy basic rate */
#define MT76X2_LEGACY_BASIC_RATE	0x1408
#define MT76X2_HT_BASIC_RATE		0x140c
#define MT76X2_HT_CTRL_CFG		0x1410

/* WCID table */
#define MT76X2_WCID_ENTRY_SIZE		8
#define MT76X2_WCID_ENTRY(n)		(0x1800 + ((n) * MT76X2_WCID_ENTRY_SIZE))
#define MT76X2_WCID_ATTR_BASE		0xa800
#define MT76X2_WCID_ATTR(n)		(MT76X2_WCID_ATTR_BASE + ((n) * 4))
#define MT76X2_WCID_ATTR_PAIRWISE	(1U << 0)
#define MT76X2_WCID_ATTR_CIPHER_SHIFT	1
#define MT76X2_WCID_ATTR_CIPHER_MASK	0x0000000e
#define MT76X2_WCID_ATTR_BSS_IDX_SHIFT	4
#define MT76X2_WCID_ATTR_BSS_IDX_MASK	0x00000070

/* Hardware cipher modes (WCID_ATTR bits [3:1]) */
#define MT76X2_CIPHER_NONE		0
#define MT76X2_CIPHER_WEP40		1
#define MT76X2_CIPHER_WEP104		2
#define MT76X2_CIPHER_TKIP		3
#define MT76X2_CIPHER_AES_CCMP		4

/* Pairwise key table (per-WCID) */
#define MT76X2_WCID_KEY_BASE		0x8000
#define MT76X2_WCID_KEY(n)		(MT76X2_WCID_KEY_BASE + ((n) * 32))

/* IV/EIV table (per-WCID) */
#define MT76X2_IVEIV_BASE		0xa000
#define MT76X2_IVEIV(n)			(MT76X2_IVEIV_BASE + ((n) * 8))

/* Shared key table (per-BSS, 4 keys each) */
#define MT76X2_SKEY_BASE_0		0xac00
#define MT76X2_SKEY_BASE_1		0xb400
#define MT76X2_SKEY_0(bss, kidx)	(MT76X2_SKEY_BASE_0 + (4 * (bss) + (kidx)) * 32)
#define MT76X2_SKEY_1(bss, kidx)	(MT76X2_SKEY_BASE_1 + (4 * ((bss) & 7) + (kidx)) * 32)
#define MT76X2_SKEY(bss, kidx)		(((bss) & 8) ? MT76X2_SKEY_1(bss, kidx) : MT76X2_SKEY_0(bss, kidx))
#define MT76X2_SKEY_MODE_BASE_0		0xb000
#define MT76X2_SKEY_MODE_BASE_1		0xb3f0
#define MT76X2_SKEY_MODE_0(bss)		(MT76X2_SKEY_MODE_BASE_0 + (((bss) / 2) << 2))
#define MT76X2_SKEY_MODE_1(bss)		(MT76X2_SKEY_MODE_BASE_1 + ((((bss) & 7) / 2) << 2))
#define MT76X2_SKEY_MODE(bss)		(((bss) & 8) ? MT76X2_SKEY_MODE_1(bss) : MT76X2_SKEY_MODE_0(bss))

/* Beacon */
#define MT76X2_BCN_BASE			0xc000
#define MT76X2_BCN_SIZE			512

/* BBP register groups (via USB vendor read/write) */
/* Per-group base addresses matching Linux mt76x02_regs.h */
#define MT76X2_BBP_CORE_BASE		0x2000
#define MT76X2_BBP_IBI_BASE		0x2100
#define MT76X2_BBP_AGC_BASE		0x2300
#define MT76X2_BBP_TXC_BASE		0x2400
#define MT76X2_BBP_RXC_BASE		0x2500
#define MT76X2_BBP_TXO_BASE		0x2600
#define MT76X2_BBP_TXBE_BASE		0x2700
#define MT76X2_BBP_RXFE_BASE		0x2800
#define MT76X2_BBP_RXO_BASE		0x2900

#define MT76X2_BBP(_grp, _reg)		\
	(MT76X2_BBP_##_grp##_BASE + ((_reg) << 2))

/* TX band configuration */
#define MT76X2_TX_BAND_CFG		0x132c
#define MT76X2_TX_BAND_CFG_UPPER_40M	(1U << 0)
#define MT76X2_TX_BAND_CFG_5G		(1U << 1)
#define MT76X2_TX_BAND_CFG_2G		(1U << 2)

/* TX RTS config */
#define MT76X2_TX_RTS_CFG		0x1344

/* TX/RX delay config */
#define MT76X2_TX_SW_CFG0		0x1330
#define MT76X2_TX_SW_CFG1		0x1334

/* Extended CCA configuration */
#define MT76X2_EXT_CCA_CFG		0x141c

/* PA mode registers */
#define MT76X2_BB_PA_MODE_CFG0		0x1214
#define MT76X2_BB_PA_MODE_CFG1		0x1218
#define MT76X2_RF_PA_MODE_CFG0		0x121c
#define MT76X2_RF_PA_MODE_CFG1		0x1220
#define MT76X2_RF_PA_MODE_ADJ0		0x1228
#define MT76X2_RF_PA_MODE_ADJ1		0x122c

/* TX power registers */
#define MT76X2_TX_ALC_CFG_0		0x13b0
#define MT76X2_TX_ALC_CFG_0_CH_INIT_0	0x0000003f	/* bits [5:0] */
#define MT76X2_TX_ALC_CFG_0_CH_INIT_1	0x00003f00	/* bits [13:8] */
#define MT76X2_TX_ALC_CFG_1		0x13b4
#define MT76X2_TX_ALC_CFG_2		0x13a8
#define MT76X2_TX_ALC_CFG_3		0x13ac
#define MT76X2_TX_ALC_CFG_4		0x13c0
#define MT76X2_TX0_RF_GAIN_CORR		0x13a0
#define MT76X2_TX1_RF_GAIN_CORR		0x13a4
/* TX_PWR_CFG_0..4 are contiguous at 0x1314+n*4 */
#define MT76X2_TX_PWR_CFG_0		0x1314
#define MT76X2_TX_PWR_CFG_1		0x1318
#define MT76X2_TX_PWR_CFG_2		0x131c
#define MT76X2_TX_PWR_CFG_3		0x1320
#define MT76X2_TX_PWR_CFG_4		0x1324
/* TX_PWR_CFG_7..9 are NOT contiguous with 0..4 */
#define MT76X2_TX_PWR_CFG_7		0x13d4
#define MT76X2_TX_PWR_CFG_8		0x13d8
#define MT76X2_TX_PWR_CFG_9		0x13dc

/* MCU command types */
#define MT76X2_MCU_CMD_FUN_SET		0x01
#define MT76X2_MCU_CMD_LOAD_CR		0x02
#define MT76X2_MCU_CMD_BURST_WRITE	0x08
#define MT76X2_MCU_CMD_RANDOM_READ	0x0a
#define MT76X2_MCU_CMD_RANDOM_WRITE	0x0c
#define MT76X2_MCU_CMD_CALIBRATION	0x1f

/* MCU calibration IDs (used with CMD_CALIBRATION, from Linux mt76x2/mcu.h) */
#define MT76X2_MCU_CAL_R		1
#define MT76X2_MCU_CAL_TEMP_SENSOR	2
#define MT76X2_MCU_CAL_RXDCOC		3
#define MT76X2_MCU_CAL_RC		4
#define MT76X2_MCU_CAL_SX_LOGEN	5
#define MT76X2_MCU_CAL_LC		6
#define MT76X2_MCU_CAL_TX_LOFT		7
#define MT76X2_MCU_CAL_TXIQ		8
#define MT76X2_MCU_CAL_TSSI		9
#define MT76X2_MCU_CAL_TSSI_COMP	10
#define MT76X2_MCU_CAL_DPD		11
#define MT76X2_MCU_CAL_RXIQC_FI	12
#define MT76X2_MCU_CAL_RXIQC_FD	13
#define MT76X2_MCU_CAL_PWRON		14
#define MT76X2_MCU_CAL_TX_SHAPING	15
#define MT76X2_MCU_CMD_LED_MODE_OP	0x10
#define MT76X2_MCU_CMD_POWER_SAVING	0x14
#define MT76X2_MCU_CMD_SWITCH_CHANNEL	0x1e
#define MT76X2_MCU_CMD_INIT_GAIN	0x03

/* MCU function selections */
#define MT76X2_Q_SELECT			1
#define MT76X2_BW_SETTING		17

/* MCU message info bits - matches Linux mt76x02_dma.h */
#define MT76X2_MCU_MSG_LEN_SHIFT	0
#define MT76X2_MCU_MSG_LEN_MASK		0x0000ffff
#define MT76X2_MCU_MSG_CMD_SEQ_SHIFT	16
#define MT76X2_MCU_MSG_CMD_SEQ_MASK	0x000f0000
#define MT76X2_MCU_MSG_CMD_TYPE_SHIFT	20
#define MT76X2_MCU_MSG_CMD_TYPE_MASK	0x07f00000
#define MT76X2_MCU_MSG_PORT_SHIFT	27
#define MT76X2_MCU_MSG_PORT_MASK	0x38000000
#define MT76X2_MCU_MSG_TYPE_CMD		(1U << 30)

/* MCU response info */
#define MT76X2_RX_FCE_INFO_CMD_SEQ	0x00000f00
#define MT76X2_RX_FCE_INFO_EVT_TYPE	0x000000f0
#define MT76X2_EVT_CMD_DONE		0x0e
#define MT76X2_EVT_RADAR		0x05	/* DFS radar detection event */

/* Temperature sensor (BBP space, bits [6:0] = raw temp value) */
#define MT76X2_TEMP_SENSOR		0x1d000
#define MT76X2_TEMP_SENSOR_VAL		0x7f

/* RX status counters (clear on read) */
#define MT76X2_RX_STAT_0		0x1700
#define MT76X2_RX_STAT_1		0x1704
#define MT76X2_RX_STAT_1_CCA_ERRORS	0x0000ffff	/* bits [15:0] */
#define MT76X2_RX_STAT_2		0x1708

/* TX/RX queue status (for MAC idle polling) */
#define MT76X2_TXQ_STA			0x0434
#define MT76X2_RXQ_STA			0x0430

/* MAC error detection register */
#define MT76X2_MAC_ERR_STATUS		0x10f4

/* CPU TX port */
#define MT76X2_CPU_TX_PORT		2
#define MT76X2_WLAN_PORT		0

/*
 * TX DMA Info header (TXD) — 4 bytes prepended to every USB TX frame.
 * Tells the firmware how to route the frame (WLAN data vs MCU command).
 * Matches Linux MT_TXD_INFO_* defines in mt76x02_dma.h.
 */
#define MT76X2_TXD_NEXT_VLD		(1U << 16)	/* more frames follow */
#define MT76X2_TXD_TX_BURST		(1U << 17)
#define MT76X2_TXD_80211		(1U << 19)
#define MT76X2_TXD_WIV			(1U << 24)
#define MT76X2_TXD_QSEL_SHIFT		25
#define MT76X2_TXD_QSEL_MGMT		(0U << 25)
#define MT76X2_TXD_QSEL_HCCA		(1U << 25)
#define MT76X2_TXD_QSEL_EDCA		(2U << 25)
#define MT76X2_TXD_DPORT_SHIFT		27
#define MT76X2_TXD_TYPE_SHIFT		30

/* EEPROM offsets */
#define MT76X2_EE_SIZE			512
#define MT76X2_EE_MAC_ADDR		0x04
#define MT76X2_EE_VERSION		0x02
#define MT76X2_EE_NIC_CONF0		0x34
#define MT76X2_EE_NIC_CONF1		0x36
#define MT76X2_EE_NIC_CONF2		0x42
#define MT76X2_EE_WIFI_RF_SETTING	0x48
#define MT76X2_EE_TX_POWER_DELTA_BW40	0x50
#define MT76X2_EE_TX_POWER_DELTA_BW80	0x52
#define MT76X2_EE_TX_POWER_0_START_2G	0x56
#define MT76X2_EE_TX_POWER_1_START_2G	0x5c
#define MT76X2_EE_TX_POWER_EXT_PA_5G	0x54
#define MT76X2_TX_POWER_GROUP_SIZE_5G	5
#define MT76X2_TX_POWER_GROUPS_5G	6
#define MT76X2_EE_TX_POWER_0_START_5G	0x62
#define MT76X2_EE_TSSI_SLOPE_2G		0x6e
#define MT76X2_EE_TX_POWER_1_START_5G	0x80
#define MT76X2_EE_TX_POWER_CCK		0xa0
#define MT76X2_EE_TX_POWER_OFDM_2G_6M	0xa2
#define MT76X2_EE_TX_POWER_OFDM_2G_24M	0xa4
#define MT76X2_EE_TX_POWER_HT_MCS0	0xa6
#define MT76X2_EE_TX_POWER_HT_MCS4	0xa8
#define MT76X2_EE_TX_POWER_HT_MCS8	0xaa
#define MT76X2_EE_TX_POWER_HT_MCS12	0xac
#define MT76X2_EE_TX_POWER_OFDM_5G_6M	0xb2
#define MT76X2_EE_TX_POWER_OFDM_5G_24M	0xb4
#define MT76X2_EE_TX_POWER_VHT_MCS8	0xbe
#define MT76X2_EE_2G_TARGET_POWER	0xd0
#define MT76X2_EE_5G_TARGET_POWER	0xd2
#define MT76X2_EE_RF_2G_TSSI_OFF_TXPOWER 0xf6
/*
 * RSSI offset and LNA gain EEPROM byte offsets.
 * Derived from Linux mt76x02_eeprom.h word addresses:
 *   MT_EE_RSSI_OFFSET_2G_0 (0x046): [0]=chain0, [1]=chain1
 *   MT_EE_LNA_GAIN (0x044): [0]=2G, [1]=5G group 0
 *   MT_EE_RSSI_OFFSET_2G_1 (0x048): [1]=5G LNA group 1
 *   MT_EE_RSSI_OFFSET_5G_0 (0x04a): [0]=chain0, [1]=chain1
 *   MT_EE_RSSI_OFFSET_5G_1 (0x04c): [1]=5G LNA group 2
 */
#define MT76X2_EE_RSSI_OFFSET_2G_0	0x46
#define MT76X2_EE_RSSI_OFFSET_2G_1	0x47
#define MT76X2_EE_LNA_GAIN		0x44
#define MT76X2_EE_LNA_GAIN_2G		0x44	/* byte 0 of word at 0x44 */
#define MT76X2_EE_LNA_GAIN_5G_0	0x45	/* byte 1 of word at 0x44 */
#define MT76X2_EE_RSSI_OFFSET_5G_0	0x4a
#define MT76X2_EE_RSSI_OFFSET_5G_1	0x4b
#define MT76X2_EE_LNA_GAIN_5G_1	0x49	/* byte 1 of word at 0x48 */
#define MT76X2_EE_LNA_GAIN_5G_2	0x4d	/* byte 1 of word at 0x4c */

/* Per-channel RX high gain from EEPROM (packed 2×4-bit per chain) */
#define MT76X2_EE_RF_2G_RX_HIGH_GAIN	0xf8
#define MT76X2_EE_RF_5G_GRP01_RX_HIGH_GAIN 0xfa
#define MT76X2_EE_RF_5G_GRP23_RX_HIGH_GAIN 0xfc
#define MT76X2_EE_RF_5G_GRP45_RX_HIGH_GAIN 0xfe
#define MT76X2_EE_BT_RCAL_RESULT	0x138

/* NIC_CONF_1 LNA external flags */
#define MT76X2_EE_NIC_CONF1_LNA_EXT_2G	(1U << 2)
#define MT76X2_EE_NIC_CONF1_LNA_EXT_5G	(1U << 3)

#define MT76X2_EE_NIC_CONF0_RX_PATH	0x000f
#define MT76X2_EE_NIC_CONF0_TX_PATH	0x00f0
#define MT76X2_EE_NIC_CONF0_PA_TYPE	0x0300
#define MT76X2_EE_NIC_CONF0_BOARD_TYPE	0x6000

/* Firmware */
#define MT76X2_FW_HDR_SIZE		36
#define MT76X2_MCU_ILM_OFFSET		0x80000
#define MT76X2_MCU_DLM_OFFSET		0x110000
#define MT76X2_MCU_ROM_PATCH_OFFSET	0x90000
#define MT76X2_FW_MAX_PAYLOAD		0x3900
#define MT76X2_ROM_PATCH_MAX_PAYLOAD	2048

/* Firmware header structure */
struct mt76x2_fw_header {
	uint32_t	ilm_len;
	uint32_t	dlm_len;
	uint16_t	build_ver;
	uint16_t	fw_ver;
	uint8_t		pad[4];
	char		build_time[16];
} __packed;

/* ROM patch header */
struct mt76x2_patch_header {
	char		build_time[16];
	char		platform[4];
	char		hw_version[4];
	char		patch_version[4];
	uint16_t	pad;
} __packed;

/* TX descriptor (USB DMA info) */
struct mt76x2_txd {
	uint32_t	info;	/* MCU port, len, cmd flags */
} __packed;

/* TX Wireless Info (TXWI) - prepended to every TX frame */
/*
 * TX Wireless Info (TXWI) — 20 bytes prepended to every TX frame.
 * Matches Linux mt76x02_txwi (drivers/net/wireless/mediatek/mt76/mt76x02_mac.h).
 */
struct mt76x2_txwi {
	uint16_t	flags;		/* LE: FRAG, MMPS, CFACK, TS, AMPDU, etc. */
#define MT76X2_TXWI_FRAG	(1U << 0)
#define MT76X2_TXWI_MMPS	(1U << 1)
#define MT76X2_TXWI_CFACK	(1U << 2)
#define MT76X2_TXWI_TS		(1U << 3)
#define MT76X2_TXWI_AMPDU	(1U << 4)
#define MT76X2_TXWI_MPDU_DENSITY_SHIFT	5
#define MT76X2_TXWI_MPDU_DENSITY_MASK	0x00e0	/* flags bits [7:5] */
#define MT76X2_TXWI_TXOP_SHIFT	8
#define MT76X2_TXWI_TXOP_HT	(0U << 8)
#define MT76X2_TXWI_TXOP_PIFS	(1U << 8)
#define MT76X2_TXWI_TXOP_SIFS	(2U << 8)
#define MT76X2_TXWI_TXOP_BACKOFF (3U << 8)
#define MT76X2_TXWI_NSEQ	(1U << 13)
#define MT76X2_TXWI_SOUND	(1U << 14)
	uint16_t	rate;		/* LE: MCS, BW, SGI, STBC, LDPC, PHY mode */
#define MT76X2_TXWI_MCS_MASK	0x003f
#define MT76X2_TXWI_MCS_SHIFT	0
#define MT76X2_TXWI_LDPC	(1U << 6)
#define MT76X2_TXWI_BW_SHIFT	7
#define MT76X2_TXWI_SGI		(1U << 9)
#define MT76X2_TXWI_STBC	(1U << 10)
#define MT76X2_TXWI_PHY_SHIFT	13
#define MT76X2_TXWI_PHY_MASK	0xe000
#define MT76X2_TXWI_PHY_CCK	0
#define MT76X2_TXWI_PHY_OFDM	1
#define MT76X2_TXWI_PHY_HT	2
#define MT76X2_TXWI_PHY_HT_GF	3
#define MT76X2_TXWI_PHY_VHT	4
	uint8_t		ack_ctl;	/* ACK_REQ, NSEQ, BA_WINDOW */
#define MT76X2_TXWI_ACK		(1U << 0)
#define MT76X2_TXWI_NSEQ_CTL	(1U << 1)
#define MT76X2_TXWI_BA_WINDOW_SHIFT	2
#define MT76X2_TXWI_BA_WINDOW_MAX	63
	uint8_t		wcid;		/* Wireless Client ID */
	uint16_t	len_ctl;	/* LE: MPDU length */
	uint32_t	iv;		/* IV (for HW encryption) */
	uint32_t	eiv;		/* EIV (for HW encryption) */
	uint8_t		aid;		/* Association ID */
	uint8_t		txstream;	/* TX stream selection */
#define MT76X2_TXWI_TXSTREAM_E4	0x13	/* E4+ silicon: both chains */
	uint8_t		ctl2;		/* additional control */
	uint8_t		pktid;		/* Packet ID */
} __packed;

/*
 * RX Wireless Info (RXWI) — 32 bytes prepended to every received frame.
 * Matches Linux mt76x02_rxwi (drivers/net/wireless/mediatek/mt76/mt76x02_mac.h).
 */
struct mt76x2_rxwi {
	uint32_t	rxinfo;		/* word 0: flags (CRC_ERR, L2PAD, etc.) */
#define MT76X2_RXINFO_CRC_ERR	(1U << 8)
#define MT76X2_RXINFO_ICVERR	(1U << 9)
#define MT76X2_RXINFO_MICERR	(1U << 10)
#define MT76X2_RXINFO_L2PAD	(1U << 14)
#define MT76X2_RXINFO_DECRYPT	(1U << 16)
#define MT76X2_RXINFO_PN_LEN_SHIFT	19
#define MT76X2_RXINFO_PN_LEN_MASK	(0x7U << 19)	/* bits [21:19] */
	uint32_t	ctl;		/* word 1: WCID, MPDU_LEN */
#define MT76X2_RXWI_CTL_WCID	0x000000ff
#define MT76X2_RXWI_CTL_KEY_IDX 0x00000300
#define MT76X2_RXWI_CTL_BSS_IDX 0x00001c00
#define MT76X2_RXWI_CTL_MPDU_LEN_SHIFT	16
#define MT76X2_RXWI_CTL_MPDU_LEN_MASK	0x3fff0000
	uint16_t	tid_sn;		/* word 2 low: TID + sequence number */
	uint16_t	rate;		/* word 2 high: rate info */
/* Rate field bit layout matches Linux MT_RXWI_RATE_* in mt76x02_mac.h */
#define MT76X2_RXWI_MCS_SHIFT	0
#define MT76X2_RXWI_MCS_MASK	0x003f	/* bits [5:0] */
#define MT76X2_RXWI_LDPC	(1U << 6)	/* bit 6 */
#define MT76X2_RXWI_BW_SHIFT	7
#define MT76X2_RXWI_BW_MASK	0x0180	/* bits [8:7] */
#define MT76X2_RXWI_SGI		(1U << 9)	/* bit 9 */
#define MT76X2_RXWI_STBC	(1U << 10)	/* bit 10 */
#define MT76X2_RXWI_LDPC_EXSYM	(1U << 11)	/* bit 11 */
#define MT76X2_RXWI_PHY_SHIFT	13
#define MT76X2_RXWI_PHY_MASK	0xe000	/* bits [15:13] */
	uint8_t		rssi[4];	/* word 3: per-antenna RSSI */
	uint32_t	bbp_rxinfo[4];	/* words 4-7: BBP RX info */
} __packed;

/* RX DMA descriptor (appended at end of USB bulk rx) */
struct mt76x2_rxd {
	uint32_t	flags;
#define MT76X2_RXD_LAST		(1U << 0)
#define MT76X2_RXD_HTC		(1U << 1)
#define MT76X2_RXD_AMPDU	(1U << 2)
#define MT76X2_RXD_L2PAD	(1U << 3)
#define MT76X2_RXD_AMSDU	(1U << 4)
#define MT76X2_RXD_HW_DEC	(1U << 5)
#define MT76X2_RXD_CRC_ERR	(1U << 8)
#define MT76X2_RXD_MIC_ERR	(1U << 9)
#define MT76X2_RXD_ICVERR	(1U << 10)
#define MT76X2_RXD_LEN_SHIFT	16
#define MT76X2_RXD_LEN_MASK	0x3fff0000
} __packed;

/* Supported device IDs are in usbdevs.h */

/* Default MAC init values for MT7612U */
struct mt76x2_mac_val {
	uint32_t	reg;
	uint32_t	val;
};

/* Firmware names */
#define MT76X2_FW_NAME		"mt7662u.bin"
#define MT76X2_ROM_PATCH_NAME	"mt7662u_rom_patch.bin"

/* Max endpoints */
#define MT76X2_EP_OUT_INBAND_CMD	8
#define MT76X2_EP_IN_CMD_RESP		1

#endif /* _IF_MT76X2REG_H_ */
