// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cadence MACB/GEM Ethernet Controller driver
 *
 * Copyright (C) 2004-2006 Atmel Corporation
 */

#ifndef _NTL_MACB_H
#define _NTL_MACB_H

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/clk.h>
#include <linux/crc32.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/circ_buf.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/pm_runtime.h>
#include <linux/inetdevice.h>
#include <linux/ptp_clock_kernel.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
#include <linux/linkmode.h>
#endif

#ifdef CONFIG_NTL_TSU
#include "ntl_tsu.h"
#endif

#if defined(CONFIG_ARCH_DMA_ADDR_T_64BIT)
#define NTL_MACB_EXT_DESC
#endif

#define NTL_MACB_GREGS_NBR 16
#define NTL_MACB_GREGS_VERSION 2
#define NTL_MACB_MAX_QUEUES 8

/* MACB register offsets */
#define NTL_MACB_NCR		0x0000 /* Network Control */
#define NTL_MACB_NCFGR		0x0004 /* Network Config */
#define NTL_MACB_NSR		0x0008 /* Network Status */
#define NTL_MACB_TAR		0x000c /* AT91RM9200 only */
#define NTL_MACB_TCR		0x0010 /* AT91RM9200 only */
#define NTL_MACB_TSR		0x0014 /* Transmit Status */
#define NTL_MACB_RBQP		0x0018 /* RX Q Base Address */
#define NTL_MACB_TBQP		0x001c /* TX Q Base Address */
#define NTL_MACB_RSR		0x0020 /* Receive Status */
#define NTL_MACB_ISR		0x0024 /* Interrupt Status */
#define NTL_MACB_IER		0x0028 /* Interrupt Enable */
#define NTL_MACB_IDR		0x002c /* Interrupt Disable */
#define NTL_MACB_IMR		0x0030 /* Interrupt Mask */
#define NTL_MACB_MAN		0x0034 /* PHY Maintenance */
#define NTL_MACB_PTR		0x0038
#define NTL_MACB_PFR		0x003c
#define NTL_MACB_FTO		0x0040
#define NTL_MACB_SCF		0x0044
#define NTL_MACB_MCF		0x0048
#define NTL_MACB_FRO		0x004c
#define NTL_MACB_FCSE		0x0050
#define NTL_MACB_ALE		0x0054
#define NTL_MACB_DTF		0x0058
#define NTL_MACB_LCOL		0x005c
#define NTL_MACB_EXCOL		0x0060
#define NTL_MACB_TUND		0x0064
#define NTL_MACB_CSE		0x0068
#define NTL_MACB_RRE		0x006c
#define NTL_MACB_ROVR		0x0070
#define NTL_MACB_RSE		0x0074
#define NTL_MACB_ELE		0x0078
#define NTL_MACB_RJA		0x007c
#define NTL_MACB_USF		0x0080
#define NTL_MACB_STE		0x0084
#define NTL_MACB_RLE		0x0088
#define NTL_MACB_TPF		0x008c
#define NTL_MACB_HRB		0x0090
#define NTL_MACB_HRT		0x0094
#define NTL_MACB_SA1B		0x0098
#define NTL_MACB_SA1T		0x009c
#define NTL_MACB_SA2B		0x00a0
#define NTL_MACB_SA2T		0x00a4
#define NTL_MACB_SA3B		0x00a8
#define NTL_MACB_SA3T		0x00ac
#define NTL_MACB_SA4B		0x00b0
#define NTL_MACB_SA4T		0x00b4
#define NTL_MACB_TID		0x00b8
#define NTL_MACB_TPQ		0x00bc
#define NTL_MACB_USRIO		0x00c0
#define NTL_MACB_WOL		0x00c4
#define NTL_MACB_MID		0x00fc
#define NTL_MACB_TBQPH		0x04C8
#define NTL_MACB_RBQPH		0x04D4

/* GEM register offsets. */
#define NTL_GEM_NCFGR		0x0004 /* Network Config */
#define NTL_GEM_USRIO		0x000c /* User IO */
#define NTL_GEM_DMACFG		0x0010 /* DMA Configuration */
#define NTL_GEM_PBUFRXCUT		0x0044 /* RX Partial Store and Forward */
#define NTL_GEM_JML			0x0048 /* Jumbo Max Length */
#define NTL_GEM_HRB			0x0080 /* Hash Bottom */
#define NTL_GEM_HRT			0x0084 /* Hash Top */
#define NTL_GEM_SA1B		0x0088 /* Specific1 Bottom */
#define NTL_GEM_SA1T		0x008C /* Specific1 Top */
#define NTL_GEM_SA2B		0x0090 /* Specific2 Bottom */
#define NTL_GEM_SA2T		0x0094 /* Specific2 Top */
#define NTL_GEM_SA3B		0x0098 /* Specific3 Bottom */
#define NTL_GEM_SA3T		0x009C /* Specific3 Top */
#define NTL_GEM_SA4B		0x00A0 /* Specific4 Bottom */
#define NTL_GEM_SA4T		0x00A4 /* Specific4 Top */
#define NTL_GEM_WOL			0x00B8 /* Wake on LAN */
#define NTL_GEM_RXPTPUNI		0x00D4 /* PTP RX Unicast address */
#define NTL_GEM_TXPTPUNI		0x00D8 /* PTP TX Unicast address */
#define NTL_GEM_EFTSH		0x00e8 /* PTP Event Frame Transmitted Seconds Register 47:32 */
#define NTL_GEM_EFRSH		0x00ec /* PTP Event Frame Received Seconds Register 47:32 */
#define NTL_GEM_PEFTSH		0x00f0 /* PTP Peer Event Frame Transmitted Seconds Register 47:32 */
#define NTL_GEM_PEFRSH		0x00f4 /* PTP Peer Event Frame Received Seconds Register 47:32 */
#define NTL_GEM_OTX			0x0100 /* Octets transmitted */
#define NTL_GEM_OCTTXL		0x0100 /* Octets transmitted [31:0] */
#define NTL_GEM_OCTTXH		0x0104 /* Octets transmitted [47:32] */
#define NTL_GEM_TXCNT		0x0108 /* Frames Transmitted counter */
#define NTL_GEM_TXBCCNT		0x010c /* Broadcast Frames counter */
#define NTL_GEM_TXMCCNT		0x0110 /* Multicast Frames counter */
#define NTL_GEM_TXPAUSECNT		0x0114 /* Pause Frames Transmitted Counter */
#define NTL_GEM_TX64CNT		0x0118 /* 64 byte Frames TX counter */
#define NTL_GEM_TX65CNT		0x011c /* 65-127 byte Frames TX counter */
#define NTL_GEM_TX128CNT		0x0120 /* 128-255 byte Frames TX counter */
#define NTL_GEM_TX256CNT		0x0124 /* 256-511 byte Frames TX counter */
#define NTL_GEM_TX512CNT		0x0128 /* 512-1023 byte Frames TX counter */
#define NTL_GEM_TX1024CNT		0x012c /* 1024-1518 byte Frames TX counter */
#define NTL_GEM_TX1519CNT		0x0130 /* 1519+ byte Frames TX counter */
#define NTL_GEM_TXURUNCNT		0x0134 /* TX under run error counter */
#define NTL_GEM_SNGLCOLLCNT		0x0138 /* Single Collision Frame Counter */
#define NTL_GEM_MULTICOLLCNT	0x013c /* Multiple Collision Frame Counter */
#define NTL_GEM_EXCESSCOLLCNT	0x0140 /* Excessive Collision Frame Counter */
#define NTL_GEM_LATECOLLCNT		0x0144 /* Late Collision Frame Counter */
#define NTL_GEM_TXDEFERCNT		0x0148 /* Deferred Transmission Frame Counter */
#define NTL_GEM_TXCSENSECNT		0x014c /* Carrier Sense Error Counter */
#define NTL_GEM_ORX			0x0150 /* Octets received */
#define NTL_GEM_OCTRXL		0x0150 /* Octets received [31:0] */
#define NTL_GEM_OCTRXH		0x0154 /* Octets received [47:32] */
#define NTL_GEM_RXCNT		0x0158 /* Frames Received Counter */
#define NTL_GEM_RXBROADCNT		0x015c /* Broadcast Frames Received Counter */
#define NTL_GEM_RXMULTICNT		0x0160 /* Multicast Frames Received Counter */
#define NTL_GEM_RXPAUSECNT		0x0164 /* Pause Frames Received Counter */
#define NTL_GEM_RX64CNT		0x0168 /* 64 byte Frames RX Counter */
#define NTL_GEM_RX65CNT		0x016c /* 65-127 byte Frames RX Counter */
#define NTL_GEM_RX128CNT		0x0170 /* 128-255 byte Frames RX Counter */
#define NTL_GEM_RX256CNT		0x0174 /* 256-511 byte Frames RX Counter */
#define NTL_GEM_RX512CNT		0x0178 /* 512-1023 byte Frames RX Counter */
#define NTL_GEM_RX1024CNT		0x017c /* 1024-1518 byte Frames RX Counter */
#define NTL_GEM_RX1519CNT		0x0180 /* 1519+ byte Frames RX Counter */
#define NTL_GEM_RXUNDRCNT		0x0184 /* Undersize Frames Received Counter */
#define NTL_GEM_RXOVRCNT		0x0188 /* Oversize Frames Received Counter */
#define NTL_GEM_RXJABCNT		0x018c /* Jabbers Received Counter */
#define NTL_GEM_RXFCSCNT		0x0190 /* Frame Check Sequence Error Counter */
#define NTL_GEM_RXLENGTHCNT		0x0194 /* Length Field Error Counter */
#define NTL_GEM_RXSYMBCNT		0x0198 /* Symbol Error Counter */
#define NTL_GEM_RXALIGNCNT		0x019c /* Alignment Error Counter */
#define NTL_GEM_RXRESERRCNT		0x01a0 /* Receive Resource Error Counter */
#define NTL_GEM_RXORCNT		0x01a4 /* Receive Overrun Counter */
#define NTL_GEM_RXIPCCNT		0x01a8 /* IP header Checksum Error Counter */
#define NTL_GEM_RXTCPCCNT		0x01ac /* TCP Checksum Error Counter */
#define NTL_GEM_RXUDPCCNT		0x01b0 /* UDP Checksum Error Counter */
#define NTL_GEM_TISUBN		0x01bc /* 1588 Timer Increment Sub-ns */
#define NTL_GEM_TSH			0x01c0 /* 1588 Timer Seconds High */
#define NTL_GEM_TSL			0x01d0 /* 1588 Timer Seconds Low */
#define NTL_GEM_TN			0x01d4 /* 1588 Timer Nanoseconds */
#define NTL_GEM_TA			0x01d8 /* 1588 Timer Adjust */
#define NTL_GEM_TI			0x01dc /* 1588 Timer Increment */
#define NTL_GEM_EFTSL		0x01e0 /* PTP Event Frame Tx Seconds Low */
#define NTL_GEM_EFTN		0x01e4 /* PTP Event Frame Tx Nanoseconds */
#define NTL_GEM_EFRSL		0x01e8 /* PTP Event Frame Rx Seconds Low */
#define NTL_GEM_EFRN		0x01ec /* PTP Event Frame Rx Nanoseconds */
#define NTL_GEM_PEFTSL		0x01f0 /* PTP Peer Event Frame Tx Secs Low */
#define NTL_GEM_PEFTN		0x01f4 /* PTP Peer Event Frame Tx Ns */
#define NTL_GEM_PEFRSL		0x01f8 /* PTP Peer Event Frame Rx Sec Low */
#define NTL_GEM_PEFRN		0x01fc /* PTP Peer Event Frame Rx Ns */
#define NTL_GEM_PCSCNTRL		0x0200 /* PCS Control */
#define NTL_GEM_DCFG1		0x0280 /* Design Config 1 */
#define NTL_GEM_DCFG2		0x0284 /* Design Config 2 */
#define NTL_GEM_DCFG3		0x0288 /* Design Config 3 */
#define NTL_GEM_DCFG4		0x028c /* Design Config 4 */
#define NTL_GEM_DCFG5		0x0290 /* Design Config 5 */
#define NTL_GEM_DCFG6		0x0294 /* Design Config 6 */
#define NTL_GEM_DCFG7		0x0298 /* Design Config 7 */
#define NTL_GEM_DCFG8		0x029C /* Design Config 8 */
#define NTL_GEM_DCFG10		0x02A4 /* Design Config 10 */

