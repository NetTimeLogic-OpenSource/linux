// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL TSU driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */

#ifndef NTL_TSU_H
#define NTL_TSU_H

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/circ_buf.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/gpio.h>
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
#include <linux/net_tstamp.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
#define NTL_TSU_DRIVER_NAME                         "ntl_tsu"
#define NTL_TSU_DRIVER_AUTHOR                       "NetTimeLogic GmbH, Sven Meier <sven.meier@nettimelogic.com>"
#define NTL_TSU_DRIVER_DESC                         "NetTimeLogic GmbH TSU"
#define NTL_TSU_DRIVER_LICENSE                      "GPL"
#define NTL_TSU_DRIVER_VERSION                      "1.01"

/*****************************************************************************/
/* Driver config                                                             */
/*****************************************************************************/
/* if meta information is available, else it will go in polling mode without queue*/
#define NTL_TSU_META_INFO

/* if NTP Frame timestamping is supported*/
#define NTL_TSU_NTP_SUPPORT

#ifdef NTL_TSU_META_INFO

/* if queued mode with irq shall be used or not, this also requires that
meta information is available, else it will go in polling mode without queue */
//#define NTL_TSU_IRQ_MODE // macb runs the rx and tx fetching in irq context

#ifdef NTL_TSU_IRQ_MODE

/* if in irq mode if the RX timestamps shall be queued, this is only allowed if the rx timestamp fetching is not done in an irq */
#define NTL_TSU_IRQ_MODE_RX

/* if in irq mode if the TX timestamps shall be queued, this is only allowed if the tx timestamp fetching is not done in an irq */
#define NTL_TSU_IRQ_MODE_TX

#endif

#endif

/* how many timestamps we can buffer in software per type, can be changed for higher frame rates*/
#define NTL_TSU_TS_QUEUE_MAX_ENTRIES                64

/* if destination MAC shall be checked (Unicast is always passing the check)*/
#define NTL_TSU_CHECK_MAC_TX

/* RX timestamp should be there, but still we allow polling */
#define NTL_TSU_RX_TS_TIMEOUT_MICROSECOND           100000
/* TX timestamp might need some polling */
#define NTL_TSU_TX_TS_TIMEOUT_MICROSECOND           100000

/* if there should be still a software timestamp when not a PTP Event frame*/
#define NTL_TSU_NON_TSU_TS

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
/* Register Set */
#define NTL_TSU_REGSET_SIZE                         0x10000

#define NTL_TSU_CONTROL_REG                         0x00000000
#define NTL_TSU_STATUS_REG                          0x00000004
#define NTL_TSU_VERSION_REG                         0x0000000C

#define NTL_TSU_TS_CONTROL_REG                      0x00000100
#define NTL_TSU_TS_STATUS_REG                       0x00000104
#define NTL_TSU_TS_IRQ_REG                          0x00000110
#define NTL_TSU_TS_IRQMASK_REG                      0x00000114
#define NTL_TSU_DELAY_REQ_RX_L_REG                  0x00000120
#define NTL_TSU_DELAY_REQ_RX_H_REG                  0x00000124
#define NTL_TSU_DELAY_REQ_TX_L_REG                  0x00000128
#define NTL_TSU_DELAY_REQ_TX_H_REG                  0x0000012C
#define NTL_TSU_PDELAY_REQ_RX_L_REG                 0x00000130
#define NTL_TSU_PDELAY_REQ_RX_H_REG                 0x00000134
#define NTL_TSU_PDELAY_REQ_TX_L_REG                 0x00000138
#define NTL_TSU_PDELAY_REQ_TX_H_REG                 0x0000013C
#define NTL_TSU_PDELAY_RESP_RX_L_REG                0x00000140
#define NTL_TSU_PDELAY_RESP_RX_H_REG                0x00000144
#define NTL_TSU_PDELAY_RESP_TX_L_REG                0x00000148
#define NTL_TSU_PDELAY_RESP_TX_H_REG                0x0000014C
#define NTL_TSU_SYNC_RX_L_REG                       0x00000150
#define NTL_TSU_SYNC_RX_H_REG                       0x00000154
#define NTL_TSU_SYNC_TX_L_REG                       0x00000158
#define NTL_TSU_SYNC_TX_H_REG                       0x0000015C
#define NTL_TSU_NTP_RX_L_REG                        0x00000160
#define NTL_TSU_NTP_RX_H_REG                        0x00000164
#define NTL_TSU_NTP_TX_L_REG                        0x00000168
#define NTL_TSU_NTP_TX_H_REG                        0x0000016C