#define NTL_GEM_TXBDCTRL	0x04cc /* TX Buffer Descriptor control register */
#define NTL_GEM_RXBDCTRL	0x04d0 /* RX Buffer Descriptor control register */

/* Screener Type 2 match registers */
#define NTL_GEM_SCRT2		0x540

/* EtherType registers */
#define NTL_GEM_ETHT		0x06E0

/* Type 2 compare registers */
#define NTL_GEM_T2CMPW0		0x0700
#define NTL_GEM_T2CMPW1		0x0704
#define T2CMP_OFST(t2idx)	(t2idx * 2)

/* type 2 compare registers
 * each location requires 3 compare regs
 */
#define NTL_GEM_IP4SRC_CMP(idx)		(idx * 3)
#define NTL_GEM_IP4DST_CMP(idx)		(idx * 3 + 1)
#define NTL_GEM_PORT_CMP(idx)		(idx * 3 + 2)

/* Which screening type 2 EtherType register will be used (0 - 7) */
#define SCRT2_ETHT		0

#define NTL_GEM_ISR(hw_q)		(0x0400 + ((hw_q) << 2))
#define NTL_GEM_TBQP(hw_q)		(0x0440 + ((hw_q) << 2))
#define NTL_GEM_TBQPH(hw_q)		(0x04C8)
#define NTL_GEM_RBQP(hw_q)		(0x0480 + ((hw_q) << 2))
#define NTL_GEM_RBQS(hw_q)		(0x04A0 + ((hw_q) << 2))
#define NTL_GEM_RBQPH(hw_q)		(0x04D4)
#define NTL_GEM_IER(hw_q)		(0x0600 + ((hw_q) << 2))
#define NTL_GEM_IDR(hw_q)		(0x0620 + ((hw_q) << 2))
#define NTL_GEM_IMR(hw_q)		(0x0640 + ((hw_q) << 2))

/* Bitfields in NCR */
#define NTL_MACB_LB_OFFSET		0 /* reserved */
#define NTL_MACB_LB_SIZE		1
#define NTL_MACB_LLB_OFFSET		1 /* Loop back local */
#define NTL_MACB_LLB_SIZE		1
#define NTL_MACB_RE_OFFSET		2 /* Receive enable */
#define NTL_MACB_RE_SIZE		1
#define NTL_MACB_TE_OFFSET		3 /* Transmit enable */
#define NTL_MACB_TE_SIZE		1
#define NTL_MACB_MPE_OFFSET		4 /* Management port enable */
#define NTL_MACB_MPE_SIZE		1
#define NTL_MACB_CLRSTAT_OFFSET	5 /* Clear stats regs */
#define NTL_MACB_CLRSTAT_SIZE	1
#define NTL_MACB_INCSTAT_OFFSET	6 /* Incremental stats regs */
#define NTL_MACB_INCSTAT_SIZE	1
#define NTL_MACB_WESTAT_OFFSET	7 /* Write enable stats regs */
#define NTL_MACB_WESTAT_SIZE	1
#define NTL_MACB_BP_OFFSET		8 /* Back pressure */
#define NTL_MACB_BP_SIZE		1
#define NTL_MACB_TSTART_OFFSET	9 /* Start transmission */
#define NTL_MACB_TSTART_SIZE	1
#define NTL_MACB_THALT_OFFSET	10 /* Transmit halt */
#define NTL_MACB_THALT_SIZE		1
#define NTL_MACB_NCR_TPF_OFFSET	11 /* Transmit pause frame */
#define NTL_MACB_NCR_TPF_SIZE	1
#define NTL_MACB_TZQ_OFFSET		12 /* Transmit zero quantum pause frame */
#define NTL_MACB_TZQ_SIZE		1
#define NTL_MACB_SRTSM_OFFSET	15
#define NTL_MACB_PTPUNI_OFFSET			20
#define NTL_MACB_PTPUNI_SIZE			1
#define NTL_MACB_OSSMODE_OFFSET 24 /* Enable One Step Synchro Mode */
#define NTL_MACB_OSSMODE_SIZE	1

/* Bitfields in NCFGR */
#define NTL_MACB_SPD_OFFSET		0 /* Speed */
#define NTL_MACB_SPD_SIZE		1
#define NTL_MACB_FD_OFFSET		1 /* Full duplex */
#define NTL_MACB_FD_SIZE		1
#define NTL_MACB_BIT_RATE_OFFSET	2 /* Discard non-VLAN frames */
#define NTL_MACB_BIT_RATE_SIZE	1
#define NTL_MACB_JFRAME_OFFSET	3 /* reserved */
#define NTL_MACB_JFRAME_SIZE	1
#define NTL_MACB_CAF_OFFSET		4 /* Copy all frames */
#define NTL_MACB_CAF_SIZE		1
#define NTL_MACB_NBC_OFFSET		5 /* No broadcast */
#define NTL_MACB_NBC_SIZE		1
#define NTL_MACB_NCFGR_MTI_OFFSET	6 /* Multicast hash enable */
#define NTL_MACB_NCFGR_MTI_SIZE	1
#define NTL_MACB_UNI_OFFSET		7 /* Unicast hash enable */
#define NTL_MACB_UNI_SIZE		1
#define NTL_MACB_BIG_OFFSET		8 /* Receive 1536 byte frames */
#define NTL_MACB_BIG_SIZE		1
#define NTL_MACB_EAE_OFFSET		9 /* External address match enable */
#define NTL_MACB_EAE_SIZE		1
#define NTL_MACB_CLK_OFFSET		10
#define NTL_MACB_CLK_SIZE		2
#define NTL_MACB_RTY_OFFSET		12 /* Retry test */
#define NTL_MACB_RTY_SIZE		1
#define NTL_MACB_PAE_OFFSET		13 /* Pause enable */
#define NTL_MACB_PAE_SIZE		1
#define NTL_MACB_RM9200_RMII_OFFSET	13 /* AT91RM9200 only */
#define NTL_MACB_RM9200_RMII_SIZE	1  /* AT91RM9200 only */
#define NTL_MACB_RBOF_OFFSET	14 /* Receive buffer offset */
#define NTL_MACB_RBOF_SIZE		2
#define NTL_MACB_RLCE_OFFSET	16 /* Length field error frame discard */
#define NTL_MACB_RLCE_SIZE		1
#define NTL_MACB_DRFCS_OFFSET	17 /* FCS remove */
#define NTL_MACB_DRFCS_SIZE		1
#define NTL_MACB_EFRHD_OFFSET	18
#define NTL_MACB_EFRHD_SIZE		1
#define NTL_MACB_IRXFCS_OFFSET	19
#define NTL_MACB_IRXFCS_SIZE	1

/* GEM specific NCFGR bitfields. */
#define NTL_GEM_GBE_OFFSET		10 /* Gigabit mode enable */
#define NTL_GEM_GBE_SIZE		1
#define NTL_GEM_PCSSEL_OFFSET	11
#define NTL_GEM_PCSSEL_SIZE		1
#define NTL_GEM_CLK_OFFSET		18 /* MDC clock division */
#define NTL_GEM_CLK_SIZE		3
#define NTL_GEM_DBW_OFFSET		21 /* Data bus width */
#define NTL_GEM_DBW_SIZE		2
#define NTL_GEM_RXCOEN_OFFSET	24
#define NTL_GEM_RXCOEN_SIZE		1
#define NTL_GEM_SGMIIEN_OFFSET	27
#define NTL_GEM_SGMIIEN_SIZE	1


/* Constants for data bus width. */
#define NTL_GEM_DBW32		0 /* 32 bit AMBA AHB data bus width */
#define NTL_GEM_DBW64		1 /* 64 bit AMBA AHB data bus width */
#define NTL_GEM_DBW128		2 /* 128 bit AMBA AHB data bus width */

/* Bitfields in DMACFG. */
#define NTL_GEM_FBLDO_OFFSET	0 /* fixed burst length for DMA */
#define NTL_GEM_FBLDO_SIZE		5
#define NTL_GEM_ENDIA_DESC_OFFSET	6 /* endian swap mode for management descriptor access */
#define NTL_GEM_ENDIA_DESC_SIZE	1
#define NTL_GEM_ENDIA_PKT_OFFSET	7 /* endian swap mode for packet data access */
#define NTL_GEM_ENDIA_PKT_SIZE	1
#define NTL_GEM_RXBMS_OFFSET	8 /* RX packet buffer memory size select */
#define NTL_GEM_RXBMS_SIZE		2
#define NTL_GEM_TXPBMS_OFFSET	10 /* TX packet buffer memory size select */
#define NTL_GEM_TXPBMS_SIZE		1
#define NTL_GEM_TXCOEN_OFFSET	11 /* TX IP/TCP/UDP checksum gen offload */
#define NTL_GEM_TXCOEN_SIZE		1
#define NTL_GEM_RXBS_OFFSET		16 /* DMA receive buffer size */
#define NTL_GEM_RXBS_SIZE		8
#define NTL_GEM_DDRP_OFFSET		24 /* disc_when_no_ahb */
#define NTL_GEM_DDRP_SIZE		1
#define NTL_GEM_RXEXT_OFFSET	28 /* RX extended Buffer Descriptor mode */
#define NTL_GEM_RXEXT_SIZE		1
#define NTL_GEM_TXEXT_OFFSET	29 /* TX extended Buffer Descriptor mode */
#define NTL_GEM_TXEXT_SIZE		1
#define NTL_GEM_ADDR64_OFFSET	30 /* Address bus width - 64b or 32b */
#define NTL_GEM_ADDR64_SIZE		1

/* Bitfields in PBUFRXCUT */
#define NTL_GEM_WTRMRK_OFFSET	0 /* Watermark value offset */
#define NTL_GEM_WTRMRK_SIZE		12
#define NTL_GEM_ENCUTTHRU_OFFSET	31 /* Enable RX partial store and forward */
#define NTL_GEM_ENCUTTHRU_SIZE	1

/* Bitfields in NSR */
#define NTL_MACB_NSR_LINK_OFFSET	0 /* pcs_link_state */
#define NTL_MACB_NSR_LINK_SIZE	1
#define NTL_MACB_MDIO_OFFSET	1 /* status of the mdio_in pin */
#define NTL_MACB_MDIO_SIZE		1
#define NTL_MACB_IDLE_OFFSET	2 /* The PHY management logic is idle */
#define NTL_MACB_IDLE_SIZE		1

/* Bitfields in TSR */
#define NTL_MACB_UBR_OFFSET		0 /* Used bit read */
#define NTL_MACB_UBR_SIZE		1
#define NTL_MACB_COL_OFFSET		1 /* Collision occurred */
#define NTL_MACB_COL_SIZE		1
#define NTL_MACB_TSR_RLE_OFFSET	2 /* Retry limit exceeded */
#define NTL_MACB_TSR_RLE_SIZE	1
#define NTL_MACB_TGO_OFFSET		3 /* Transmit go */
#define NTL_MACB_TGO_SIZE		1
#define NTL_MACB_BEX_OFFSET		4 /* TX frame corruption due to AHB error */
#define NTL_MACB_BEX_SIZE		1
#define NTL_MACB_RM9200_BNQ_OFFSET	4 /* AT91RM9200 only */
#define NTL_MACB_RM9200_BNQ_SIZE	1 /* AT91RM9200 only */
#define NTL_MACB_COMP_OFFSET	5 /* Trnasmit complete */
#define NTL_MACB_COMP_SIZE		1
#define NTL_MACB_UND_OFFSET		6 /* Trnasmit under run */
#define NTL_MACB_UND_SIZE		1

/* Bitfields in RSR */
#define NTL_MACB_BNA_OFFSET		0 /* Buffer not available */
#define NTL_MACB_BNA_SIZE		1
#define NTL_MACB_REC_OFFSET		1 /* Frame received */
#define NTL_MACB_REC_SIZE		1
#define NTL_MACB_OVR_OFFSET		2 /* Receive overrun */
#define NTL_MACB_OVR_SIZE		1

/* Bitfields in ISR/IER/IDR/IMR */
#define NTL_MACB_MFD_OFFSET		0 /* Management frame sent */
#define NTL_MACB_MFD_SIZE		1
#define NTL_MACB_RCOMP_OFFSET	1 /* Receive complete */
#define NTL_MACB_RCOMP_SIZE		1
#define NTL_MACB_RXUBR_OFFSET	2 /* RX used bit read */
#define NTL_MACB_RXUBR_SIZE		1
#define NTL_MACB_TXUBR_OFFSET	3 /* TX used bit read */
#define NTL_MACB_TXUBR_SIZE		1
#define NTL_MACB_ISR_TUND_OFFSET	4 /* Enable TX buffer under run interrupt */
#define NTL_MACB_ISR_TUND_SIZE	1
#define NTL_MACB_ISR_RLE_OFFSET	5 /* EN retry exceeded/late coll interrupt */
#define NTL_MACB_ISR_RLE_SIZE	1
#define NTL_MACB_TXERR_OFFSET	6 /* EN TX frame corrupt from error interrupt */
#define NTL_MACB_TXERR_SIZE		1
#define NTL_MACB_TCOMP_OFFSET	7 /* Enable transmit complete interrupt */
#define NTL_MACB_TCOMP_SIZE		1
#define NTL_MACB_ISR_LINK_OFFSET	9 /* Enable link change interrupt */
#define NTL_MACB_ISR_LINK_SIZE	1
#define NTL_MACB_ISR_ROVR_OFFSET	10 /* Enable receive overrun interrupt */
#define NTL_MACB_ISR_ROVR_SIZE	1
#define NTL_MACB_HRESP_OFFSET	11 /* Enable hrsep not OK interrupt */
#define NTL_MACB_HRESP_SIZE		1
#define NTL_MACB_PFR_OFFSET		12 /* Enable pause frame w/ quantum interrupt */
#define NTL_MACB_PFR_SIZE		1
#define NTL_MACB_PTZ_OFFSET		13 /* Enable pause time zero interrupt */
#define NTL_MACB_PTZ_SIZE		1
#define NTL_MACB_WOL_OFFSET		28 /* Enable WOL received interrupt */
#define NTL_MACB_WOL_SIZE		1
#define NTL_MACB_DRQFR_OFFSET	18 /* PTP Delay Request Frame Received */
#define NTL_MACB_DRQFR_SIZE		1
#define NTL_MACB_SFR_OFFSET		19 /* PTP Sync Frame Received */
#define NTL_MACB_SFR_SIZE		1
#define NTL_MACB_DRQFT_OFFSET	20 /* PTP Delay Request Frame Transmitted */
#define NTL_MACB_DRQFT_SIZE		1
#define NTL_MACB_SFT_OFFSET		21 /* PTP Sync Frame Transmitted */
#define NTL_MACB_SFT_SIZE		1
#define NTL_MACB_PDRQFR_OFFSET	22 /* PDelay Request Frame Received */
#define NTL_MACB_PDRQFR_SIZE	1
#define NTL_MACB_PDRSFR_OFFSET	23 /* PDelay Response Frame Received */
#define NTL_MACB_PDRSFR_SIZE	1
#define NTL_MACB_PDRQFT_OFFSET	24 /* PDelay Request Frame Transmitted */
#define NTL_MACB_PDRQFT_SIZE	1
#define NTL_MACB_PDRSFT_OFFSET	25 /* PDelay Response Frame Transmitted */
#define NTL_MACB_PDRSFT_SIZE	1
#define NTL_MACB_SRI_OFFSET		26 /* TSU Seconds Register Increment */
#define NTL_MACB_SRI_SIZE		1