#define NTL_TSU_META_DELAY_REQ_RX_0_REG             0x00000200
#define NTL_TSU_META_DELAY_REQ_RX_1_REG             0x00000204
#define NTL_TSU_META_DELAY_REQ_RX_2_REG             0x00000208
#define NTL_TSU_META_DELAY_REQ_RX_3_REG             0x0000020C
#define NTL_TSU_META_DELAY_REQ_TX_0_REG             0x00000210
#define NTL_TSU_META_DELAY_REQ_TX_1_REG             0x00000214
#define NTL_TSU_META_DELAY_REQ_TX_2_REG             0x00000218
#define NTL_TSU_META_DELAY_REQ_TX_3_REG             0x0000021C
#define NTL_TSU_META_PDELAY_REQ_RX_0_REG            0x00000220
#define NTL_TSU_META_PDELAY_REQ_RX_1_REG            0x00000224
#define NTL_TSU_META_PDELAY_REQ_RX_2_REG            0x00000228
#define NTL_TSU_META_PDELAY_REQ_RX_3_REG            0x0000022C
#define NTL_TSU_META_PDELAY_REQ_TX_0_REG            0x00000230
#define NTL_TSU_META_PDELAY_REQ_TX_1_REG            0x00000234
#define NTL_TSU_META_PDELAY_REQ_TX_2_REG            0x00000238
#define NTL_TSU_META_PDELAY_REQ_TX_3_REG            0x0000023C
#define NTL_TSU_META_PDELAY_RESP_RX_0_REG           0x00000240
#define NTL_TSU_META_PDELAY_RESP_RX_1_REG           0x00000244
#define NTL_TSU_META_PDELAY_RESP_RX_2_REG           0x00000248
#define NTL_TSU_META_PDELAY_RESP_RX_3_REG           0x0000024C
#define NTL_TSU_META_PDELAY_RESP_TX_0_REG           0x00000250
#define NTL_TSU_META_PDELAY_RESP_TX_1_REG           0x00000254
#define NTL_TSU_META_PDELAY_RESP_TX_2_REG           0x00000258
#define NTL_TSU_META_PDELAY_RESP_TX_3_REG           0x0000025C
#define NTL_TSU_META_SYNC_RX_0_REG                  0x00000260
#define NTL_TSU_META_SYNC_RX_1_REG                  0x00000264
#define NTL_TSU_META_SYNC_RX_2_REG                  0x00000268
#define NTL_TSU_META_SYNC_RX_3_REG                  0x0000026C
#define NTL_TSU_META_SYNC_TX_0_REG                  0x00000270
#define NTL_TSU_META_SYNC_TX_1_REG                  0x00000274
#define NTL_TSU_META_SYNC_TX_2_REG                  0x00000278
#define NTL_TSU_META_SYNC_TX_3_REG                  0x0000027C
#define NTL_TSU_META_NTP_RX_0_REG                   0x00000280
#define NTL_TSU_META_NTP_RX_1_REG                   0x00000284
#define NTL_TSU_META_NTP_RX_2_REG                   0x00000288
#define NTL_TSU_META_NTP_RX_3_REG                   0x0000028C
#define NTL_TSU_META_NTP_TX_0_REG                   0x00000290
#define NTL_TSU_META_NTP_TX_1_REG                   0x00000294
#define NTL_TSU_META_NTP_TX_2_REG                   0x00000298
#define NTL_TSU_META_NTP_TX_3_REG                   0x0000029C

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
#define NTL_TSU_CONTROL_ENABLE_BIT                  0x00000001
#define NTL_TSU_CONTROL_ENABLE_ONESTEP_BIT          0x00000002
#define NTL_TSU_CONTROL_100MBIT_BIT                 0x00000100
#define NTL_TSU_CONTROL_1000MBIT_BIT                0x00000200

#define NTL_TSU_STATUS_TIME_INVALID_BIT             0x00000001
#define NTL_TSU_STATUS_TIME_JUMP_BIT                0x00000002
#define NTL_TSU_STATUS_META_INFO_BIT                0x00010000
#define NTL_TSU_STATUS_NTP_SUPPORT_BIT              0x00020000