/* Timer increment fields */
#define NTL_MACB_TI_CNS_OFFSET	0
#define NTL_MACB_TI_CNS_SIZE	8
#define NTL_MACB_TI_ACNS_OFFSET	8
#define NTL_MACB_TI_ACNS_SIZE	8
#define NTL_MACB_TI_NIT_OFFSET	16
#define NTL_MACB_TI_NIT_SIZE	8

/* Bitfields in MAN */
#define NTL_MACB_DATA_OFFSET	0 /* data */
#define NTL_MACB_DATA_SIZE		16
#define NTL_MACB_CODE_OFFSET	16 /* Must be written to 10 */
#define NTL_MACB_CODE_SIZE		2
#define NTL_MACB_REGA_OFFSET	18 /* Register address */
#define NTL_MACB_REGA_SIZE		5
#define NTL_MACB_PHYA_OFFSET	23 /* PHY address */
#define NTL_MACB_PHYA_SIZE		5
#define NTL_MACB_RW_OFFSET		28 /* Operation. 10 is read. 01 is write. */
#define NTL_MACB_RW_SIZE		2
#define NTL_MACB_SOF_OFFSET		30 /* Must be written to 1 for Clause 22 */
#define NTL_MACB_SOF_SIZE		2

/* Bitfields in USRIO (AVR32) */
#define NTL_MACB_MII_OFFSET				0
#define NTL_MACB_MII_SIZE				1
#define NTL_MACB_EAM_OFFSET				1
#define NTL_MACB_EAM_SIZE				1
#define NTL_MACB_TX_PAUSE_OFFSET			2
#define NTL_MACB_TX_PAUSE_SIZE			1
#define NTL_MACB_TX_PAUSE_ZERO_OFFSET		3
#define NTL_MACB_TX_PAUSE_ZERO_SIZE			1

/* Bitfields in USRIO (AT91) */
#define NTL_MACB_RMII_OFFSET			0
#define NTL_MACB_RMII_SIZE				1
#define NTL_GEM_RGMII_OFFSET			0 /* GEM gigabit mode */
#define NTL_GEM_RGMII_SIZE				1
#define NTL_MACB_CLKEN_OFFSET			1
#define NTL_MACB_CLKEN_SIZE				1

/* Bitfields in WOL */
#define NTL_MACB_IP_OFFSET				0
#define NTL_MACB_IP_SIZE				16
#define NTL_MACB_MAG_OFFSET				16
#define NTL_MACB_MAG_SIZE				1
#define NTL_MACB_ARP_OFFSET				17
#define NTL_MACB_ARP_SIZE				1
#define NTL_MACB_SA1_OFFSET				18
#define NTL_MACB_SA1_SIZE				1
#define NTL_MACB_WOL_MTI_OFFSET			19
#define NTL_MACB_WOL_MTI_SIZE			1

/* Bitfields in MID */
#define NTL_MACB_IDNUM_OFFSET			16
#define NTL_MACB_IDNUM_SIZE				12
#define NTL_MACB_REV_OFFSET				0
#define NTL_MACB_REV_SIZE				16

/* Bitfields in PCSCNTRL */
#define NTL_GEM_PCSAUTONEG_OFFSET			12
#define NTL_GEM_PCSAUTONEG_SIZE			1

/* Bitfields in DCFG1. */
#define NTL_GEM_IRQCOR_OFFSET			23
#define NTL_GEM_IRQCOR_SIZE				1
#define NTL_GEM_DBWDEF_OFFSET			25
#define NTL_GEM_DBWDEF_SIZE				3

/* Bitfields in DCFG2. */
#define NTL_GEM_RX_PKT_BUFF_OFFSET			20
#define NTL_GEM_RX_PKT_BUFF_SIZE			1
#define NTL_GEM_TX_PKT_BUFF_OFFSET			21
#define NTL_GEM_TX_PKT_BUFF_SIZE			1

/* Bitfields in DCFG5. */
#define NTL_GEM_TSU_OFFSET				8
#define NTL_GEM_TSU_SIZE				1

/* Bitfields in DCFG6. */
#define NTL_GEM_PBUF_LSO_OFFSET			27
#define NTL_GEM_PBUF_LSO_SIZE			1
#define NTL_GEM_DAW64_OFFSET			23
#define NTL_GEM_DAW64_SIZE				1

/* Bitfields in DCFG8. */
#define NTL_GEM_T1SCR_OFFSET			24
#define NTL_GEM_T1SCR_SIZE				8
#define NTL_GEM_T2SCR_OFFSET			16
#define NTL_GEM_T2SCR_SIZE				8
#define NTL_GEM_SCR2ETH_OFFSET			8
#define NTL_GEM_SCR2ETH_SIZE			8
#define NTL_GEM_SCR2CMP_OFFSET			0
#define NTL_GEM_SCR2CMP_SIZE			8

/* Bitfields in DCFG10 */
#define NTL_GEM_TXBD_RDBUFF_OFFSET			12
#define NTL_GEM_TXBD_RDBUFF_SIZE			4
#define NTL_GEM_RXBD_RDBUFF_OFFSET			8
#define NTL_GEM_RXBD_RDBUFF_SIZE			4

/* Bitfields in TISUBN */
#define NTL_GEM_SUBNSINCR_OFFSET			0
#define NTL_GEM_SUBNSINCRL_OFFSET			24
#define NTL_GEM_SUBNSINCRL_SIZE			8
#define NTL_GEM_SUBNSINCRH_OFFSET			0
#define NTL_GEM_SUBNSINCRH_SIZE			16
#define NTL_GEM_SUBNSINCR_SIZE			24

/* Bitfields in TI */
#define NTL_GEM_NSINCR_OFFSET			0
#define NTL_GEM_NSINCR_SIZE				8

/* Bitfields in TSH */
#define NTL_GEM_TSH_OFFSET				0 /* TSU timer value (s). MSB [47:32] of seconds timer count */
#define NTL_GEM_TSH_SIZE				16

/* Bitfields in TSL */
#define NTL_GEM_TSL_OFFSET				0 /* TSU timer value (s). LSB [31:0] of seconds timer count */
#define NTL_GEM_TSL_SIZE				32

/* Bitfields in TN */
#define NTL_GEM_TN_OFFSET				0 /* TSU timer value (ns) */
#define NTL_GEM_TN_SIZE					30

/* Bitfields in TXBDCTRL */
#define NTL_GEM_TXTSMODE_OFFSET			4 /* TX Descriptor Timestamp Insertion mode */
#define NTL_GEM_TXTSMODE_SIZE			2

/* Bitfields in RXBDCTRL */
#define NTL_GEM_RXTSMODE_OFFSET			4 /* RX Descriptor Timestamp Insertion mode */
#define NTL_GEM_RXTSMODE_SIZE			2

/* Bitfields in SCRT2 */
#define NTL_GEM_QUEUE_OFFSET			0 /* Queue Number */
#define NTL_GEM_QUEUE_SIZE				4
#define NTL_GEM_VLANPR_OFFSET			4 /* VLAN Priority */
#define NTL_GEM_VLANPR_SIZE				3
#define NTL_GEM_VLANEN_OFFSET			8 /* VLAN Enable */
#define NTL_GEM_VLANEN_SIZE				1
#define NTL_GEM_ETHT2IDX_OFFSET			9 /* Index to screener type 2 EtherType register */
#define NTL_GEM_ETHT2IDX_SIZE			3
#define NTL_GEM_ETHTEN_OFFSET			12 /* EtherType Enable */
#define NTL_GEM_ETHTEN_SIZE				1
#define NTL_GEM_CMPA_OFFSET				13 /* Compare A - Index to screener type 2 Compare register */
#define NTL_GEM_CMPA_SIZE				5
#define NTL_GEM_CMPAEN_OFFSET			18 /* Compare A Enable */
#define NTL_GEM_CMPAEN_SIZE				1
#define NTL_GEM_CMPB_OFFSET				19 /* Compare B - Index to screener type 2 Compare register */
#define NTL_GEM_CMPB_SIZE				5
#define NTL_GEM_CMPBEN_OFFSET			24 /* Compare B Enable */
#define NTL_GEM_CMPBEN_SIZE				1
#define NTL_GEM_CMPC_OFFSET				25 /* Compare C - Index to screener type 2 Compare register */
#define NTL_GEM_CMPC_SIZE				5
#define NTL_GEM_CMPCEN_OFFSET			30 /* Compare C Enable */
#define NTL_GEM_CMPCEN_SIZE				1

/* Bitfields in ETHT */
#define NTL_GEM_ETHTCMP_OFFSET			0 /* EtherType compare value */
#define NTL_GEM_ETHTCMP_SIZE			16

/* Bitfields in T2CMPW0 */
#define NTL_GEM_T2CMP_OFFSET			16 /* 0xFFFF0000 compare value */
#define NTL_GEM_T2CMP_SIZE				16
#define NTL_GEM_T2MASK_OFFSET			0 /* 0x0000FFFF compare value or mask */
#define NTL_GEM_T2MASK_SIZE				16

/* Bitfields in T2CMPW1 */
#define NTL_GEM_T2DISMSK_OFFSET			9 /* disable mask */
#define NTL_GEM_T2DISMSK_SIZE			1
#define NTL_GEM_T2CMPOFST_OFFSET			7 /* compare offset */
#define NTL_GEM_T2CMPOFST_SIZE			2
#define NTL_GEM_T2OFST_OFFSET			0 /* offset value */
#define NTL_GEM_T2OFST_SIZE				7

/* Offset for screener type 2 compare values (T2CMPOFST).
 * Note the offset is applied after the specified point,
 * e.g. NTL_GEM_T2COMPOFST_ETYPE denotes the EtherType field, so an offset
 * of 12 bytes from this would be the source IP address in an IP header
 */
#define NTL_GEM_T2COMPOFST_SOF		0
#define NTL_GEM_T2COMPOFST_ETYPE	1
#define NTL_GEM_T2COMPOFST_IPHDR	2
#define NTL_GEM_T2COMPOFST_TCPUDP	3

/* offset from EtherType to IP address */
#define NTL_ETYPE_SRCIP_OFFSET			12
#define NTL_ETYPE_DSTIP_OFFSET			16

/* offset from IP header to port */
#define NTL_IPHDR_SRCPORT_OFFSET		0
#define NTL_IPHDR_DSTPORT_OFFSET		2

/* Transmit DMA buffer descriptor Word 1 */
#define NTL_GEM_DMA_TXVALID_OFFSET		23 /* timestamp has been captured in the Buffer Descriptor */
#define NTL_GEM_DMA_TXVALID_SIZE		1

/* Receive DMA buffer descriptor Word 0 */
#define NTL_GEM_DMA_RXVALID_OFFSET		2 /* indicates a valid timestamp in the Buffer Descriptor */
#define NTL_GEM_DMA_RXVALID_SIZE		1

/* DMA buffer descriptor Word 2 (32 bit addressing) or Word 4 (64 bit addressing) */
#define NTL_GEM_DMA_SECL_OFFSET			30 /* Timestamp seconds[1:0]  */
#define NTL_GEM_DMA_SECL_SIZE			2
#define NTL_GEM_DMA_NSEC_OFFSET			0 /* Timestamp nanosecs [29:0] */
#define NTL_GEM_DMA_NSEC_SIZE			30

/* DMA buffer descriptor Word 3 (32 bit addressing) or Word 5 (64 bit addressing) */

/* New hardware supports 12 bit precision of timestamp in DMA buffer descriptor.
 * Old hardware supports only 6 bit precision but it is enough for PTP.
 * Less accuracy is used always instead of checking hardware version.
 */
#define NTL_GEM_DMA_SECH_OFFSET			0 /* Timestamp seconds[5:2] */
#define NTL_GEM_DMA_SECH_SIZE			4
#define NTL_GEM_DMA_SEC_WIDTH			(NTL_GEM_DMA_SECH_SIZE + NTL_GEM_DMA_SECL_SIZE)
#define NTL_GEM_DMA_SEC_TOP				(1 << NTL_GEM_DMA_SEC_WIDTH)
#define NTL_GEM_DMA_SEC_MASK			(NTL_GEM_DMA_SEC_TOP - 1)

/* Bitfields in ADJ */
#define NTL_GEM_ADDSUB_OFFSET			31
#define NTL_GEM_ADDSUB_SIZE				1
/* Constants for CLK */
#define NTL_MACB_CLK_DIV8				0
#define NTL_MACB_CLK_DIV16				1
#define NTL_MACB_CLK_DIV32				2
#define NTL_MACB_CLK_DIV64				3

/* GEM specific constants for CLK. */
#define NTL_GEM_CLK_DIV8				0
#define NTL_GEM_CLK_DIV16				1
#define NTL_GEM_CLK_DIV32				2
#define NTL_GEM_CLK_DIV48				3
#define NTL_GEM_CLK_DIV64				4
#define NTL_GEM_CLK_DIV96				5

/* Constants for MAN register */
#define NTL_MACB_MAN_SOF				1
#define NTL_MACB_MAN_WRITE				1
#define NTL_MACB_MAN_READ				2
#define NTL_MACB_MAN_CODE				2

/* Capability mask bits */
#define NTL_MACB_CAPS_ISR_CLEAR_ON_WRITE		0x00000001
#define NTL_MACB_CAPS_USRIO_HAS_CLKEN		0x00000002
#define NTL_MACB_CAPS_USRIO_DEFAULT_IS_MII_GMII	0x00000004
#define NTL_MACB_CAPS_NO_GIGABIT_HALF		0x00000008
#define NTL_MACB_CAPS_USRIO_DISABLED		0x00000010
#define NTL_MACB_CAPS_JUMBO				0x00000020
#define NTL_MACB_CAPS_NTL_GEM_HAS_PTP			0x00000040
#define NTL_MACB_CAPS_BD_RD_PREFETCH		0x00000080
#define NTL_MACB_CAPS_NEEDS_RSTONUBR		0x00000100
#define NTL_MACB_CAPS_PCS				0x00000400
#define NTL_MACB_CAPS_PARTIAL_STORE_FORWARD		0x00000800
#define NTL_MACB_CAPS_WOL				0x00000200
#define NTL_MACB_CAPS_FIFO_MODE			0x10000000
#define NTL_MACB_CAPS_GIGABIT_MODE_AVAILABLE	0x20000000
#define NTL_MACB_CAPS_SG_DISABLED			0x40000000
#define NTL_MACB_CAPS_NTL_MACB_IS_GEM			0x80000000

#define NS_PER_SEC              1000000000ULL

/* LSO settings */
#define NTL_MACB_LSO_UFO_ENABLE			0x01
#define NTL_MACB_LSO_TSO_ENABLE			0x02