#define NTL_TSU_TS_STATUS_DELAY_REQ_RX_BIT          0x00000001
#define NTL_TSU_TS_STATUS_DELAY_REQ_TX_BIT          0x00000002
#define NTL_TSU_TS_STATUS_PDELAY_REQ_RX_BIT         0x00000004
#define NTL_TSU_TS_STATUS_PDELAY_REQ_TX_BIT         0x00000008
#define NTL_TSU_TS_STATUS_PDELAY_RESP_RX_BIT        0x00000010
#define NTL_TSU_TS_STATUS_PDELAY_RESP_TX_BIT        0x00000020
#define NTL_TSU_TS_STATUS_SYNC_RX_BIT               0x00000040
#define NTL_TSU_TS_STATUS_SYNC_TX_BIT               0x00000080
#define NTL_TSU_TS_STATUS_NTP_RX_BIT                0x00000100
#define NTL_TSU_TS_STATUS_NTP_TX_BIT                0x00000200
#define NTL_TSU_TS_STATUS_DELAY_REQ_RX_ERROR_BIT    0x00010000
#define NTL_TSU_TS_STATUS_DELAY_REQ_TX_ERROR_BIT    0x00020000
#define NTL_TSU_TS_STATUS_PDELAY_REQ_RX_ERROR_BIT   0x00040000
#define NTL_TSU_TS_STATUS_PDELAY_REQ_TX_ERROR_BIT   0x00080000
#define NTL_TSU_TS_STATUS_PDELAY_RESP_RX_ERROR_BIT  0x00100000
#define NTL_TSU_TS_STATUS_PDELAY_RESP_TX_ERROR_BIT  0x00200000
#define NTL_TSU_TS_STATUS_SYNC_RX_ERROR_BIT         0x00400000
#define NTL_TSU_TS_STATUS_SYNC_TX_ERROR_BIT         0x00800000
#define NTL_TSU_TS_STATUS_NTP_RX_ERROR_BIT          0x01000000
#define NTL_TSU_TS_STATUS_NTP_TX_ERROR_BIT          0x02000000

#define NTL_TSU_TS_CONTROL_DELAY_REQ_RX_BIT         0x00000001
#define NTL_TSU_TS_CONTROL_DELAY_REQ_TX_BIT         0x00000002
#define NTL_TSU_TS_CONTROL_PDELAY_REQ_RX_BIT        0x00000004
#define NTL_TSU_TS_CONTROL_PDELAY_REQ_TX_BIT        0x00000008
#define NTL_TSU_TS_CONTROL_PDELAY_RESP_RX_BIT       0x00000010
#define NTL_TSU_TS_CONTROL_PDELAY_RESP_TX_BIT       0x00000020
#define NTL_TSU_TS_CONTROL_SYNC_RX_BIT              0x00000040
#define NTL_TSU_TS_CONTROL_SYNC_TX_BIT              0x00000080
#define NTL_TSU_TS_CONTROL_NTP_RX_BIT               0x00000100
#define NTL_TSU_TS_CONTROL_NTP_TX_BIT               0x00000200

#define NTL_TSU_TS_IRQMASK_DELAY_REQ_RX_BIT         0x00000001
#define NTL_TSU_TS_IRQMASK_DELAY_REQ_TX_BIT         0x00000002
#define NTL_TSU_TS_IRQMASK_PDELAY_REQ_RX_BIT        0x00000004
#define NTL_TSU_TS_IRQMASK_PDELAY_REQ_TX_BIT        0x00000008
#define NTL_TSU_TS_IRQMASK_PDELAY_RESP_RX_BIT       0x00000010
#define NTL_TSU_TS_IRQMASK_PDELAY_RESP_TX_BIT       0x00000020
#define NTL_TSU_TS_IRQMASK_SYNC_RX_BIT              0x00000040
#define NTL_TSU_TS_IRQMASK_SYNC_TX_BIT              0x00000080
#define NTL_TSU_TS_IRQMASK_NTP_RX_BIT               0x00000100
#define NTL_TSU_TS_IRQMASK_NTP_TX_BIT               0x00000200

#define NTL_TSU_TS_IRQ_DELAY_REQ_RX_BIT             0x00000001
#define NTL_TSU_TS_IRQ_DELAY_REQ_TX_BIT             0x00000002
#define NTL_TSU_TS_IRQ_PDELAY_REQ_RX_BIT            0x00000004
#define NTL_TSU_TS_IRQ_PDELAY_REQ_TX_BIT            0x00000008
#define NTL_TSU_TS_IRQ_PDELAY_RESP_RX_BIT           0x00000010
#define NTL_TSU_TS_IRQ_PDELAY_RESP_TX_BIT           0x00000020
#define NTL_TSU_TS_IRQ_SYNC_RX_BIT                  0x00000040
#define NTL_TSU_TS_IRQ_SYNC_TX_BIT                  0x00000080
#define NTL_TSU_TS_IRQ_NTP_RX_BIT                   0x00000100
#define NTL_TSU_TS_IRQ_NTP_TX_BIT                   0x00000200

/*****************************************************************************/
/* Other definitions                                                         */
/*****************************************************************************/
#define NS_PER_SEC                                  1000000000ULL

/*****************************************************************************/
/* Internal structure                                                        */
/*****************************************************************************/
struct ntl_tsu {
    atomic_t                                        enable;
    void*                                           ctrl_base;
    phys_addr_t                                     physical_ctrl_base;
    spinlock_t                                      lock;
    int                                             tx_type;
#ifdef NTL_TSU_NTP_SUPPORT
    int                                             ntp_support;
#endif
#ifdef NTL_TSU_META_INFO
    int                                             meta_mode;
#ifdef NTL_TSU_IRQ_MODE
    int                                             irq_mode;
    int                                             irq;
#endif
#ifdef NTL_TSU_IRQ_MODE_RX
    wait_queue_head_t                               wait_queue_delay_req_rx;
    wait_queue_head_t                               wait_queue_pdelay_req_rx;
    wait_queue_head_t                               wait_queue_pdelay_resp_rx;
    wait_queue_head_t                               wait_queue_sync_rx;
#ifdef NTL_TSU_NTP_SUPPORT
    wait_queue_head_t                               wait_queue_ntp_rx;
#endif
#endif
    atomic_t                                        data_queue_entry_count_delay_req_rx;
    atomic_t                                        data_queue_entry_count_pdelay_req_rx;
    atomic_t                                        data_queue_entry_count_pdelay_resp_rx;
    atomic_t                                        data_queue_entry_count_sync_rx;
#ifdef NTL_TSU_NTP_SUPPORT
    atomic_t                                        data_queue_entry_count_ntp_rx;
#endif
    struct list_head                                data_queue_delay_req_rx;
    struct list_head                                data_queue_pdelay_req_rx;
    struct list_head                                data_queue_pdelay_resp_rx;
    struct list_head                                data_queue_sync_rx;
#ifdef NTL_TSU_NTP_SUPPORT
    struct list_head                                data_queue_ntp_rx;
#endif
#ifdef NTL_TSU_IRQ_MODE_TX
    wait_queue_head_t                               wait_queue_delay_req_tx;
    wait_queue_head_t                               wait_queue_sync_tx;
    wait_queue_head_t                               wait_queue_pdelay_req_tx;
    wait_queue_head_t                               wait_queue_pdelay_resp_tx;
#ifdef NTL_TSU_NTP_SUPPORT
    wait_queue_head_t                               wait_queue_ntp_tx;
#endif
#endif
    atomic_t                                        data_queue_entry_count_delay_req_tx;
    atomic_t                                        data_queue_entry_count_pdelay_req_tx;
    atomic_t                                        data_queue_entry_count_pdelay_resp_tx;
    atomic_t                                        data_queue_entry_count_sync_tx;
#ifdef NTL_TSU_NTP_SUPPORT
    atomic_t                                        data_queue_entry_count_ntp_tx;
#endif
    struct list_head                                data_queue_delay_req_tx;
    struct list_head                                data_queue_pdelay_req_tx;
    struct list_head                                data_queue_pdelay_resp_tx;
    struct list_head                                data_queue_sync_tx;
#ifdef NTL_TSU_NTP_SUPPORT
    struct list_head                                data_queue_ntp_tx;
#endif
#endif
};

struct ntl_tsu_meta_info {
    uint8_t                                         clock_identity[8];
    uint16_t                                        port_number;
    uint16_t                                        sequence_identity;
    uint8_t                                         domain_number;
};

#ifdef NTL_TSU_META_INFO
struct ntl_tsu_timestamp {
    uint32_t                                        second;
    uint32_t                                        nanosecond;
    struct ntl_tsu_meta_info                        meta_info;
};

struct ntl_tsu_timestamp_list {
    struct list_head                                list;
    struct ntl_tsu_timestamp                        timestamp;
};
#endif

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/
void ntl_tsu_set_link_speed(struct ntl_tsu* tsu, int speed);
void ntl_tsu_handle_rxtstamp(struct ntl_tsu* tsu, struct sk_buff *skb);
void ntl_tsu_handle_txtstamp(struct ntl_tsu* tsu, struct sk_buff *skb);
void ntl_tsu_clear_alltstamp(struct ntl_tsu* tsu);

int ntl_tsu_probe(struct ntl_tsu* tsu, struct platform_device *pdev);
int ntl_tsu_remove(struct ntl_tsu* tsu, struct platform_device *pdev);

#endif