/* Bit manipulation macros */
#define NTL_MACB_BIT(name)					\
	(1 << NTL_MACB_##name##_OFFSET)
#define NTL_MACB_BF(name,value)				\
	(((value) & ((1 << NTL_MACB_##name##_SIZE) - 1))	\
	 << NTL_MACB_##name##_OFFSET)
#define NTL_MACB_BFEXT(name,value)\
	(((value) >> NTL_MACB_##name##_OFFSET)		\
	 & ((1 << NTL_MACB_##name##_SIZE) - 1))
#define NTL_MACB_BFINS(name,value,old)			\
	(((old) & ~(((1 << NTL_MACB_##name##_SIZE) - 1)	\
		    << NTL_MACB_##name##_OFFSET))		\
	 | NTL_MACB_BF(name,value))

#define NTL_GEM_BIT(name)					\
	(1 << NTL_GEM_##name##_OFFSET)
#define NTL_GEM_BF(name, value)				\
	(((value) & ((1 << NTL_GEM_##name##_SIZE) - 1))	\
	 << NTL_GEM_##name##_OFFSET)
#define NTL_GEM_BFEXT(name, value)\
	(((value) >> NTL_GEM_##name##_OFFSET)		\
	 & ((1 << NTL_GEM_##name##_SIZE) - 1))
#define NTL_GEM_BFINS(name, value, old)			\
	(((old) & ~(((1 << NTL_GEM_##name##_SIZE) - 1)	\
		    << NTL_GEM_##name##_OFFSET))		\
	 | NTL_GEM_BF(name, value))

/* Register access macros */
#define ntl_macb_readl(port, reg)		(port)->ntl_macb_reg_readl((port), NTL_MACB_##reg)
#define ntl_macb_writel(port, reg, value)	(port)->ntl_macb_reg_writel((port), NTL_MACB_##reg, (value))
#define ntl_gem_readl(port, reg)		(port)->ntl_macb_reg_readl((port), NTL_GEM_##reg)
#define ntl_gem_writel(port, reg, value)	(port)->ntl_macb_reg_writel((port), NTL_GEM_##reg, (value))
#define ntl_queue_readl(queue, reg)		(queue)->bp->ntl_macb_reg_readl((queue)->bp, (queue)->reg)
#define ntl_queue_writel(queue, reg, value)	(queue)->bp->ntl_macb_reg_writel((queue)->bp, (queue)->reg, (value))
#define ntl_gem_readl_n(port, reg, idx)		(port)->ntl_macb_reg_readl((port), NTL_GEM_##reg + idx * 4)
#define ntl_gem_writel_n(port, reg, idx, value)	(port)->ntl_macb_reg_writel((port), NTL_GEM_##reg + idx * 4, (value))

#define NTL_PTP_TS_BUFFER_SIZE		128 /* must be power of 2 */

/* Conditional GEM/MACB macros.  These perform the operation to the correct
 * register dependent on whether the device is a GEM or a MACB.  For registers
 * and bitfields that are common across both devices, use ntl_macb_{read,write}l
 * to avoid the cost of the conditional.
 */
#define ntl_macb_or_ntl_gem_writel(__bp, __reg, __value) \
	({ \
		if (ntl_macb_is_gem((__bp))) \
			ntl_gem_writel((__bp), __reg, __value); \
		else \
			ntl_macb_writel((__bp), __reg, __value); \
	})

#define ntl_macb_or_ntl_gem_readl(__bp, __reg) \
	({ \
		u32 __v; \
		if (ntl_macb_is_gem((__bp))) \
			__v = ntl_gem_readl((__bp), __reg); \
		else \
			__v = ntl_macb_readl((__bp), __reg); \
		__v; \
	})

/* struct ntl_macb_dma_desc - Hardware DMA descriptor
 * @addr: DMA address of data buffer
 * @ctrl: Control and status bits
 */
struct ntl_macb_dma_desc {
	u32	addr;
	u32	ctrl;
};

#ifdef NTL_MACB_EXT_DESC
#define HW_DMA_CAP_32B		0
#define HW_DMA_CAP_64B		(1 << 0)

struct ntl_macb_dma_desc_64 {
	u32 addrh;
	u32 resvd;
};

#endif

/* DMA descriptor bitfields */
#define NTL_MACB_RX_USED_OFFSET			0
#define NTL_MACB_RX_USED_SIZE			1
#define NTL_MACB_RX_WRAP_OFFSET			1
#define NTL_MACB_RX_WRAP_SIZE			1
#define NTL_MACB_RX_WADDR_OFFSET			2
#define NTL_MACB_RX_WADDR_SIZE			30

#define NTL_MACB_RX_FRMLEN_OFFSET			0
#define NTL_MACB_RX_FRMLEN_SIZE			12
#define NTL_MACB_RX_OFFSET_OFFSET			12
#define NTL_MACB_RX_OFFSET_SIZE			2
#define NTL_MACB_RX_SOF_OFFSET			14
#define NTL_MACB_RX_SOF_SIZE			1
#define NTL_MACB_RX_EOF_OFFSET			15
#define NTL_MACB_RX_EOF_SIZE			1
#define NTL_MACB_RX_CFI_OFFSET			16
#define NTL_MACB_RX_CFI_SIZE			1
#define NTL_MACB_RX_VLAN_PRI_OFFSET			17
#define NTL_MACB_RX_VLAN_PRI_SIZE			3
#define NTL_MACB_RX_PRI_TAG_OFFSET			20
#define NTL_MACB_RX_PRI_TAG_SIZE			1
#define NTL_MACB_RX_VLAN_TAG_OFFSET			21
#define NTL_MACB_RX_VLAN_TAG_SIZE			1
#define NTL_MACB_RX_TYPEID_MATCH_OFFSET		22
#define NTL_MACB_RX_TYPEID_MATCH_SIZE		1
#define NTL_MACB_RX_SA4_MATCH_OFFSET		23
#define NTL_MACB_RX_SA4_MATCH_SIZE			1
#define NTL_MACB_RX_SA3_MATCH_OFFSET		24
#define NTL_MACB_RX_SA3_MATCH_SIZE			1
#define NTL_MACB_RX_SA2_MATCH_OFFSET		25
#define NTL_MACB_RX_SA2_MATCH_SIZE			1
#define NTL_MACB_RX_SA1_MATCH_OFFSET		26
#define NTL_MACB_RX_SA1_MATCH_SIZE			1
#define NTL_MACB_RX_EXT_MATCH_OFFSET		28
#define NTL_MACB_RX_EXT_MATCH_SIZE			1
#define NTL_MACB_RX_UHASH_MATCH_OFFSET		29
#define NTL_MACB_RX_UHASH_MATCH_SIZE		1
#define NTL_MACB_RX_MHASH_MATCH_OFFSET		30
#define NTL_MACB_RX_MHASH_MATCH_SIZE		1
#define NTL_MACB_RX_BROADCAST_OFFSET		31
#define NTL_MACB_RX_BROADCAST_SIZE			1

#define NTL_MACB_RX_FRMLEN_MASK			0xFFF
#define NTL_MACB_RX_JFRMLEN_MASK			0x3FFF

/* RX checksum offload disabled: bit 24 clear in NCFGR */
#define NTL_GEM_RX_TYPEID_MATCH_OFFSET		22
#define NTL_GEM_RX_TYPEID_MATCH_SIZE		2

/* RX checksum offload enabled: bit 24 set in NCFGR */
#define NTL_GEM_RX_CSUM_OFFSET			22
#define NTL_GEM_RX_CSUM_SIZE			2

#define NTL_MACB_TX_FRMLEN_OFFSET			0
#define NTL_MACB_TX_FRMLEN_SIZE			11
#define NTL_MACB_TX_LAST_OFFSET			15
#define NTL_MACB_TX_LAST_SIZE			1
#define NTL_MACB_TX_NOCRC_OFFSET			16
#define NTL_MACB_TX_NOCRC_SIZE			1
#define NTL_MACB_MSS_MFS_OFFSET			16
#define NTL_MACB_MSS_MFS_SIZE			14
#define NTL_MACB_TX_LSO_OFFSET			17
#define NTL_MACB_TX_LSO_SIZE			2
#define NTL_MACB_TX_TCP_SEQ_SRC_OFFSET		19
#define NTL_MACB_TX_TCP_SEQ_SRC_SIZE		1
#define NTL_MACB_TX_BUF_EXHAUSTED_OFFSET		27
#define NTL_MACB_TX_BUF_EXHAUSTED_SIZE		1
#define NTL_MACB_TX_UNDERRUN_OFFSET			28
#define NTL_MACB_TX_UNDERRUN_SIZE			1
#define NTL_MACB_TX_ERROR_OFFSET			29
#define NTL_MACB_TX_ERROR_SIZE			1
#define NTL_MACB_TX_WRAP_OFFSET			30
#define NTL_MACB_TX_WRAP_SIZE			1
#define NTL_MACB_TX_USED_OFFSET			31
#define NTL_MACB_TX_USED_SIZE			1

#define NTL_GEM_TX_FRMLEN_OFFSET			0
#define NTL_GEM_TX_FRMLEN_SIZE			14

/* Buffer descriptor constants */
#define NTL_GEM_RX_CSUM_NONE			0
#define NTL_GEM_RX_CSUM_IP_ONLY			1
#define NTL_GEM_RX_CSUM_IP_TCP			2
#define NTL_GEM_RX_CSUM_IP_UDP			3

/* limit RX checksum offload to TCP and UDP packets */
#define NTL_GEM_RX_CSUM_CHECKED_MASK		2

/* Scaled PPM fraction */
#define PPM_FRACTION	16

/* struct ntl_macb_tx_skb - data about an skb which is being transmitted
 * @skb: skb currently being transmitted, only set for the last buffer
 *       of the frame
 * @mapping: DMA address of the skb's fragment buffer
 * @size: size of the DMA mapped buffer
 * @mapped_as_page: true when buffer was mapped with skb_frag_dma_map(),
 *                  false when buffer was mapped with dma_map_single()
 */
struct ntl_macb_tx_skb {
	struct sk_buff		*skb;
	dma_addr_t		mapping;
	size_t			size;
	bool			mapped_as_page;
};

/* Hardware-collected statistics. Used when updating the network
 * device stats by a periodic timer.
 */
struct ntl_macb_stats {
	u32	rx_pause_frames;
	u32	tx_ok;
	u32	tx_single_cols;
	u32	tx_multiple_cols;
	u32	rx_ok;
	u32	rx_fcs_errors;
	u32	rx_align_errors;
	u32	tx_deferred;
	u32	tx_late_cols;
	u32	tx_excessive_cols;
	u32	tx_underruns;
	u32	tx_carrier_errors;
	u32	rx_resource_errors;
	u32	rx_overruns;
	u32	rx_symbol_errors;
	u32	rx_oversize_pkts;
	u32	rx_jabbers;
	u32	rx_undersize_pkts;
	u32	sqe_test_errors;
	u32	rx_length_mismatch;
	u32	tx_pause_frames;
};

struct gem_stats {
	u32	tx_octets_31_0;
	u32	tx_octets_47_32;
	u32	tx_frames;
	u32	tx_broadcast_frames;
	u32	tx_multicast_frames;
	u32	tx_pause_frames;
	u32	tx_64_byte_frames;
	u32	tx_65_127_byte_frames;
	u32	tx_128_255_byte_frames;
	u32	tx_256_511_byte_frames;
	u32	tx_512_1023_byte_frames;
	u32	tx_1024_1518_byte_frames;
	u32	tx_greater_than_1518_byte_frames;
	u32	tx_underrun;
	u32	tx_single_collision_frames;
	u32	tx_multiple_collision_frames;
	u32	tx_excessive_collisions;
	u32	tx_late_collisions;
	u32	tx_deferred_frames;
	u32	tx_carrier_sense_errors;
	u32	rx_octets_31_0;
	u32	rx_octets_47_32;
	u32	rx_frames;
	u32	rx_broadcast_frames;
	u32	rx_multicast_frames;
	u32	rx_pause_frames;
	u32	rx_64_byte_frames;
	u32	rx_65_127_byte_frames;
	u32	rx_128_255_byte_frames;
	u32	rx_256_511_byte_frames;
	u32	rx_512_1023_byte_frames;
	u32	rx_1024_1518_byte_frames;
	u32	rx_greater_than_1518_byte_frames;
	u32	rx_undersized_frames;
	u32	rx_oversize_frames;
	u32	rx_jabbers;
	u32	rx_frame_check_sequence_errors;
	u32	rx_length_field_frame_errors;
	u32	rx_symbol_errors;
	u32	rx_alignment_errors;
	u32	rx_resource_errors;
	u32	rx_overruns;
	u32	rx_ip_header_checksum_errors;
	u32	rx_tcp_checksum_errors;
	u32	rx_udp_checksum_errors;
};

/* Describes the name and offset of an individual statistic register, as
 * returned by `ethtool -S`. Also describes which net_device_stats statistics
 * this register should contribute to.
 */
struct ntl_gem_statistic {
	char stat_string[ETH_GSTRING_LEN];
	int offset;
	u32 stat_bits;
};

/* Bitfield defs for net_device_stat statistics */
#define NTL_GEM_NDS_RXERR_OFFSET		0
#define NTL_GEM_NDS_RXLENERR_OFFSET		1
#define NTL_GEM_NDS_RXOVERERR_OFFSET	2
#define NTL_GEM_NDS_RXCRCERR_OFFSET		3
#define NTL_GEM_NDS_RXFRAMEERR_OFFSET	4
#define NTL_GEM_NDS_RXFIFOERR_OFFSET	5
#define NTL_GEM_NDS_TXERR_OFFSET		6
#define NTL_GEM_NDS_TXABORTEDERR_OFFSET	7
#define NTL_GEM_NDS_TXCARRIERERR_OFFSET	8
#define NTL_GEM_NDS_TXFIFOERR_OFFSET	9
#define NTL_GEM_NDS_COLLISIONS_OFFSET	10

#define NTL_GEM_STAT_TITLE(name, title) NTL_GEM_STAT_TITLE_BITS(name, title, 0)
#define NTL_GEM_STAT_TITLE_BITS(name, title, bits) {	\
	.stat_string = title,				\
	.offset = NTL_GEM_##name,				\
	.stat_bits = bits				\
}

/* list of gem statistic registers. The names MUST match the
 * corresponding NTL_GEM_* definitions.
 */
static const struct ntl_gem_statistic ntl_gem_statistics[] = {
	NTL_GEM_STAT_TITLE(OCTTXL, "tx_octets"), /* OCTTXH combined with OCTTXL */
	NTL_GEM_STAT_TITLE(TXCNT, "tx_frames"),
	NTL_GEM_STAT_TITLE(TXBCCNT, "tx_broadcast_frames"),
	NTL_GEM_STAT_TITLE(TXMCCNT, "tx_multicast_frames"),
	NTL_GEM_STAT_TITLE(TXPAUSECNT, "tx_pause_frames"),
	NTL_GEM_STAT_TITLE(TX64CNT, "tx_64_byte_frames"),
	NTL_GEM_STAT_TITLE(TX65CNT, "tx_65_127_byte_frames"),
	NTL_GEM_STAT_TITLE(TX128CNT, "tx_128_255_byte_frames"),
	NTL_GEM_STAT_TITLE(TX256CNT, "tx_256_511_byte_frames"),
	NTL_GEM_STAT_TITLE(TX512CNT, "tx_512_1023_byte_frames"),
	NTL_GEM_STAT_TITLE(TX1024CNT, "tx_1024_1518_byte_frames"),
	NTL_GEM_STAT_TITLE(TX1519CNT, "tx_greater_than_1518_byte_frames"),
	NTL_GEM_STAT_TITLE_BITS(TXURUNCNT, "tx_underrun",
			    NTL_GEM_BIT(NDS_TXERR)|NTL_GEM_BIT(NDS_TXFIFOERR)),
	NTL_GEM_STAT_TITLE_BITS(SNGLCOLLCNT, "tx_single_collision_frames",
			    NTL_GEM_BIT(NDS_TXERR)|NTL_GEM_BIT(NDS_COLLISIONS)),
	NTL_GEM_STAT_TITLE_BITS(MULTICOLLCNT, "tx_multiple_collision_frames",
			    NTL_GEM_BIT(NDS_TXERR)|NTL_GEM_BIT(NDS_COLLISIONS)),
	NTL_GEM_STAT_TITLE_BITS(EXCESSCOLLCNT, "tx_excessive_collisions",
			    NTL_GEM_BIT(NDS_TXERR)|
			    NTL_GEM_BIT(NDS_TXABORTEDERR)|
			    NTL_GEM_BIT(NDS_COLLISIONS)),
	NTL_GEM_STAT_TITLE_BITS(LATECOLLCNT, "tx_late_collisions",
			    NTL_GEM_BIT(NDS_TXERR)|NTL_GEM_BIT(NDS_COLLISIONS)),
	NTL_GEM_STAT_TITLE(TXDEFERCNT, "tx_deferred_frames"),
	NTL_GEM_STAT_TITLE_BITS(TXCSENSECNT, "tx_carrier_sense_errors",
			    NTL_GEM_BIT(NDS_TXERR)|NTL_GEM_BIT(NDS_COLLISIONS)),
	NTL_GEM_STAT_TITLE(OCTRXL, "rx_octets"), /* OCTRXH combined with OCTRXL */
	NTL_GEM_STAT_TITLE(RXCNT, "rx_frames"),
	NTL_GEM_STAT_TITLE(RXBROADCNT, "rx_broadcast_frames"),
	NTL_GEM_STAT_TITLE(RXMULTICNT, "rx_multicast_frames"),
	NTL_GEM_STAT_TITLE(RXPAUSECNT, "rx_pause_frames"),
	NTL_GEM_STAT_TITLE(RX64CNT, "rx_64_byte_frames"),
	NTL_GEM_STAT_TITLE(RX65CNT, "rx_65_127_byte_frames"),
	NTL_GEM_STAT_TITLE(RX128CNT, "rx_128_255_byte_frames"),
	NTL_GEM_STAT_TITLE(RX256CNT, "rx_256_511_byte_frames"),
	NTL_GEM_STAT_TITLE(RX512CNT, "rx_512_1023_byte_frames"),
	NTL_GEM_STAT_TITLE(RX1024CNT, "rx_1024_1518_byte_frames"),
	NTL_GEM_STAT_TITLE(RX1519CNT, "rx_greater_than_1518_byte_frames"),
	NTL_GEM_STAT_TITLE_BITS(RXUNDRCNT, "rx_undersized_frames",
			    NTL_GEM_BIT(NDS_RXERR)|NTL_GEM_BIT(NDS_RXLENERR)),
	NTL_GEM_STAT_TITLE_BITS(RXOVRCNT, "rx_oversize_frames",
			    NTL_GEM_BIT(NDS_RXERR)|NTL_GEM_BIT(NDS_RXLENERR)),
	NTL_GEM_STAT_TITLE_BITS(RXJABCNT, "rx_jabbers",
			    NTL_GEM_BIT(NDS_RXERR)|NTL_GEM_BIT(NDS_RXLENERR)),
	NTL_GEM_STAT_TITLE_BITS(RXFCSCNT, "rx_frame_check_sequence_errors",
			    NTL_GEM_BIT(NDS_RXERR)|NTL_GEM_BIT(NDS_RXCRCERR)),
	NTL_GEM_STAT_TITLE_BITS(RXLENGTHCNT, "rx_length_field_frame_errors",
			    NTL_GEM_BIT(NDS_RXERR)),
	NTL_GEM_STAT_TITLE_BITS(RXSYMBCNT, "rx_symbol_errors",
			    NTL_GEM_BIT(NDS_RXERR)|NTL_GEM_BIT(NDS_RXFRAMEERR)),
	NTL_GEM_STAT_TITLE_BITS(RXALIGNCNT, "rx_alignment_errors",
			    NTL_GEM_BIT(NDS_RXERR)|NTL_GEM_BIT(NDS_RXOVERERR)),
	NTL_GEM_STAT_TITLE_BITS(RXRESERRCNT, "rx_resource_errors",
			    NTL_GEM_BIT(NDS_RXERR)|NTL_GEM_BIT(NDS_RXOVERERR)),
	NTL_GEM_STAT_TITLE_BITS(RXORCNT, "rx_overruns",
			    NTL_GEM_BIT(NDS_RXERR)|NTL_GEM_BIT(NDS_RXFIFOERR)),
	NTL_GEM_STAT_TITLE_BITS(RXIPCCNT, "rx_ip_header_checksum_errors",
			    NTL_GEM_BIT(NDS_RXERR)),
	NTL_GEM_STAT_TITLE_BITS(RXTCPCCNT, "rx_tcp_checksum_errors",
			    NTL_GEM_BIT(NDS_RXERR)),
	NTL_GEM_STAT_TITLE_BITS(RXUDPCCNT, "rx_udp_checksum_errors",
			    NTL_GEM_BIT(NDS_RXERR)),
};

#define NTL_GEM_STATS_LEN ARRAY_SIZE(ntl_gem_statistics)

#define NTL_QUEUE_STAT_TITLE(title) {	\
	.stat_string = title,			\
}

/* per queue statistics, each should be unsigned long type */
struct ntl_queue_stats {
	union {
		unsigned long first;
		unsigned long rx_packets;
	};
	unsigned long rx_bytes;
	unsigned long rx_dropped;
	unsigned long tx_packets;
	unsigned long tx_bytes;
	unsigned long tx_dropped;
};

static const struct ntl_gem_statistic ntl_queue_statistics[] = {
		NTL_QUEUE_STAT_TITLE("rx_packets"),
		NTL_QUEUE_STAT_TITLE("rx_bytes"),
		NTL_QUEUE_STAT_TITLE("rx_dropped"),
		NTL_QUEUE_STAT_TITLE("tx_packets"),
		NTL_QUEUE_STAT_TITLE("tx_bytes"),
		NTL_QUEUE_STAT_TITLE("tx_dropped"),
};

#define NTL_QUEUE_STATS_LEN ARRAY_SIZE(ntl_queue_statistics)

struct ntl_macb;
struct ntl_macb_queue;

struct ntl_macb_or_gem_ops {
	int	(*mog_alloc_rx_buffers)(struct ntl_macb *bp);
	void	(*mog_free_rx_buffers)(struct ntl_macb *bp);
	void	(*mog_init_rings)(struct ntl_macb *bp);
	int	(*mog_rx)(struct ntl_macb_queue *queue, int budget);
};

struct ntl_macb_config {
	u32			caps;
	unsigned int		dma_burst_length;
	int	(*clk_init)(struct platform_device *pdev, struct clk **pclk,
			    struct clk **hclk, struct clk **tx_clk,
			    struct clk **rx_clk, struct clk **tsu_clk);
	int	(*init)(struct platform_device *pdev);
	int	jumbo_max_len;
};

struct tsu_incr {
	u32 sub_ns;
	u32 ns;
};

struct ntl_macb_queue {
	struct ntl_macb		*bp;
	int			irq;

	unsigned int		ISR;
	unsigned int		IER;
	unsigned int		IDR;
	unsigned int		IMR;
	unsigned int		TBQP;
	unsigned int		TBQPH;
	unsigned int		RBQS;
	unsigned int		RBQP;
	unsigned int		RBQPH;

	unsigned int		tx_head, tx_tail;
	struct ntl_macb_dma_desc	*tx_ring;
	struct ntl_macb_tx_skb	*tx_skb;
	dma_addr_t		tx_ring_dma;
	struct work_struct	tx_error_task;

	dma_addr_t		rx_ring_dma;
	dma_addr_t		rx_buffers_dma;
	unsigned int		rx_tail;
	unsigned int		rx_prepared_head;
	struct ntl_macb_dma_desc	*rx_ring;
	struct sk_buff		**rx_skbuff;
	void			*rx_buffers;
	struct napi_struct	napi;
	struct ntl_queue_stats stats;
};

struct ntl_ethtool_rx_fs_item {
	struct ethtool_rx_flow_spec fs;
	struct list_head list;
};

struct ntl_ethtool_rx_fs_list {
	struct list_head list;
	unsigned int count;
};

struct ntl_macb {
	void __iomem		*regs;
	bool			native_io;

	/* hardware IO accessors */
	u32	(*ntl_macb_reg_readl)(struct ntl_macb *bp, int offset);
	void	(*ntl_macb_reg_writel)(struct ntl_macb *bp, int offset, u32 value);

	struct ntl_macb_dma_desc	*rx_ring_tieoff;
	size_t			rx_buffer_size;

	unsigned int		rx_ring_size;
	unsigned int		tx_ring_size;

	unsigned int		num_queues;
	unsigned int		queue_mask;
	struct ntl_macb_queue	queues[NTL_MACB_MAX_QUEUES];

	spinlock_t		lock;
	struct platform_device	*pdev;
	struct clk		*pclk;
	struct clk		*hclk;
	struct clk		*tx_clk;
	struct clk		*rx_clk;
	struct clk		*tsu_clk;
	struct net_device	*dev;
	union {
		struct ntl_macb_stats	macb;
		struct gem_stats	gem;
	}			hw_stats;

	dma_addr_t		rx_ring_tieoff_dma;

	struct ntl_macb_or_gem_ops	ntl_macbgem_ops;

	struct mii_bus		*mii_bus;
	struct device_node	*phy_node;
	int 			link;
	int 			speed;
	int 			duplex;

	u32			caps;
	unsigned int		dma_burst_length;

	phy_interface_t		phy_interface;

	/* AT91RM9200 transmit */
	struct sk_buff *skb;			/* holds skb until xmit interrupt completes */
	dma_addr_t skb_physaddr;		/* phys addr from pci_map_single */
	int skb_length;				/* saved skb length for pci_unmap_single */
	unsigned int		max_tx_length;

	u64			ethtool_stats[NTL_GEM_STATS_LEN + NTL_QUEUE_STATS_LEN * NTL_MACB_MAX_QUEUES];

	unsigned int		rx_frm_len_mask;
	unsigned int		jumbo_max_len;

	u32			wol;

	/* holds value of rx watermark value for pbuf_rxcutthru register */
	u16			rx_watermark;

#ifdef CONFIG_NTL_TSU
    struct ntl_tsu          tsu;
#endif

#ifdef NTL_MACB_EXT_DESC
	uint8_t hw_dma_cap;
#endif
	spinlock_t tsu_clk_lock; /* gem tsu clock locking */
	unsigned int tsu_rate;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_clock_info;
	struct tsu_incr tsu_incr;
	struct hwtstamp_config tstamp_config;

	/* RX queue filer rule set*/
	struct ntl_ethtool_rx_fs_list rx_fs_list;
	spinlock_t rx_fs_lock;
	unsigned int max_tuples;

	struct tasklet_struct	hresp_err_tasklet;

	int	rx_bd_rd_prefetch;
	int	tx_bd_rd_prefetch;

	u32	rx_intr_mask;
};

static inline bool ntl_macb_is_gem(struct ntl_macb *bp)
{
	return !!(bp->caps & NTL_MACB_CAPS_NTL_MACB_IS_GEM);
}

#endif /* _NTL_MACB_H */