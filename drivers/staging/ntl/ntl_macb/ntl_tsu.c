// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL TSU driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */

#include "ntl_tsu.h"

/*****************************************************************************/
/* Log level                                                                 */
/*****************************************************************************/
#define NTL_TSU_DEBUG_LEVEL                         0
#define NTL_TSU_INFO_LEVEL                          1
#define NTL_TSU_WARNING_LEVEL                       2
#define NTL_TSU_ERROR_LEVEL                         3

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/
unsigned int ntl_tsu_debuglevel = NTL_TSU_WARNING_LEVEL;

/*****************************************************************************/
/* Print macro                                                               */
/*****************************************************************************/
#define CONFIG_NTL_TSU_DEBUG
#ifdef CONFIG_NTL_TSU_DEBUG
    #define NTL_TSU_DPRINTK(level, value...)        {if(level >= ntl_tsu_debuglevel) {printk(KERN_INFO value);}}
    #define DRIVER_DEBUG                            "!!! DEBUG VERSION  !!!"
#else
    #define NTL_TSU_DPRINTK(level, value...)        {if(level >= NTL_TSU_ERROR_LEVEL) {printk(KERN_INFO value);}}
#endif

/*****************************************************************************/
/* Frame definitions                                                         */
/*****************************************************************************/
#define NTL_TSU_RX                                  0
#define NTL_TSU_TX                                  1

const uint8_t NTL_TSU_PTP_DSTMAC[]                  = {0x01, 0x1B, 0x19, 0x00, 0x00, 0x00};
const uint8_t NTL_TSU_PTP_PDELAYDSTMAC[]            = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};
const uint8_t NTL_TSU_PTP_IPV4DSTMAC[]              = {0x01, 0x00, 0x5E, 0x00, 0x01, 0x81};
const uint8_t NTL_TSU_PTP_IPV4PDELAYDSTMAC[]        = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x6B};
const uint8_t NTL_TSU_PTP_IPV6DSTMAC[]              = {0x33, 0x33, 0x00, 0x00, 0x01, 0x81};
const uint8_t NTL_TSU_PTP_IPV6PDELAYDSTMAC[]        = {0x33, 0x33, 0x00, 0x00, 0x00, 0x6B};
const uint8_t NTL_TSU_PTP_DSTIPV4[]                 = {0xE0, 0x00, 0x01, 0x81};
const uint8_t NTL_TSU_PTP_PDELAYDSTIPV4[]           = {0xE0, 0x00, 0x00, 0x6B};
const uint8_t NTL_TSU_PTP_DSTIPV6[]                 = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x81}; // second byte low nibble is a wildcard so set to 0
const uint8_t NTL_TSU_PTP_PDELAYDSTIPV6[]           = {0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6B};

#define NTL_TSU_PTP_ETHERTYPE                       0x88F7
#define NTL_TSU_IPv4_ETHERTYPE                      0x0800
#define NTL_TSU_IPv6_ETHERTYPE                      0x86DD
#define NTL_TSU_HSR_ETHERTYPE                       0x892F
#define NTL_TSU_FRER_ETHERTYPE                      0xF1C1
#define NTL_TSU_VLAN_ETHERTYPE                      0x8100
#define NTL_TSU_SCION_ETHERTYPE                     0xFFD7

#define NTL_TSU_LAYER_ETH                           0x01
#define NTL_TSU_LAYER_IPv4                          0x02
#define NTL_TSU_LAYER_IPv6                          0x03
#define NTL_TSU_LAYER_SCION                         0x04

#define NTL_TSU_IPv4_VERSION                        0x40
#define NTL_TSU_IPv6_VERSION                        0x60
#define NTL_TSU_UDP                                 0x11
#define NTL_TSU_UDP_EVENT_PORT                      0x013F
#define NTL_TSU_UDP_GENERAL_PORT                    0x0140
#define NTL_TSU_UDP_NTP_PORT                        0x007B
#define NTL_TSU_UDP_SCION_PORT                      0x7559
#define NTL_TSU_UDP_SCION_EVENT_PORT                0x284F
#define NTL_TSU_UDP_SCION_GENERAL_PORT              0x2850
#define NTL_TSU_UDP_SCION_NTP_PORT                  0x278B

#define NTL_TSU_PTP_VERSION                         0x02

#define NTL_TSU_PTP_MSG_SYNC                        0x00
#define NTL_TSU_PTP_MSG_DELAY_REQ                   0x01
#define NTL_TSU_PTP_MSG_PDELAY_REQ                  0x02
#define NTL_TSU_PTP_MSG_PDELAY_RESP                 0x03
#define NTL_TSU_PTP_MSG_SYNC_FU                     0x08
#define NTL_TSU_PTP_MSG_DELAY_RESP                  0x09
#define NTL_TSU_PTP_MSG_PDELAY_RESP_FU              0x0A
#define NTL_TSU_PTP_MSG_ANNOUNCE                    0x0B
#define NTL_TSU_PTP_MSG_SIGNALING                   0x0C
#define NTL_TSU_PTP_MSG_MANAGEMENT                  0x0D
#define NTL_TSU_PTP_MSG_RESERVED_E                  0x0E
#define NTL_TSU_PTP_MSG_RESERVED_F                  0x0F
#define NTL_TSU_NTP_MSG                             0x10

#define NTL_TSU_PTP_FLAG_TWO_STEP                   0x0200

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/
static int ntl_tsu_write_reg(struct ntl_tsu* tsu, uint32_t reg_addr, uint32_t* reg_data);
static int ntl_tsu_read_reg(struct ntl_tsu* tsu, uint32_t reg_addr, uint32_t* reg_data);
static void ntl_tsu_print_meta_info(struct ntl_tsu_meta_info* meta_info, int level);
static int ntl_tsu_parse_frame(struct sk_buff* skb, uint8_t direction, struct ntl_tsu_meta_info* meta_info, uint8_t* skip);
static int ntl_tsu_get_timestamp(struct ntl_tsu* tsu, uint32_t error_bit, uint32_t ts_bit, uint32_t ts_reg_addr_l, uint32_t ts_reg_addr_h, uint32_t* second, uint32_t* nanosecond);
#ifdef NTL_TSU_META_INFO
static int ntl_tsu_get_timestamp_and_meta_info(struct ntl_tsu* tsu, uint32_t error_bit, uint32_t ts_bit, uint32_t ts_reg_addr_l, uint32_t ts_reg_addr_h, uint32_t* second, uint32_t* nanosecond,
                                               uint32_t meta_reg_addr_0, uint32_t meta_reg_addr_1, uint32_t meta_reg_addr_2, uint32_t meta_reg_addr_3, struct ntl_tsu_meta_info* meta_info);
static int ntl_tsu_push_timestamp(struct ntl_tsu* tsu, struct list_head* data_queue, atomic_t* data_queue_entry_count, struct ntl_tsu_meta_info* meta_info, uint32_t* second, uint32_t* nanosecond);
static int ntl_tsu_find_timestamp(struct ntl_tsu* tsu, struct list_head* data_queue, atomic_t* data_queue_entry_count, struct ntl_tsu_meta_info* meta_info, uint32_t* second, uint32_t* nanosecond);
static void ntl_tsu_print_timestamp(struct ntl_tsu_timestamp* timestamp, int level);
static void ntl_tsu_print_list(struct list_head* data_queue, int level);
#endif
#ifdef NTL_TSU_IRQ_MODE
static irqreturn_t ntl_tsu_irq(int irq, void* dev);
#endif

/*****************************************************************************/
/* Write register                                                            */
/*****************************************************************************/
static int ntl_tsu_write_reg(struct ntl_tsu* tsu, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    if(tsu->ctrl_base == NULL)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1;
    }

    iowrite32(*reg_data, (tsu->ctrl_base + reg_addr));

    return 0;
}

/*****************************************************************************/
/* Read register                                                             */
/*****************************************************************************/
static int ntl_tsu_read_reg(struct ntl_tsu* tsu, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    if(tsu->ctrl_base == NULL)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1 ;
    }

    *reg_data = ioread32((tsu->ctrl_base + reg_addr));

    return 0;
}

/*****************************************************************************/
/* Parse frame                                                               */
/*****************************************************************************/
static int ntl_tsu_parse_frame(struct sk_buff* skb, uint8_t direction, struct ntl_tsu_meta_info* meta_info, uint8_t* skip)
{

    uint32_t offset;
#ifdef NTL_TSU_NTP_SUPPORT
    uint8_t ntp_clock_id[8];
#endif
#ifdef NTL_TSU_CHECK_MAC_TX
    uint8_t dst_mac[6];
#endif
    uint8_t check_ip;
    uint16_t ethertype_type;
    uint16_t header_length;
    uint8_t layer;
    uint8_t header_ip;
    uint8_t protocol_ip;
    uint8_t header_scion;
    uint8_t protocol_scion;
    uint8_t dst_ip_v4[4];
    uint8_t dst_ip_v6[16];
    uint16_t port_udp_src;
    uint16_t port_udp_dst;
    uint8_t ptp_version;
    uint8_t ptp_msg_type;
    uint16_t ptp_flags;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);


    // parse frame
    *skip = 0; // per default we say don't skip
    offset = 0;
    check_ip = 0;
    layer = 0;

    if (direction == NTL_TSU_TX)
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "TX FRAME\n");
        goto check_mac;

    }
    else if (direction == NTL_TSU_RX)
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "RX FRAME\n");
        if (NTL_TSU_PTP_ETHERTYPE == ntohs(skb->protocol))
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PROTOCOL PTP\n");
            goto check_ptp;
        }
        else if (NTL_TSU_VLAN_ETHERTYPE == ntohs(skb->protocol))
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PROTOCOL VLAN\n");
            offset = 2;
            goto check_ethertype;
        }
        else if (NTL_TSU_HSR_ETHERTYPE == ntohs(skb->protocol))
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PROTOCOL HSR\n");
            offset += 4;
            goto check_ethertype;
        }
        else if (NTL_TSU_FRER_ETHERTYPE == ntohs(skb->protocol))
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PROTOCOL FRER\n");
            offset += 4;
            goto check_ethertype;
        }
        else if (NTL_TSU_IPv4_ETHERTYPE == ntohs(skb->protocol))
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PROTOCOL IPv4\n");
            goto check_ip_v4;
        }
        else if (NTL_TSU_IPv6_ETHERTYPE == ntohs(skb->protocol))
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PROTOCOL IPv6\n");
            goto check_ip_v6;
        }
        else if (NTL_TSU_SCION_ETHERTYPE == ntohs(skb->protocol))
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PROTOCOL SCION\n");
            goto check_scion;
        }
        else
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "NO PTP RELEVANT PROTOCOL\n");
        }
    }
    else
    {
        goto skip_ts;
    }

// check destination mac, only if not unicast
check_mac:
#ifdef NTL_TSU_CHECK_MAC_TX
    memcpy(dst_mac, (skb->data + offset), sizeof(dst_mac));
    if ((dst_mac[0] & 0x01) == 0x00)
    {
        check_ip = 0;
        offset += 12; // go to ethertype
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNICAST MAC\n");
        goto check_ethertype;
    }
    else if ((0 == memcmp(dst_mac, NTL_TSU_PTP_DSTMAC, sizeof(dst_mac))) ||
             (0 == memcmp(dst_mac, NTL_TSU_PTP_PDELAYDSTMAC, sizeof(dst_mac))) ||
             (0 == memcmp(dst_mac, NTL_TSU_PTP_IPV4DSTMAC, sizeof(dst_mac))) ||
             (0 == memcmp(dst_mac, NTL_TSU_PTP_IPV4PDELAYDSTMAC, sizeof(dst_mac))) ||
             (0 == memcmp(dst_mac, NTL_TSU_PTP_IPV6DSTMAC, sizeof(dst_mac))) ||
             (0 == memcmp(dst_mac, NTL_TSU_PTP_IPV6PDELAYDSTMAC, sizeof(dst_mac))))
    {
        check_ip = 1;
        offset += 12; // go to ethertype
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP MAC\n");
        goto check_ethertype;
    }
    else
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "NO PTP OR UNICAST MAC\n");
        goto skip_ts;
    }
#else
        check_ip = 0;
        offset += 12; // go to ethertype
        goto check_ethertype;
#endif

// check ethertype for ip, vlan, hsr frer or ptp
check_ethertype:
    layer = NTL_TSU_LAYER_ETH;
    memcpy(&ethertype_type, (skb->data + offset), sizeof(ethertype_type));
    ethertype_type = ntohs(ethertype_type);

    if (ethertype_type == NTL_TSU_PTP_ETHERTYPE)
    {
       offset += 2; // go to ptp header
       NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP ETHERTYPE\n");
       goto check_ptp;
    }
    else if (ethertype_type == NTL_TSU_VLAN_ETHERTYPE)
    {
        offset += 4; // go to ethertype again
        goto check_ethertype;
    }
    else if (ethertype_type == NTL_TSU_HSR_ETHERTYPE)
    {
        offset += 6; // go to ethertype again
        goto check_ethertype;
    }
    else if (ethertype_type == NTL_TSU_FRER_ETHERTYPE)
    {
        offset += 6; // go to ethertype again
        goto check_ethertype;
    }
    else if (ethertype_type == NTL_TSU_IPv4_ETHERTYPE)
    {
       offset += 2;  // go to ipv4 header
       goto check_ip_v4;
    }
    else if (ethertype_type == NTL_TSU_IPv6_ETHERTYPE)
    {
       offset += 2;  // go to ipv6 header
       goto check_ip_v6;
    }
    else if (ethertype_type == NTL_TSU_SCION_ETHERTYPE)
    {
       offset += 2;  // go to scion header
       goto check_scion;
    }
    else
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN ETHERTYPE\n");
        goto skip_ts;
    }

// check ipv4 version, protocol and destination ip
check_ip_v4:
    layer = NTL_TSU_LAYER_IPv4;

#ifdef NTL_TSU_NTP_SUPPORT
    memcpy(&(ntp_clock_id[0]), (skb->data + offset + 4), 2); // IP ID
    memcpy(&(ntp_clock_id[2]), (skb->data + offset + 10), 2); // IP Header Checksum
    if (direction == NTL_TSU_TX)
    {
        memcpy(&(ntp_clock_id[4]), (skb->data + offset + 16), 4); // DST IP
    }
    else
    {
        memcpy(&(ntp_clock_id[4]), (skb->data + offset + 12), 4); // SRC IP
    }
#endif

    memcpy(&header_ip, (skb->data + offset + 0), sizeof(header_ip)); // this is where the version and header length is
    if ((header_ip & 0xF0) != NTL_TSU_IPv4_VERSION) // not ip v4
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN IP VERSION\n");
        goto skip_ts;
    }

    header_length = (header_ip & 0x0F);
    header_length *= 4; // header is in 32 bits

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "IP HEADER LENGTH %d\n", header_length);

    memcpy(&protocol_ip, (skb->data + offset + 9), sizeof(protocol_ip)); // this is where the protocol is
    if (protocol_ip  != NTL_TSU_UDP) // not udp
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN IP PROTOCOL\n");
        goto skip_ts;
    }

    if (check_ip != 0)
    {
        memcpy(dst_ip_v4, (skb->data + offset + 16), sizeof(dst_ip_v4)); // this is where the destination ip is
        if ((0 == memcmp(dst_ip_v4, NTL_TSU_PTP_DSTIPV4, sizeof(dst_ip_v4))) ||
            (0 == memcmp(dst_ip_v4, NTL_TSU_PTP_PDELAYDSTIPV4, sizeof(dst_ip_v4))))
        {
            offset += header_length; // go to udp header
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP DST IP\n");
            goto check_udp;
        }
        else
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN DST IP\n");
            goto skip_ts;
        }
    }
    else
    {
        offset += header_length; // go to udp header
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "DID NOT CHECK IP\n");
        goto check_udp;
    }

// check ipv6 version, protocol and destination ip
check_ip_v6:
    layer = NTL_TSU_LAYER_IPv6;

#ifdef NTL_TSU_NTP_SUPPORT
    if (direction == NTL_TSU_TX)
    {
        memcpy(ntp_clock_id, (skb->data + offset + 32), 8); // DST IP (lower 64bits)
    }
    else
    {
        memcpy(ntp_clock_id, (skb->data + offset + 16), 8); // SRC IP (lower 64bits)
    }
#endif

    memcpy(&header_ip, (skb->data + offset + 0), sizeof(header_ip)); // this is where the version is
    if ((header_ip & 0xF0) != NTL_TSU_IPv6_VERSION) // not ip v6
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN IP VERSION\n");
        goto skip_ts;
    }

    memcpy(&protocol_ip, (skb->data + offset + 6), sizeof(protocol_ip)); // this is where the protocol is
    if (protocol_ip  != NTL_TSU_UDP) // not udp
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN IP PROTOCOL\n");
        goto skip_ts;
    }

    if (check_ip != 0)
    {
        memcpy(dst_ip_v6, (skb->data + offset + 24), sizeof(dst_ip_v6)); // this is where the destination ip is
        if (0 == memcmp(dst_ip_v6, NTL_TSU_PTP_PDELAYDSTIPV6, sizeof(dst_ip_v6)))
        {
            offset += 40; // go to udp header
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP DST IP\n");
            goto check_udp;
        }
        else
        {
            dst_ip_v6[1] &= 0xF0; // lowest nibble on second byte is a wildecard so set it to 0 as for the comparison
            if (0 == memcmp(dst_ip_v6, NTL_TSU_PTP_DSTIPV6, sizeof(dst_ip_v6)))
            {
                offset += 40; // go to udp header
                NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP DST IP\n");
                goto check_udp;
            }
            else
            {
                NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN DST IP\n");
                goto skip_ts;
            }
        }
    }
    else
    {
        offset += 40; // go to udp header
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "DID NOT CHECK IP\n");
        goto check_udp;
    }

// check scion and skip header
check_scion:
    layer = NTL_TSU_LAYER_SCION;

#ifdef NTL_TSU_NTP_SUPPORT
    if (direction == NTL_TSU_TX)
    {
        memcpy(ntp_clock_id, (skb->data + offset + 12), 8); // DST ISD & AS
    }
    else
    {
        memcpy(ntp_clock_id, (skb->data + offset + 20), 8); // SRC ISD & AS
    }
#endif

    memcpy(&protocol_scion, (skb->data + offset + 4), sizeof(protocol_scion)); // this is where the protocol is
    if (protocol_scion  != NTL_TSU_UDP) // not udp
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN SCION PROTOCOL\n");
        goto skip_ts;
    }

    memcpy(&header_scion, (skb->data + offset + 5), sizeof(header_scion)); // this is where the header length is

    header_length = header_scion;
    header_length *= 4; // header is in 32 bits

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "SCION HEADER LENGTH %d\n", header_length);

    offset += header_length; // go to udp header
    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "DID NOT CHECK SCION\n");

    if (offset > skb->data_len)
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "SCION HEADER LENGTH WRONG?\n");
        goto skip_ts;
    }
    else
    {
        goto check_udp;
    }

// check udp destination port
check_udp:
    memcpy(&port_udp_src, (skb->data + offset + 0), sizeof(port_udp_src)); // this is where the source port is
    port_udp_src = ntohs(port_udp_src);

    memcpy(&port_udp_dst, (skb->data + offset + 2), sizeof(port_udp_dst)); // this is where the destination port is
    port_udp_dst = ntohs(port_udp_dst);

    if ((port_udp_src == NTL_TSU_UDP_EVENT_PORT) ||
        (port_udp_dst == NTL_TSU_UDP_EVENT_PORT)) // event message
    {
        offset += 8; // go to ptp header
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP UDP EVENT PORT\n");
        goto check_ptp;
    }
    else if ((port_udp_src == NTL_TSU_UDP_GENERAL_PORT) ||
             (port_udp_dst == NTL_TSU_UDP_GENERAL_PORT)) // no event message
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP UDP GENERAL PORT, not to ts\n");
        goto skip_ts;
    }
#ifdef NTL_TSU_NTP_SUPPORT
    else if ((port_udp_src == NTL_TSU_UDP_NTP_PORT) ||
             (port_udp_dst == NTL_TSU_UDP_NTP_PORT)) // ntp message
    {
        offset += 8; // go to ntp header
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "NTP UDP PORT\n");
        goto check_ntp;
    }
#endif
    else if ((layer != NTL_TSU_LAYER_SCION) &&
             ((port_udp_src == NTL_TSU_UDP_SCION_PORT) ||
              (port_udp_src == NTL_TSU_UDP_SCION_EVENT_PORT) ||
              (port_udp_src == NTL_TSU_UDP_SCION_GENERAL_PORT) ||
              (port_udp_src == NTL_TSU_UDP_SCION_NTP_PORT) ||
              (port_udp_dst == NTL_TSU_UDP_SCION_PORT) ||
              (port_udp_dst == NTL_TSU_UDP_SCION_EVENT_PORT) ||
              (port_udp_dst == NTL_TSU_UDP_SCION_GENERAL_PORT) ||
              (port_udp_dst == NTL_TSU_UDP_SCION_NTP_PORT))) // scion ports and outer
    {
        offset += 8; // go to scion header
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "SCION UDP PORT\n");
        goto check_scion;
    }
    else if ((layer = NTL_TSU_LAYER_SCION) &&
             ((port_udp_src == NTL_TSU_UDP_SCION_EVENT_PORT) ||
              (port_udp_dst == NTL_TSU_UDP_SCION_EVENT_PORT))) // scion PTP ports and inner
    {
        offset += 8; // go to ptp header
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP SCION UDP EVENT PORT\n");
        goto check_ptp;
    }
    else if ((layer = NTL_TSU_LAYER_SCION) &&
             ((port_udp_src == NTL_TSU_UDP_SCION_GENERAL_PORT) ||
              (port_udp_dst == NTL_TSU_UDP_SCION_GENERAL_PORT))) // scion PTP ports and inner
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP SCION UDP GENERAL PORT, not to ts\n");
        goto skip_ts;
    }
#ifdef NTL_TSU_NTP_SUPPORT
    else if ((layer = NTL_TSU_LAYER_SCION) &&
             ((port_udp_src == NTL_TSU_UDP_SCION_NTP_PORT) ||
              (port_udp_dst == NTL_TSU_UDP_SCION_NTP_PORT))) // scion NTP ports and inner
    {
        offset += 8; // go to ntp header
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "NTP SCION UDP PORT\n");
        goto check_ntp;
    }
#endif
    else
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN UDP PORT but with PTP IP: %04x\n", port_udp_dst);
        goto skip_ts;
    }

// check ptp version and message type
check_ptp:
    memcpy(&ptp_version, (skb->data + offset + 1), sizeof(ptp_version)); // this is where the version is
    ptp_version &= 0x0F;
    if (ptp_version != NTL_TSU_PTP_VERSION)
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "UNKNOWN PTP VERSION\n");
        goto skip_ts;
    }

    memcpy(&ptp_flags, (skb->data + offset + 6), sizeof(ptp_flags)); // this is where the flags are
    ptp_flags = ntohs(ptp_flags);

    memcpy(&ptp_msg_type, (skb->data + offset + 0), sizeof(ptp_msg_type)); // this is where the message type is
    ptp_msg_type &= 0x0F;
    if ((ptp_msg_type == NTL_TSU_PTP_MSG_SYNC) ||
        (ptp_msg_type == NTL_TSU_PTP_MSG_DELAY_REQ) ||
        (ptp_msg_type == NTL_TSU_PTP_MSG_PDELAY_REQ) ||
        (ptp_msg_type == NTL_TSU_PTP_MSG_PDELAY_RESP))
    {
        // for these no timestamp is expected
        if (((ptp_msg_type == NTL_TSU_PTP_MSG_SYNC) && (direction == NTL_TSU_TX) && ((ptp_flags & NTL_TSU_PTP_FLAG_TWO_STEP) == 0)) ||
            ((ptp_msg_type == NTL_TSU_PTP_MSG_PDELAY_RESP) && (direction == NTL_TSU_TX) && ((ptp_flags & NTL_TSU_PTP_FLAG_TWO_STEP) == 0)))
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "One Step PTP FRAME not to TS\n");
            *skip = 1; // to signal that we fetch the timestamp but not providing it to the hetwork stack
        }
        else
        {
            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP FRAME to TS\n");
        }

        memcpy(&(meta_info->domain_number), (skb->data + offset + 4), sizeof(meta_info->domain_number));
        memcpy(&(meta_info->clock_identity), (skb->data + offset + 20), sizeof(meta_info->clock_identity));
        memcpy(&(meta_info->port_number), (skb->data + offset + 28), sizeof(meta_info->port_number));
        meta_info->port_number = ntohs(meta_info->port_number);
        memcpy(&(meta_info->sequence_identity), (skb->data + offset + 30), sizeof(meta_info->sequence_identity));
        meta_info->sequence_identity = ntohs(meta_info->sequence_identity);
        goto get_ts;
    }
    else
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "PTP FRAME not to TS\n");
        goto skip_ts;
    }

#ifdef NTL_TSU_NTP_SUPPORT
// check and extract some info from the frame
check_ntp:
    ptp_msg_type = NTL_TSU_NTP_MSG;
    memcpy(&(meta_info->domain_number), (skb->data + offset + 0), sizeof(meta_info->domain_number));
    meta_info->domain_number &= 0x3F; // we use the Version and Mode as Domain
    memcpy(&(meta_info->clock_identity), ntp_clock_id, sizeof(meta_info->clock_identity)); // might be parts of IPv4, IPv6 or Scion Header
    memcpy(&(meta_info->port_number), (skb->data + offset + 28), sizeof(meta_info->port_number)); // upper part of lower 32bits of the Origin Timestamp
    meta_info->port_number = ntohs(meta_info->port_number);
    memcpy(&(meta_info->sequence_identity), (skb->data + offset + 30), sizeof(meta_info->sequence_identity)); // lower part of lower 32bits of the Origin Timestamp
    meta_info->sequence_identity = ntohs(meta_info->sequence_identity);
    goto get_ts;
#endif

skip_ts:
    *skip = 1;
    return -1;
get_ts:
    return ptp_msg_type;
}


/*****************************************************************************/
/* Set link speed                                                            */
/*****************************************************************************/
void ntl_tsu_set_link_speed(struct ntl_tsu* tsu, int speed)
{
    uint32_t reg_data;
    unsigned long flags;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&tsu->enable))
    {
        //NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "tsu not enabled\n");
        return;
    }

    // start spinlock section
    spin_lock_irqsave(&(tsu->lock), flags);

    // get current value of the control register
    ntl_tsu_read_reg(tsu, NTL_TSU_CONTROL_REG, &reg_data);


    switch (speed) {
    case SPEED_10:
        // clear 100Mbit and 1000Mbit flag
        reg_data &= ~NTL_TSU_CONTROL_100MBIT_BIT;
        reg_data &= ~NTL_TSU_CONTROL_1000MBIT_BIT;
        break;
    case SPEED_100:
        // set 100Mbit and clear 1000Mbit flag
        reg_data |= NTL_TSU_CONTROL_100MBIT_BIT;
        reg_data &= ~NTL_TSU_CONTROL_1000MBIT_BIT;
        break;
    case SPEED_1000:
        // set 1000Mbit and clear 100Mbit flag
        reg_data |= NTL_TSU_CONTROL_1000MBIT_BIT;
        reg_data &= ~NTL_TSU_CONTROL_100MBIT_BIT;
        break;
    default:
		spin_unlock_irqrestore(&(tsu->lock), flags);
        return;
    }

    // write control register
    ntl_tsu_write_reg(tsu, NTL_TSU_CONTROL_REG, &reg_data);

    // end spinlock section
    spin_unlock_irqrestore(&(tsu->lock), flags);

    return;
}

/*****************************************************************************/
/* Print functions                                                          */
/*****************************************************************************/
static void ntl_tsu_print_meta_info(struct ntl_tsu_meta_info* meta_info, int level)
{
    NTL_TSU_DPRINTK(level, "meta info:\n");
    NTL_TSU_DPRINTK(level, "    clock id: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                                                           meta_info->clock_identity[0],
                                                           meta_info->clock_identity[1],
                                                           meta_info->clock_identity[2],
                                                           meta_info->clock_identity[3],
                                                           meta_info->clock_identity[4],
                                                           meta_info->clock_identity[5],
                                                           meta_info->clock_identity[6],
                                                           meta_info->clock_identity[7]);
    NTL_TSU_DPRINTK(level, "    port nr: %04X\n", meta_info->port_number);
    NTL_TSU_DPRINTK(level, "    sequence id: %04X\n", meta_info->sequence_identity);
    NTL_TSU_DPRINTK(level, "    domain: %02X\n", meta_info->domain_number);
}

#ifdef NTL_TSU_META_INFO
static void ntl_tsu_print_timestamp(struct ntl_tsu_timestamp* timestamp, int level)
{
    NTL_TSU_DPRINTK(level, "timestamp:\n");
    NTL_TSU_DPRINTK(level, "    second: %u\n", timestamp->second);
    NTL_TSU_DPRINTK(level, "    nanosecond: %u\n", timestamp->nanosecond);
    NTL_TSU_DPRINTK(level, "    meta info:\n");
    NTL_TSU_DPRINTK(level, "        clock id: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
                                                           timestamp->meta_info.clock_identity[0],
                                                           timestamp->meta_info.clock_identity[1],
                                                           timestamp->meta_info.clock_identity[2],
                                                           timestamp->meta_info.clock_identity[3],
                                                           timestamp->meta_info.clock_identity[4],
                                                           timestamp->meta_info.clock_identity[5],
                                                           timestamp->meta_info.clock_identity[6],
                                                           timestamp->meta_info.clock_identity[7]);
    NTL_TSU_DPRINTK(level, "        port nr: %04X\n", timestamp->meta_info.port_number);
    NTL_TSU_DPRINTK(level, "        sequence id: %04X\n", timestamp->meta_info.sequence_identity);
    NTL_TSU_DPRINTK(level, "        domain: %02X\n", timestamp->meta_info.domain_number);
}
static void ntl_tsu_print_list(struct list_head* data_queue, int level)
{
    int i;
    struct list_head* queue_ptr;
    struct ntl_tsu_timestamp_list* timestamp;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    i = 0;
    // go over the whole list
    queue_ptr = data_queue->next;
    while (queue_ptr != data_queue)
    {
        timestamp = list_entry(queue_ptr, struct ntl_tsu_timestamp_list, list);

        // print what we are having here
        NTL_TSU_DPRINTK(level, "timestamp entry (%d) with:\n", i);
        ntl_tsu_print_meta_info(&(timestamp->timestamp.meta_info), level);

        queue_ptr = queue_ptr->next;
        i++;
    }
}
#endif
/*****************************************************************************/
/* Get timestamp                                                          */
/*****************************************************************************/
static int ntl_tsu_get_timestamp(struct ntl_tsu* tsu, uint32_t error_bit, uint32_t ts_bit, uint32_t ts_reg_addr_l, uint32_t ts_reg_addr_h, uint32_t* second, uint32_t* nanosecond)
{
    uint32_t reg_data;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    // clear them
    *second = 0;
    *nanosecond = 0;

    // check if a timestamp or error occurred
    ntl_tsu_read_reg(tsu, NTL_TSU_TS_STATUS_REG, &reg_data);

    if ((reg_data & error_bit) != 0) // check if a timestamp error occurred
    {
        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "timestamp error bit set 0x%08X (reg: 0x%08X)\n", error_bit, reg_data);
    }

    if ((reg_data & (error_bit | ts_bit)) == error_bit) // check if a timestamp error occurred without ts
    {
        // clear error
        reg_data = error_bit;
        ntl_tsu_write_reg(tsu, NTL_TSU_TS_STATUS_REG, &reg_data);
        return -1;
    }
    else
    {
        if ((reg_data & ts_bit) != 0) // check if a timestamp is ready
        {
            // get timestamp
            ntl_tsu_read_reg(tsu, ts_reg_addr_l, nanosecond);
            ntl_tsu_read_reg(tsu, ts_reg_addr_h, second);
            // clear timestamp valid
            reg_data = error_bit | ts_bit;
            ntl_tsu_write_reg(tsu, NTL_TSU_TS_STATUS_REG, &reg_data);
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return 0;
}

/*****************************************************************************/
/* Get Rx timestamp                                                          */
/*****************************************************************************/
void ntl_tsu_handle_rxtstamp(struct ntl_tsu* tsu, struct sk_buff *skb)
{
    struct ntl_tsu_meta_info meta_info_ts;
    struct ntl_tsu_meta_info meta_info;
    ktime_t start_time;
    ktime_t now_time;
    int64_t delta_time_us = 0;
    int ptp_msg_type;
    uint32_t second;
    uint32_t nanosecond;
    unsigned long flags;
    int ret;
    uint8_t skip = 0;

    struct skb_shared_hwtstamps *timestamps;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&tsu->enable))
    {
        //NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "tsu not enabled\n");
        return;
    }

    // default no timestamp
    second = 0;
    nanosecond = 0;

    // parse frame and get frame type
    ptp_msg_type = ntl_tsu_parse_frame(skb, NTL_TSU_RX, &meta_info, &skip);

    //start_time = jiffies + usecs_to_jiffies(NTL_TSU_RX_TS_TIMEOUT_MICROSECOND);
    start_time = ktime_get();

    if (ptp_msg_type >= 0)
    {
        do
        {
            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

            switch (ptp_msg_type)
            {
                case NTL_TSU_PTP_MSG_DELAY_REQ:
                    // start spinlock section
                    spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                    if (tsu->meta_mode != 0)
                    {
#ifdef NTL_TSU_IRQ_MODE_RX
                        if (tsu->irq_mode != 0)
                        {
                            //nothing
                        }
                        else
#endif
                        {
                            // read all timestamps
                            delta_time_us = 0; // ensure at least one read
                            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_DELAY_REQ_RX_ERROR_BIT, NTL_TSU_TS_STATUS_DELAY_REQ_RX_BIT, NTL_TSU_DELAY_REQ_RX_L_REG, NTL_TSU_DELAY_REQ_RX_H_REG, &second, &nanosecond,
                                                                                 NTL_TSU_META_DELAY_REQ_RX_0_REG, NTL_TSU_META_DELAY_REQ_RX_1_REG, NTL_TSU_META_DELAY_REQ_RX_2_REG, NTL_TSU_META_DELAY_REQ_RX_3_REG, &meta_info_ts)) &&
                                   (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
                            {
                                // add to the front of the queue
                                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_delay_req_rx), &(tsu->data_queue_entry_count_delay_req_rx), &meta_info_ts, &second, &nanosecond);

                                now_time = ktime_get();
                                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                            }
                        }
                        ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_delay_req_rx), &(tsu->data_queue_entry_count_delay_req_rx), &meta_info, &second, &nanosecond);
                    }
                    else
#endif
                    {
                        ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_DELAY_REQ_RX_ERROR_BIT, NTL_TSU_TS_STATUS_DELAY_REQ_RX_BIT, NTL_TSU_DELAY_REQ_RX_L_REG, NTL_TSU_DELAY_REQ_RX_H_REG, &second, &nanosecond);
                    }

                    // end spinlock section
                    spin_unlock_irqrestore(&(tsu->lock), flags);

                    now_time = ktime_get();
                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                    if (ret > 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "delay req rx timestamp\n");
                        goto return_rx_ts;
                    }
                    if (ret < 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "delay req rx timestamp error\n");
                        goto return_no_rx_ts;
                    }
                    else if (delta_time_us >= NTL_TSU_RX_TS_TIMEOUT_MICROSECOND)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no delay req rx timestamp\n");
                    }
#ifdef NTL_TSU_IRQ_MODE_RX
                    else if ((tsu->irq_mode != 0) && (ret == 0))
                    {
                        // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                        wait_event_interruptible_timeout(tsu->wait_queue_delay_req_rx, (0 == list_empty(&(tsu->data_queue_delay_req_rx))), usecs_to_jiffies(NTL_TSU_RX_TS_TIMEOUT_MICROSECOND/10));
                    }
#endif
                    break;

                case NTL_TSU_PTP_MSG_PDELAY_REQ:
                    // start spinlock section
                    spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                    if (tsu->meta_mode != 0)
                    {
#ifdef NTL_TSU_IRQ_MODE_RX
                        if (tsu->irq_mode != 0)
                        {
                            //nothing
                        }
                        else
#endif
                        {
                            // read all timestamps
                            delta_time_us = 0; // ensure at least one read
                            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_PDELAY_REQ_RX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_REQ_RX_BIT, NTL_TSU_PDELAY_REQ_RX_L_REG, NTL_TSU_PDELAY_REQ_RX_H_REG, &second, &nanosecond,
                                                                                 NTL_TSU_META_PDELAY_REQ_RX_0_REG, NTL_TSU_META_PDELAY_REQ_RX_1_REG, NTL_TSU_META_PDELAY_REQ_RX_2_REG, NTL_TSU_META_PDELAY_REQ_RX_3_REG, &meta_info_ts)) &&
                                   (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
                            {
                                // add to the front of the queue
                                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_pdelay_req_rx), &(tsu->data_queue_entry_count_pdelay_req_rx), &meta_info_ts, &second, &nanosecond);

                                now_time = ktime_get();
                                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                            }
                        }
                        ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_pdelay_req_rx), &(tsu->data_queue_entry_count_pdelay_req_rx), &meta_info, &second, &nanosecond);
                    }
                    else
#endif
                    {
                        ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_PDELAY_REQ_RX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_REQ_RX_BIT, NTL_TSU_PDELAY_REQ_RX_L_REG, NTL_TSU_PDELAY_REQ_RX_H_REG, &second, &nanosecond);
                    }

                    // end spinlock section
                    spin_unlock_irqrestore(&(tsu->lock), flags);

                    now_time = ktime_get();
                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                    if (ret > 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "pdelay req rx timestamp\n");
                        goto return_rx_ts;
                    }
                    if (ret < 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "pdelay req rx timestamp error\n");
                        goto return_no_rx_ts;
                    }
                    else if (delta_time_us >= NTL_TSU_RX_TS_TIMEOUT_MICROSECOND)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no pdelay req rx timestamp\n");
                    }
#ifdef NTL_TSU_IRQ_MODE_RX
                    else if ((tsu->irq_mode != 0) && (ret == 0))
                    {
                        // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                        wait_event_interruptible_timeout(tsu->wait_queue_pdelay_req_rx, (0 == list_empty(&(tsu->data_queue_pdelay_req_rx))), usecs_to_jiffies(NTL_TSU_RX_TS_TIMEOUT_MICROSECOND/10));
                    }
#endif
                    break;

                case NTL_TSU_PTP_MSG_PDELAY_RESP:
                    // start spinlock section
                    spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                    if (tsu->meta_mode != 0)
                    {
#ifdef NTL_TSU_IRQ_MODE_RX
                        if (tsu->irq_mode != 0)
                        {
                            //nothing
                        }
                        else
#endif
                        {
                            // read all timestamps
                            delta_time_us = 0; // ensure at least one read
                            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_PDELAY_RESP_RX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_RESP_RX_BIT, NTL_TSU_PDELAY_RESP_RX_L_REG, NTL_TSU_PDELAY_RESP_RX_H_REG, &second, &nanosecond,
                                                                                 NTL_TSU_META_PDELAY_RESP_RX_0_REG, NTL_TSU_META_PDELAY_RESP_RX_1_REG, NTL_TSU_META_PDELAY_RESP_RX_2_REG, NTL_TSU_META_PDELAY_RESP_RX_3_REG, &meta_info_ts)) &&
                                   (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
                            {
                                // add to the front of the queue
                                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_pdelay_resp_rx), &(tsu->data_queue_entry_count_pdelay_resp_rx), &meta_info_ts, &second, &nanosecond);

                                now_time = ktime_get();
                                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                            }
                        }
                        ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_pdelay_resp_rx), &(tsu->data_queue_entry_count_pdelay_resp_rx), &meta_info, &second, &nanosecond);
                    }
                    else
#endif
                    {
                        ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_PDELAY_RESP_RX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_RESP_RX_BIT, NTL_TSU_PDELAY_RESP_RX_L_REG, NTL_TSU_PDELAY_RESP_RX_H_REG, &second, &nanosecond);
                    }

                    // end spinlock section
                    spin_unlock_irqrestore(&(tsu->lock), flags);

                    now_time = ktime_get();
                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                    if (ret > 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "pdelay resp rx timestamp\n");
                        goto return_rx_ts;
                    }
                    if (ret < 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "pdelay resp rx timestamp error\n");
                        goto return_no_rx_ts;
                    }
                    else if (delta_time_us >= NTL_TSU_RX_TS_TIMEOUT_MICROSECOND)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no pdelay resp rx timestamp\n");
                    }
#ifdef NTL_TSU_IRQ_MODE_RX
                    else if ((tsu->irq_mode != 0) && (ret == 0))
                    {
                        // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                        wait_event_interruptible_timeout(tsu->wait_queue_pdelay_resp_rx, (0 == list_empty(&(tsu->data_queue_pdelay_resp_rx))), usecs_to_jiffies(NTL_TSU_RX_TS_TIMEOUT_MICROSECOND/10));
                    }
#endif
                    break;

                case NTL_TSU_PTP_MSG_SYNC:
                    // start spinlock section
                    spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                    if (tsu->meta_mode != 0)
                    {
#ifdef NTL_TSU_IRQ_MODE_RX
                        if (tsu->irq_mode != 0)
                        {
                            //nothing
                        }
                        else
#endif
                        {
                            // read all timestamps
                            delta_time_us = 0; // ensure at least one read
                            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_SYNC_RX_ERROR_BIT, NTL_TSU_TS_STATUS_SYNC_RX_BIT, NTL_TSU_SYNC_RX_L_REG, NTL_TSU_SYNC_RX_H_REG, &second, &nanosecond,
                                                                                 NTL_TSU_META_SYNC_RX_0_REG, NTL_TSU_META_SYNC_RX_1_REG, NTL_TSU_META_SYNC_RX_2_REG, NTL_TSU_META_SYNC_RX_3_REG, &meta_info_ts)) &&
                                   (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
                            {
                                // add to the front of the queue
                                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_sync_rx), &(tsu->data_queue_entry_count_sync_rx), &meta_info_ts, &second, &nanosecond);

                                now_time = ktime_get();
                                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                            }
                        }
                        ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_sync_rx), &(tsu->data_queue_entry_count_sync_rx), &meta_info, &second, &nanosecond);
                    }
                    else
#endif
                    {
                        ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_SYNC_RX_ERROR_BIT, NTL_TSU_TS_STATUS_SYNC_RX_BIT, NTL_TSU_SYNC_RX_L_REG, NTL_TSU_SYNC_RX_H_REG, &second, &nanosecond);
                    }

                    // end spinlock section
                    spin_unlock_irqrestore(&(tsu->lock), flags);

                    now_time = ktime_get();
                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                    if (ret > 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "sync rx timestamp\n");
                        goto return_rx_ts;
                    }
                    if (ret < 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "sync rx timestamp error\n");
                        goto return_no_rx_ts;
                    }
                    else if (delta_time_us >= NTL_TSU_RX_TS_TIMEOUT_MICROSECOND)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no sync rx timestamp\n");
                    }
#ifdef NTL_TSU_IRQ_MODE_RX
                    else if ((tsu->irq_mode != 0) && (ret == 0))
                    {
                        // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                        wait_event_interruptible_timeout(tsu->wait_queue_sync_rx, (0 == list_empty(&(tsu->data_queue_sync_rx))), usecs_to_jiffies(NTL_TSU_RX_TS_TIMEOUT_MICROSECOND/10));
                    }
#endif
                    break;

#ifdef NTL_TSU_NTP_SUPPORT
                case NTL_TSU_NTP_MSG:
                    if (tsu->ntp_support != 0)
                    {
                        // start spinlock section
                        spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                        if (tsu->meta_mode != 0)
                        {
#ifdef NTL_TSU_IRQ_MODE_RX
                            if (tsu->irq_mode != 0)
                            {
                                //nothing
                            }
                            else
#endif
                            {
                                // read all timestamps
                                delta_time_us = 0; // ensure at least one read
                                while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_NTP_RX_ERROR_BIT, NTL_TSU_TS_STATUS_NTP_RX_BIT, NTL_TSU_NTP_RX_L_REG, NTL_TSU_NTP_RX_H_REG, &second, &nanosecond,
                                                                                     NTL_TSU_META_NTP_RX_0_REG, NTL_TSU_META_NTP_RX_1_REG, NTL_TSU_META_NTP_RX_2_REG, NTL_TSU_META_NTP_RX_3_REG, &meta_info_ts)) &&
                                       (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
                                {
                                    // add to the front of the queue
                                    ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_ntp_rx), &(tsu->data_queue_entry_count_ntp_rx), &meta_info_ts, &second, &nanosecond);

                                    now_time = ktime_get();
                                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                                }
                            }
                            ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_ntp_rx), &(tsu->data_queue_entry_count_ntp_rx), &meta_info, &second, &nanosecond);
                        }
                        else
#endif
                        {
                            ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_NTP_RX_ERROR_BIT, NTL_TSU_TS_STATUS_NTP_RX_BIT, NTL_TSU_NTP_RX_L_REG, NTL_TSU_NTP_RX_H_REG, &second, &nanosecond);
                        }

                        // end spinlock section
                        spin_unlock_irqrestore(&(tsu->lock), flags);

                        now_time = ktime_get();
                        delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                        if (ret > 0)
                        {
                            NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "ntp rx timestamp\n");
                            goto return_rx_ts;
                        }
                        if (ret < 0)
                        {
                            NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "ntp rx timestamp error\n");
                            goto return_no_rx_ts;
                        }
                        else if (delta_time_us >= NTL_TSU_RX_TS_TIMEOUT_MICROSECOND)
                        {
                            NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no ntp rx timestamp\n");
                        }
#ifdef NTL_TSU_IRQ_MODE_RX
                        else if ((tsu->irq_mode != 0) && (ret == 0))
                        {
                            // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                            wait_event_interruptible_timeout(tsu->wait_queue_ntp_rx, (0 == list_empty(&(tsu->data_queue_ntp_rx))), usecs_to_jiffies(NTL_TSU_RX_TS_TIMEOUT_MICROSECOND/10));
                        }
#endif
                    }
                    else
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "not NTP support\n");
                        goto return_no_rx_ts;
                    }
                    break;
#endif

                default:
                    NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "not supported rx message type\n");
                    goto return_no_rx_ts;
            }

            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "retrying to fetch rx timestamp\n");

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

        } while ((delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND));

        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "waiting for rx timestamp timed out\n");
    }
#ifdef NTL_TSU_NON_TSU_TS
    else
    {
        // no timestamp modification (software timestamp still intact?)
        return;
    }
#endif

return_no_rx_ts:
    // default no timestamp
    second = 0;
    nanosecond = 0;

return_rx_ts:
    timestamps = skb_hwtstamps(skb);
    memset(timestamps, 0, sizeof(struct skb_shared_hwtstamps));
	timestamps->hwtstamp = ns_to_ktime((uint64_t)(((uint64_t)second * NS_PER_SEC) + (uint64_t)nanosecond));
}

/*****************************************************************************/
/* Get Tx timestamp                                              */
/*****************************************************************************/
void ntl_tsu_handle_txtstamp(struct ntl_tsu* tsu, struct sk_buff *skb)
{
    struct ntl_tsu_meta_info meta_info_ts;
    struct ntl_tsu_meta_info meta_info;
    ktime_t start_time;
    ktime_t now_time;
    int64_t delta_time_us = 0;
    int ptp_msg_type;
    uint32_t second;
    uint32_t nanosecond;
    unsigned long flags;
    int ret;
    uint8_t skip = 0;

    struct skb_shared_hwtstamps *timestamps;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&tsu->enable))
    {
        //NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "tsu not enabled\n");
        return;
    }

    // default no timestamp
    second = 0;
    nanosecond = 0;

    // parse frame and get frame type
    ptp_msg_type = ntl_tsu_parse_frame(skb, NTL_TSU_TX, &meta_info, &skip);

    //start_time = jiffies + usecs_to_jiffies(NTL_TSU_TX_TS_TIMEOUT_MICROSECOND);
    start_time = ktime_get();

    if (ptp_msg_type >= 0)
    {
        do
        {
            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

            switch (ptp_msg_type)
            {
                case NTL_TSU_PTP_MSG_DELAY_REQ:
                    // start spinlock section
                    spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                    if (tsu->meta_mode != 0)
                    {
#ifdef NTL_TSU_IRQ_MODE_TX
                        if (tsu->irq_mode != 0)
                        {
                            //nothing
                        }
                        else
#endif
                        {
                            // read all timestamps
                            delta_time_us = 0; // ensure at least one read
                            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_DELAY_REQ_TX_ERROR_BIT, NTL_TSU_TS_STATUS_DELAY_REQ_TX_BIT, NTL_TSU_DELAY_REQ_TX_L_REG, NTL_TSU_DELAY_REQ_TX_H_REG, &second, &nanosecond,
                                                                                 NTL_TSU_META_DELAY_REQ_TX_0_REG, NTL_TSU_META_DELAY_REQ_TX_1_REG, NTL_TSU_META_DELAY_REQ_TX_2_REG, NTL_TSU_META_DELAY_REQ_TX_3_REG, &meta_info_ts)) &&
                                   (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
                            {
                                // add to the front of the queue
                                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_delay_req_tx), &(tsu->data_queue_entry_count_delay_req_tx), &meta_info_ts, &second, &nanosecond);

                                now_time = ktime_get();
                                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                            }
                        }
                        ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_delay_req_tx), &(tsu->data_queue_entry_count_delay_req_tx), &meta_info, &second, &nanosecond);
                    }
                    else
#endif
                    {
                        ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_DELAY_REQ_TX_ERROR_BIT, NTL_TSU_TS_STATUS_DELAY_REQ_TX_BIT, NTL_TSU_DELAY_REQ_TX_L_REG, NTL_TSU_DELAY_REQ_TX_H_REG, &second, &nanosecond);
                    }

                    // end spinlock section
                    spin_unlock_irqrestore(&(tsu->lock), flags);

                    now_time = ktime_get();
                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                    if (ret > 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "delay req tx timestamp\n");
                        goto return_tx_ts;
                    }
                    if (ret < 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "delay req tx timestamp error\n");
                        goto return_no_tx_ts;
                    }
                    else if (delta_time_us >= NTL_TSU_TX_TS_TIMEOUT_MICROSECOND)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no delay req tx timestamp\n");
                    }
#ifdef NTL_TSU_IRQ_MODE_TX
                    else if ((tsu->irq_mode != 0) && (ret == 0))
                    {
                        // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                        wait_event_interruptible_timeout(tsu->wait_queue_delay_req_tx, (0 == list_empty(&(tsu->data_queue_delay_req_tx))), usecs_to_jiffies(NTL_TSU_TX_TS_TIMEOUT_MICROSECOND/10));
                    }
#endif
                    break;

                case NTL_TSU_PTP_MSG_PDELAY_REQ:
                    // start spinlock section
                    spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                    if (tsu->meta_mode != 0)
                    {
#ifdef NTL_TSU_IRQ_MODE_TX
                        if (tsu->irq_mode != 0)
                        {
                            //nothing
                        }
                        else
#endif
                        {
                            // read all timestamps
                            delta_time_us = 0; // ensure at least one read
                            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_PDELAY_REQ_TX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_REQ_TX_BIT, NTL_TSU_PDELAY_REQ_TX_L_REG, NTL_TSU_PDELAY_REQ_TX_H_REG, &second, &nanosecond,
                                                                                 NTL_TSU_META_PDELAY_REQ_TX_0_REG, NTL_TSU_META_PDELAY_REQ_TX_1_REG, NTL_TSU_META_PDELAY_REQ_TX_2_REG, NTL_TSU_META_PDELAY_REQ_TX_3_REG, &meta_info_ts)) &&
                                   (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
                            {
                                // add to the front of the queue
                                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_pdelay_req_tx), &(tsu->data_queue_entry_count_pdelay_req_tx), &meta_info_ts, &second, &nanosecond);

                                now_time = ktime_get();
                                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                            }
                        }
                        ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_pdelay_req_tx), &(tsu->data_queue_entry_count_pdelay_req_tx), &meta_info, &second, &nanosecond);
                    }
                    else
#endif
                    {
                        ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_PDELAY_REQ_TX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_REQ_TX_BIT, NTL_TSU_PDELAY_REQ_TX_L_REG, NTL_TSU_PDELAY_REQ_TX_H_REG, &second, &nanosecond);
                    }

                    // end spinlock section
                    spin_unlock_irqrestore(&(tsu->lock), flags);

                    now_time = ktime_get();
                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                    if (ret > 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "pdelay req tx timestamp\n");
                        goto return_tx_ts;
                    }
                    if (ret < 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "pdelay req tx timestamp error\n");
                        goto return_no_tx_ts;
                    }
                    else if (delta_time_us >= NTL_TSU_TX_TS_TIMEOUT_MICROSECOND)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no pdelay req tx timestamp\n");
                    }
#ifdef NTL_TSU_IRQ_MODE_TX
                    else if ((tsu->irq_mode != 0) && (ret == 0))
                    {
                        // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                        wait_event_interruptible_timeout(tsu->wait_queue_pdelay_req_tx, (0 == list_empty(&(tsu->data_queue_pdelay_req_tx))), usecs_to_jiffies(NTL_TSU_TX_TS_TIMEOUT_MICROSECOND/10));
                    }
#endif
                    break;

                case NTL_TSU_PTP_MSG_PDELAY_RESP:
                    // start spinlock section
                    spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                    if (tsu->meta_mode != 0)
                    {
#ifdef NTL_TSU_IRQ_MODE_TX
                        if (tsu->irq_mode != 0)
                        {
                            //nothing
                        }
                        else
#endif
                        {
                            // read all timestamps
                            delta_time_us = 0; // ensure at least one read
                            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_PDELAY_RESP_TX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_RESP_TX_BIT, NTL_TSU_PDELAY_RESP_TX_L_REG, NTL_TSU_PDELAY_RESP_TX_H_REG, &second, &nanosecond,
                                                                                 NTL_TSU_META_PDELAY_RESP_TX_0_REG, NTL_TSU_META_PDELAY_RESP_TX_1_REG, NTL_TSU_META_PDELAY_RESP_TX_2_REG, NTL_TSU_META_PDELAY_RESP_TX_3_REG, &meta_info_ts)) &&
                                   (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
                            {
                                // add to the front of the queue
                                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_pdelay_resp_tx), &(tsu->data_queue_entry_count_pdelay_resp_tx), &meta_info_ts, &second, &nanosecond);

                                now_time = ktime_get();
                                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                            }
                        }
                        ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_pdelay_resp_tx), &(tsu->data_queue_entry_count_pdelay_resp_tx), &meta_info, &second, &nanosecond);
                    }
                    else
#endif
                    {
                        ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_PDELAY_RESP_TX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_RESP_TX_BIT, NTL_TSU_PDELAY_RESP_TX_L_REG, NTL_TSU_PDELAY_RESP_TX_H_REG, &second, &nanosecond);
                    }

                    // end spinlock section
                    spin_unlock_irqrestore(&(tsu->lock), flags);

                    now_time = ktime_get();
                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                    if (ret > 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "pdelay resp tx timestamp\n");
                        goto return_tx_ts;
                    }
                    if (ret < 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "pdelay resp tx timestamp error\n");
                        goto return_no_tx_ts;
                    }
                    else if (delta_time_us >= NTL_TSU_TX_TS_TIMEOUT_MICROSECOND)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no pdelay resp tx timestamp\n");
                    }
#ifdef NTL_TSU_IRQ_MODE_TX
                    else if ((tsu->irq_mode != 0) && (ret == 0))
                    {
                        // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                        wait_event_interruptible_timeout(tsu->wait_queue_pdelay_resp_tx, (0 == list_empty(&(tsu->data_queue_pdelay_resp_tx))), usecs_to_jiffies(NTL_TSU_TX_TS_TIMEOUT_MICROSECOND/10));
                    }
#endif
                    break;

                case NTL_TSU_PTP_MSG_SYNC:
                    // start spinlock section
                    spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                    if (tsu->meta_mode != 0)
                    {
#ifdef NTL_TSU_IRQ_MODE_TX
                        if (tsu->irq_mode != 0)
                        {
                            //nothing
                        }
                        else
#endif
                        {
                            // read all timestamps
                            delta_time_us = 0; // ensure at least one read
                            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_SYNC_TX_ERROR_BIT, NTL_TSU_TS_STATUS_SYNC_TX_BIT, NTL_TSU_SYNC_TX_L_REG, NTL_TSU_SYNC_TX_H_REG, &second, &nanosecond,
                                                                                 NTL_TSU_META_SYNC_TX_0_REG, NTL_TSU_META_SYNC_TX_1_REG, NTL_TSU_META_SYNC_TX_2_REG, NTL_TSU_META_SYNC_TX_3_REG, &meta_info_ts)) &&
                                   (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
                            {
                                // add to the front of the queue
                                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_sync_tx), &(tsu->data_queue_entry_count_sync_tx), &meta_info_ts, &second, &nanosecond);

                                now_time = ktime_get();
                                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                            }
                        }
                        ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_sync_tx), &(tsu->data_queue_entry_count_sync_tx), &meta_info, &second, &nanosecond);
                    }
                    else
#endif
                    {
                        ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_SYNC_TX_ERROR_BIT, NTL_TSU_TS_STATUS_SYNC_TX_BIT, NTL_TSU_SYNC_TX_L_REG, NTL_TSU_SYNC_TX_H_REG, &second, &nanosecond);
                    }

                    // end spinlock section
                    spin_unlock_irqrestore(&(tsu->lock), flags);

                    now_time = ktime_get();
                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                    if (ret > 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "sync tx timestamp\n");
                        goto return_tx_ts;
                    }
                    if (ret < 0)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "sync tx timestamp error\n");
                        goto return_no_tx_ts;
                    }
                    else if (delta_time_us >= NTL_TSU_TX_TS_TIMEOUT_MICROSECOND)
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no sync tx timestamp\n");
                    }
#ifdef NTL_TSU_IRQ_MODE_TX
                    else if ((tsu->irq_mode != 0) && (ret == 0))
                    {
                        // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                        wait_event_interruptible_timeout(tsu->wait_queue_sync_tx, (0 == list_empty(&(tsu->data_queue_sync_tx))), usecs_to_jiffies(NTL_TSU_TX_TS_TIMEOUT_MICROSECOND/10));
                    }
#endif
                    break;

#ifdef NTL_TSU_NTP_SUPPORT
                case NTL_TSU_NTP_MSG:
                    if (tsu->ntp_support != 0)
                    {
                        // start spinlock section
                        spin_lock_irqsave(&(tsu->lock), flags);

#ifdef NTL_TSU_META_INFO
                        if (tsu->meta_mode != 0)
                        {
#ifdef NTL_TSU_IRQ_MODE_TX
                            if (tsu->irq_mode != 0)
                            {
                                //nothing
                            }
                            else
#endif
                            {
                                // read all timestamps
                                delta_time_us = 0; // ensure at least one read
                                while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_NTP_TX_ERROR_BIT, NTL_TSU_TS_STATUS_NTP_TX_BIT, NTL_TSU_NTP_TX_L_REG, NTL_TSU_NTP_TX_H_REG, &second, &nanosecond,
                                                                                     NTL_TSU_META_NTP_TX_0_REG, NTL_TSU_META_NTP_TX_1_REG, NTL_TSU_META_NTP_TX_2_REG, NTL_TSU_META_NTP_TX_3_REG, &meta_info_ts)) &&
                                       (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
                                {
                                    // add to the front of the queue
                                    ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_ntp_tx), &(tsu->data_queue_entry_count_ntp_tx), &meta_info_ts, &second, &nanosecond);

                                    now_time = ktime_get();
                                    delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
                                }
                            }
                            ret = ntl_tsu_find_timestamp(tsu, &(tsu->data_queue_ntp_tx), &(tsu->data_queue_entry_count_ntp_tx), &meta_info, &second, &nanosecond);
                        }
                        else
#endif
                        {
                            ret = ntl_tsu_get_timestamp(tsu, NTL_TSU_TS_STATUS_NTP_TX_ERROR_BIT, NTL_TSU_TS_STATUS_NTP_TX_BIT, NTL_TSU_NTP_TX_L_REG, NTL_TSU_NTP_TX_H_REG, &second, &nanosecond);
                        }

                        // end spinlock section
                        spin_unlock_irqrestore(&(tsu->lock), flags);

                        now_time = ktime_get();
                        delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

                        if (ret > 0)
                        {
                            NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "ntp tx timestamp\n");
                            goto return_tx_ts;
                        }
                        if (ret < 0)
                        {
                            NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "ntp tx timestamp error\n");
                            goto return_no_tx_ts;
                        }
                        else if (delta_time_us >= NTL_TSU_TX_TS_TIMEOUT_MICROSECOND)
                        {
                            NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "no ntp tx timestamp\n");
                        }
#ifdef NTL_TSU_IRQ_MODE_TX
                        else if ((tsu->irq_mode != 0) && (ret == 0))
                        {
                            // wait that someone wakes us up (or timeout, one 10th of the complete timeout)
                            wait_event_interruptible_timeout(tsu->wait_queue_ntp_tx, (0 == list_empty(&(tsu->data_queue_ntp_tx))), usecs_to_jiffies(NTL_TSU_TX_TS_TIMEOUT_MICROSECOND/10));
                        }
#endif
                    }
                    else
                    {
                        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "not NTP support\n");
                        goto return_no_tx_ts;
                    }
                    break;
#endif

                default:
                    NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "not supported tx message type\n");
                    goto return_no_tx_ts;
            }

            NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "retrying to fetch tx timestamp\n");

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));

        } while ((delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND));

        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "waiting for tx timestamp timed out\n");
    }
#ifdef NTL_TSU_NON_TSU_TS
    else
    {
        // this should create a software timestamp
        skb_tstamp_tx(skb, NULL); 
        return;
    }
#endif

return_no_tx_ts:
    // default no timestamp
    second = 0;
    nanosecond = 0;
    return; // don't feed to the network stack, we should have a timestamp but don't

return_tx_ts:
    if (skip != 0 && tsu->tx_type != HWTSTAMP_TX_ON)
    {
        NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "skipping to feed timestamp to network stack (one-step)\n");
        return; // don't feed to the network stack since ptp4l was not expecting a timestamp in this case
    }

    timestamps = skb_hwtstamps(skb);
    memset(timestamps, 0, sizeof(struct skb_shared_hwtstamps));
	timestamps->hwtstamp = ns_to_ktime((uint64_t)(((uint64_t)second * NS_PER_SEC) + (uint64_t)nanosecond));
    skb_tstamp_tx(skb, skb_hwtstamps(skb));
}

/*****************************************************************************/
/* Clear all timestamps                                                      */
/*****************************************************************************/
void ntl_tsu_clear_alltstamp(struct ntl_tsu* tsu)
{
#ifdef NTL_TSU_META_INFO
    struct ntl_tsu_timestamp_list* timestamp;
#endif
    unsigned long flags;
    uint32_t reg_data;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&tsu->enable))
    {
        //NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "tsu not enabled\n");
        return;
    }

    NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "clearing all timestamps!\n");

    // start spinlock section
    spin_lock_irqsave(&(tsu->lock), flags);

    // disable all TS
    reg_data = 0;

    // write ts control register
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_CONTROL_REG, &reg_data);

    // clear error and timestamp valid
    reg_data = NTL_TSU_TS_STATUS_DELAY_REQ_RX_ERROR_BIT | NTL_TSU_TS_STATUS_DELAY_REQ_RX_BIT |
               NTL_TSU_TS_STATUS_DELAY_REQ_TX_ERROR_BIT | NTL_TSU_TS_STATUS_DELAY_REQ_TX_BIT |
               NTL_TSU_TS_STATUS_PDELAY_REQ_RX_ERROR_BIT | NTL_TSU_TS_STATUS_PDELAY_REQ_RX_BIT |
               NTL_TSU_TS_STATUS_PDELAY_REQ_TX_ERROR_BIT | NTL_TSU_TS_STATUS_PDELAY_REQ_TX_BIT |
               NTL_TSU_TS_STATUS_PDELAY_RESP_RX_ERROR_BIT | NTL_TSU_TS_STATUS_PDELAY_RESP_RX_BIT |
               NTL_TSU_TS_STATUS_PDELAY_RESP_TX_ERROR_BIT | NTL_TSU_TS_STATUS_PDELAY_RESP_TX_BIT |
               NTL_TSU_TS_STATUS_SYNC_RX_ERROR_BIT | NTL_TSU_TS_STATUS_SYNC_RX_BIT |
               NTL_TSU_TS_STATUS_SYNC_TX_ERROR_BIT | NTL_TSU_TS_STATUS_SYNC_TX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
    reg_data |= NTL_TSU_TS_STATUS_NTP_RX_ERROR_BIT | NTL_TSU_TS_STATUS_NTP_RX_BIT |
                NTL_TSU_TS_STATUS_NTP_TX_ERROR_BIT | NTL_TSU_TS_STATUS_NTP_TX_BIT;
#endif

    // write ts status register
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_STATUS_REG, &reg_data);

#ifdef NTL_TSU_IRQ_MODE
    if (tsu->irq_mode != 0)
    {
        // disable irqs
        disable_irq(tsu->irq);

        // disable irq
        reg_data = 0;

        // write irq mask register
        ntl_tsu_write_reg(tsu, NTL_TSU_TS_IRQMASK_REG, &reg_data);

        // clear potential pending irq
        reg_data = NTL_TSU_TS_IRQ_DELAY_REQ_RX_BIT |
                   NTL_TSU_TS_IRQ_DELAY_REQ_TX_BIT |
                   NTL_TSU_TS_IRQ_PDELAY_REQ_RX_BIT |
                   NTL_TSU_TS_IRQ_PDELAY_REQ_TX_BIT |
                   NTL_TSU_TS_IRQ_PDELAY_RESP_RX_BIT |
                   NTL_TSU_TS_IRQ_PDELAY_RESP_TX_BIT |
                   NTL_TSU_TS_IRQ_SYNC_RX_BIT |
                   NTL_TSU_TS_IRQ_SYNC_TX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
        reg_data |= NTL_TSU_TS_IRQ_NTP_RX_BIT |
                    NTL_TSU_TS_IRQ_NTP_TX_BIT;
#endif

        // write irq register
        ntl_tsu_write_reg(tsu, NTL_TSU_TS_IRQ_REG, &reg_data);
#endif

#ifdef NTL_TSU_META_INFO
        // empty all pending timestamps
        while (0 == list_empty(&(tsu->data_queue_delay_req_rx)))
        {
            timestamp = list_entry(tsu->data_queue_delay_req_rx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_delay_req_rx.next);
            kfree(timestamp);
        }
        while (0 == list_empty(&(tsu->data_queue_delay_req_tx)))
        {
            timestamp = list_entry(tsu->data_queue_delay_req_tx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_delay_req_tx.next);
            kfree(timestamp);
        }
        while (0 == list_empty(&(tsu->data_queue_pdelay_req_rx)))
        {
            timestamp = list_entry(tsu->data_queue_pdelay_req_rx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_pdelay_req_rx.next);
            kfree(timestamp);
        }
        while (0 == list_empty(&(tsu->data_queue_pdelay_req_tx)))
        {
            timestamp = list_entry(tsu->data_queue_pdelay_req_tx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_pdelay_req_tx.next);
            kfree(timestamp);
        }
        while (0 == list_empty(&(tsu->data_queue_pdelay_resp_rx)))
        {
            timestamp = list_entry(tsu->data_queue_pdelay_resp_rx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_pdelay_resp_rx.next);
            kfree(timestamp);
        }
        while (0 == list_empty(&(tsu->data_queue_pdelay_resp_tx)))
        {
            timestamp = list_entry(tsu->data_queue_pdelay_resp_tx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_pdelay_resp_tx.next);
            kfree(timestamp);
        }
        while (0 == list_empty(&(tsu->data_queue_sync_rx)))
        {
            timestamp = list_entry(tsu->data_queue_sync_rx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_sync_rx.next);
            kfree(timestamp);
        }
        while (0 == list_empty(&(tsu->data_queue_sync_tx)))
        {
            timestamp = list_entry(tsu->data_queue_sync_tx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_sync_tx.next);
            kfree(timestamp);
        }

#ifdef NTL_TSU_NTP_SUPPORT
        while (0 == list_empty(&(tsu->data_queue_ntp_rx)))
        {
            timestamp = list_entry(tsu->data_queue_ntp_rx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_ntp_rx.next);
            kfree(timestamp);
        }
        while (0 == list_empty(&(tsu->data_queue_ntp_tx)))
        {
            timestamp = list_entry(tsu->data_queue_ntp_tx.next, struct ntl_tsu_timestamp_list, list);
            list_del(tsu->data_queue_ntp_tx.next);
            kfree(timestamp);
        }
#endif

        // set to empty
        atomic_set(&(tsu->data_queue_entry_count_delay_req_rx), 0);
        atomic_set(&(tsu->data_queue_entry_count_pdelay_req_rx), 0);
        atomic_set(&(tsu->data_queue_entry_count_pdelay_resp_rx), 0);
        atomic_set(&(tsu->data_queue_entry_count_sync_rx), 0);
#ifdef NTL_TSU_NTP_SUPPORT
        atomic_set(&(tsu->data_queue_entry_count_ntp_rx), 0);
#endif
        atomic_set(&(tsu->data_queue_entry_count_delay_req_tx), 0);
        atomic_set(&(tsu->data_queue_entry_count_pdelay_req_tx), 0);
        atomic_set(&(tsu->data_queue_entry_count_pdelay_resp_tx), 0);
        atomic_set(&(tsu->data_queue_entry_count_sync_tx), 0);
#ifdef NTL_TSU_NTP_SUPPORT
        atomic_set(&(tsu->data_queue_entry_count_ntp_tx), 0);
#endif

#endif

#ifdef NTL_TSU_IRQ_MODE
        // start with no irqs
        reg_data = 0;

#ifdef NTL_TSU_IRQ_MODE_RX
        // enable irq
        reg_data |= NTL_TSU_TS_IRQMASK_DELAY_REQ_RX_BIT |
                    NTL_TSU_TS_IRQMASK_PDELAY_REQ_RX_BIT |
                    NTL_TSU_TS_IRQMASK_PDELAY_RESP_RX_BIT |
                    NTL_TSU_TS_IRQMASK_SYNC_RX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
    if (tsu->ntp_support != 0)
    {
        reg_data |= NTL_TSU_TS_IRQMASK_NTP_RX_BIT;
    }
#endif
#endif

#ifdef NTL_TSU_IRQ_MODE_TX
        // enable irq
        reg_data |= NTL_TSU_TS_IRQMASK_DELAY_REQ_TX_BIT |
                    NTL_TSU_TS_IRQMASK_PDELAY_REQ_TX_BIT |
                    NTL_TSU_TS_IRQMASK_PDELAY_RESP_TX_BIT |
                    NTL_TSU_TS_IRQMASK_SYNC_TX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
    if (tsu->ntp_support != 0)
    {
        reg_data |= NTL_TSU_TS_IRQMASK_NTP_TX_BIT;
    }
#endif
#endif

        // write irq mask register
        ntl_tsu_write_reg(tsu, NTL_TSU_TS_IRQMASK_REG, &reg_data);

        // enable irqs
        enable_irq(tsu->irq);
    }
#endif

    // enable all ts
    reg_data = NTL_TSU_TS_CONTROL_DELAY_REQ_RX_BIT |
               NTL_TSU_TS_CONTROL_DELAY_REQ_TX_BIT |
               NTL_TSU_TS_CONTROL_PDELAY_REQ_RX_BIT |
               NTL_TSU_TS_CONTROL_PDELAY_REQ_TX_BIT |
               NTL_TSU_TS_CONTROL_PDELAY_RESP_RX_BIT |
               NTL_TSU_TS_CONTROL_PDELAY_RESP_TX_BIT |
               NTL_TSU_TS_CONTROL_SYNC_RX_BIT |
               NTL_TSU_TS_CONTROL_SYNC_TX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
    if (tsu->ntp_support != 0)
    {
        reg_data |= NTL_TSU_TS_CONTROL_NTP_RX_BIT |
                    NTL_TSU_TS_CONTROL_NTP_TX_BIT;
    }
#endif

    // write ts control register
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_CONTROL_REG, &reg_data);

    // end spinlock section
    spin_unlock_irqrestore(&(tsu->lock), flags);

}


/*****************************************************************************/
/* Get timestamp and meta information                                        */
/*****************************************************************************/
#ifdef NTL_TSU_META_INFO
static int ntl_tsu_get_timestamp_and_meta_info(struct ntl_tsu* tsu, uint32_t error_bit, uint32_t ts_bit, uint32_t ts_reg_addr_l, uint32_t ts_reg_addr_h, uint32_t* second, uint32_t* nanosecond,
                                               uint32_t meta_reg_addr_0, uint32_t meta_reg_addr_1, uint32_t meta_reg_addr_2, uint32_t meta_reg_addr_3, struct ntl_tsu_meta_info* meta_info)
{
    uint32_t reg_data;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    // clear them
    *second = 0;
    *nanosecond = 0;

    // check if a timestamp or error occurred
    ntl_tsu_read_reg(tsu, NTL_TSU_TS_STATUS_REG, &reg_data);

    if ((reg_data & error_bit) != 0) // check if a timestamp error occurred
    {
        NTL_TSU_DPRINTK(NTL_TSU_WARNING_LEVEL, "timestamp error bit set 0x%08X (reg: 0x%08X)\n", error_bit, reg_data);
    }

    if ((reg_data & (error_bit | ts_bit)) == error_bit) // check if a timestamp error occurred without ts
    {
        // clear error
        reg_data = error_bit;
        ntl_tsu_write_reg(tsu, NTL_TSU_TS_STATUS_REG, &reg_data);
        return -1;
    }
    else
    {
        if ((reg_data & ts_bit) != 0) // check if a timestamp is ready
        {
            // get timestamp
            ntl_tsu_read_reg(tsu, ts_reg_addr_l, nanosecond);
            ntl_tsu_read_reg(tsu, ts_reg_addr_h, second);
            // get meta info
            ntl_tsu_read_reg(tsu, meta_reg_addr_0, &reg_data);
            meta_info->clock_identity[0] = (uint8_t)(((reg_data >> 0)) & 0xFF);
            meta_info->clock_identity[1] = (uint8_t)(((reg_data >> 8)) & 0xFF);
            meta_info->clock_identity[2] = (uint8_t)(((reg_data >> 16)) & 0xFF);
            meta_info->clock_identity[3] = (uint8_t)(((reg_data >> 24)) & 0xFF);
            ntl_tsu_read_reg(tsu, meta_reg_addr_1, &reg_data);
            meta_info->clock_identity[4] = (uint8_t)(((reg_data >> 0)) & 0xFF);
            meta_info->clock_identity[5] = (uint8_t)(((reg_data >> 8)) & 0xFF);
            meta_info->clock_identity[6] = (uint8_t)(((reg_data >> 16)) & 0xFF);
            meta_info->clock_identity[7] = (uint8_t)(((reg_data >> 24)) & 0xFF);
            ntl_tsu_read_reg(tsu, meta_reg_addr_2, &reg_data);
            meta_info->port_number = (uint16_t)(((reg_data >> 0)) & 0xFFFF);
            meta_info->sequence_identity = (uint16_t)(((reg_data >> 16)) & 0xFFFF);
            ntl_tsu_read_reg(tsu, meta_reg_addr_3, &reg_data);
            meta_info->domain_number = (uint8_t)(((reg_data >> 0)) & 0xFF);
            // clear timestamp valid and a potential error
            reg_data = ts_bit | error_bit;
            ntl_tsu_write_reg(tsu, NTL_TSU_TS_STATUS_REG, &reg_data);
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return 0;
}
#endif

/*****************************************************************************/
/* Push timestamp                                                            */
/*****************************************************************************/
#ifdef NTL_TSU_META_INFO
static int ntl_tsu_push_timestamp(struct ntl_tsu* tsu, struct list_head* data_queue, atomic_t* data_queue_entry_count, struct ntl_tsu_meta_info* meta_info, uint32_t* second, uint32_t* nanosecond)
{
    struct ntl_tsu_timestamp_list* timestamp;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    timestamp = (struct ntl_tsu_timestamp_list*)kmalloc(sizeof(struct ntl_tsu_timestamp_list), GFP_ATOMIC);
    if (!timestamp)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "timestamp malloc failed\n");
        return -1;
    }

    // fill timestamp
    timestamp->timestamp.second = *second;
    timestamp->timestamp.nanosecond = *nanosecond;
    memcpy(&(timestamp->timestamp.meta_info), meta_info, sizeof(struct ntl_tsu_meta_info));

    // print what we push
    NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "timestamp pushed:\n");
    ntl_tsu_print_timestamp(&(timestamp->timestamp), NTL_TSU_INFO_LEVEL);

    // add to the front of the queue
    list_add(&(timestamp->list), data_queue);

    // increase the number
    atomic_inc(data_queue_entry_count);

    // to not have a memory leak
    while (atomic_read(data_queue_entry_count) > NTL_TSU_TS_QUEUE_MAX_ENTRIES)
    {
        NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "queue full, removing oldest node:\n");
        // get the oldest timestamp
        timestamp = list_entry(data_queue->prev, struct ntl_tsu_timestamp_list, list);

        // remove oldest (this should be save this way since it is anyhow not empty)
        list_del(data_queue->prev);
        kfree(timestamp);

        // decrese the number
        atomic_dec(data_queue_entry_count);
    }

    return 0;
}
#endif

/*****************************************************************************/
/* Find timestamp                                                            */
/*****************************************************************************/
#ifdef NTL_TSU_META_INFO
static inline int ntl_tsu_compare_meta_info(struct ntl_tsu_meta_info* meta_info_l, struct ntl_tsu_meta_info* meta_info_r)
{
    int ret;

    ret = memcmp(meta_info_l->clock_identity, meta_info_r->clock_identity, sizeof(meta_info_l->clock_identity));
    if (ret == 0)
    {
        ret = memcmp(&(meta_info_l->port_number), &(meta_info_r->port_number), sizeof(meta_info_l->port_number));
        if (ret == 0)
        {
            ret = memcmp(&(meta_info_l->sequence_identity), &(meta_info_r->sequence_identity), sizeof(meta_info_l->sequence_identity));
            if (ret == 0)
            {
                ret = memcmp(&(meta_info_l->domain_number), &(meta_info_r->domain_number), sizeof(meta_info_l->domain_number));
                return ret;
            }
            return ret;
        }
        return ret;
    }
    return ret;
}

static int ntl_tsu_find_timestamp(struct ntl_tsu* tsu, struct list_head* data_queue, atomic_t* data_queue_entry_count, struct ntl_tsu_meta_info* meta_info, uint32_t* second, uint32_t* nanosecond)
{
    struct list_head* queue_ptr;
    struct list_head* queue_ptr_next;
    struct ntl_tsu_timestamp_list* timestamp;
    uint32_t ret;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    // print what we are looking for
    NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "searching timestamp with:\n");
    ntl_tsu_print_meta_info(meta_info, NTL_TSU_INFO_LEVEL);

    // say not found yet
    ret = 0;

    // clear them
    *second = 0;
    *nanosecond = 0;

    // go over the whole list
    queue_ptr = data_queue->next;
    while (queue_ptr != data_queue)
    {
        timestamp = list_entry(queue_ptr, struct ntl_tsu_timestamp_list, list);

        if (ret == 0)
        {
            // print what we are having here
            NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "timestamp entry with:\n");
            ntl_tsu_print_meta_info(&(timestamp->timestamp.meta_info), NTL_TSU_INFO_LEVEL);

            // check if it is the one that we look for
            if (0 == ntl_tsu_compare_meta_info(&(timestamp->timestamp.meta_info), meta_info))
            {
                // print what we have found
                NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "timestamp found:\n");
                ntl_tsu_print_timestamp(&(timestamp->timestamp), NTL_TSU_INFO_LEVEL);

                *second = timestamp->timestamp.second;
                *nanosecond = timestamp->timestamp.nanosecond;
                NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "found timestamp\n");
                ret = 1;
            }
        }

        // save next
        queue_ptr_next = queue_ptr->next;

        // delete found and older timestamps (it is not possible that we have timestamps out of sync)
        if (ret == 1)
        {
            // print what we are deleting
            NTL_TSU_DPRINTK(NTL_TSU_INFO_LEVEL, "deleting timestamp with:\n");
            ntl_tsu_print_meta_info(&(timestamp->timestamp.meta_info), NTL_TSU_INFO_LEVEL);

            // remove
            list_del(queue_ptr);
            kfree(timestamp);

             // decrese the number
            atomic_dec(data_queue_entry_count);
       }

        // step one further
        queue_ptr = queue_ptr_next;
    }

    return ret;
}
#endif

/*****************************************************************************/
/* Irq                                                                       */
/*****************************************************************************/
#ifdef NTL_TSU_IRQ_MODE
static irqreturn_t ntl_tsu_irq(int irq, void* dev)
{
    uint32_t reg_data;
    uint32_t irqs;
    ktime_t start_time;
    ktime_t now_time;
    int64_t delta_time_us = 0;
    struct ntl_tsu* tsu = (struct ntl_tsu*)dev;
    uint32_t second;
    uint32_t nanosecond;
    struct ntl_tsu_meta_info meta_info_ts;
    unsigned long flags;
    int ret;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    // start spinlock section
    spin_lock_irqsave(&(tsu->lock), flags);

    //start_time = jiffies + usecs_to_jiffies(NTL_TSU_RX_TS_TIMEOUT_MICROSECOND);
    start_time = ktime_get();
    now_time = start_time;

    // read irq register
    ntl_tsu_read_reg(tsu, NTL_TSU_TS_IRQ_REG, &reg_data);

    irqs = reg_data;

    // do we have any pending irq?
    if (0 == (irqs & (NTL_TSU_TS_IRQ_DELAY_REQ_RX_BIT |
                      NTL_TSU_TS_IRQ_DELAY_REQ_TX_BIT |
                      NTL_TSU_TS_IRQ_PDELAY_REQ_RX_BIT |
                      NTL_TSU_TS_IRQ_PDELAY_REQ_TX_BIT |
                      NTL_TSU_TS_IRQ_PDELAY_RESP_RX_BIT |
                      NTL_TSU_TS_IRQ_PDELAY_RESP_TX_BIT |
                      NTL_TSU_TS_IRQ_SYNC_RX_BIT |
                      NTL_TSU_TS_IRQ_SYNC_TX_BIT |
                      NTL_TSU_TS_IRQ_NTP_RX_BIT |
                      NTL_TSU_TS_IRQ_NTP_TX_BIT))) // we need to take NTP also into account
    {
        // end spinlock section
        spin_unlock_irqrestore(&(tsu->lock), flags);
        return IRQ_HANDLED;
    }

    if (0 == atomic_read(&tsu->enable))
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "device not enabled\n");
    }

    // clear potential pending irq
    reg_data = irqs;

    // write irq register (this is actually not acknowledging the timestamp)
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_IRQ_REG, &reg_data);

#ifdef NTL_TSU_IRQ_MODE_RX
    if (0 != (irqs & NTL_TSU_TS_IRQ_DELAY_REQ_RX_BIT))
    {
        delta_time_us = 0; // ensure at least one read
        while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_DELAY_REQ_RX_ERROR_BIT, NTL_TSU_TS_STATUS_DELAY_REQ_RX_BIT, NTL_TSU_DELAY_REQ_RX_L_REG, NTL_TSU_DELAY_REQ_RX_H_REG, &second, &nanosecond,
                                                             NTL_TSU_META_DELAY_REQ_RX_0_REG, NTL_TSU_META_DELAY_REQ_RX_1_REG, NTL_TSU_META_DELAY_REQ_RX_2_REG, NTL_TSU_META_DELAY_REQ_RX_3_REG, &meta_info_ts)) &&
               (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
        {
            // add to the front of the queue
            ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_delay_req_rx), &(tsu->data_queue_entry_count_delay_req_rx), &meta_info_ts, &second, &nanosecond);

            if (ret == 0)
            {
                // wake up any sleeping process
                wake_up_interruptible(&(tsu->wait_queue_delay_req_rx));
            }

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
        }
    }

    if (0 != (irqs & NTL_TSU_TS_IRQ_PDELAY_REQ_RX_BIT))
    {
        delta_time_us = 0; // ensure at least one read
        while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_PDELAY_REQ_RX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_REQ_RX_BIT, NTL_TSU_PDELAY_REQ_RX_L_REG, NTL_TSU_PDELAY_REQ_RX_H_REG, &second, &nanosecond,
                                                             NTL_TSU_META_PDELAY_REQ_RX_0_REG, NTL_TSU_META_PDELAY_REQ_RX_1_REG, NTL_TSU_META_PDELAY_REQ_RX_2_REG, NTL_TSU_META_PDELAY_REQ_RX_3_REG, &meta_info_ts)) &&
               (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
        {
            // add to the front of the queue
            ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_pdelay_req_rx), &(tsu->data_queue_entry_count_pdelay_req_rx), &meta_info_ts, &second, &nanosecond);

            if (ret == 0)
            {
                // wake up any sleeping process
                wake_up_interruptible(&(tsu->wait_queue_pdelay_req_rx));
            }

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
        }
    }

    if (0 != (irqs & NTL_TSU_TS_IRQ_PDELAY_RESP_RX_BIT))
    {
        delta_time_us = 0; // ensure at least one read
        while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_PDELAY_RESP_RX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_RESP_RX_BIT, NTL_TSU_PDELAY_RESP_RX_L_REG, NTL_TSU_PDELAY_RESP_RX_H_REG, &second, &nanosecond,
                                                             NTL_TSU_META_PDELAY_RESP_RX_0_REG, NTL_TSU_META_PDELAY_RESP_RX_1_REG, NTL_TSU_META_PDELAY_RESP_RX_2_REG, NTL_TSU_META_PDELAY_RESP_RX_3_REG, &meta_info_ts)) &&
               (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
        {
            // add to the front of the queue
            ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_pdelay_resp_rx), &(tsu->data_queue_entry_count_pdelay_resp_rx), &meta_info_ts, &second, &nanosecond);

            if (ret == 0)
            {
                // wake up any sleeping process
                wake_up_interruptible(&(tsu->wait_queue_pdelay_resp_rx));
            }

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
        }
    }

    if (0 != (irqs & NTL_TSU_TS_IRQ_SYNC_RX_BIT))
    {
        delta_time_us = 0; // ensure at least one read
        while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_SYNC_RX_ERROR_BIT, NTL_TSU_TS_STATUS_SYNC_RX_BIT, NTL_TSU_SYNC_RX_L_REG, NTL_TSU_SYNC_RX_H_REG, &second, &nanosecond,
                                                             NTL_TSU_META_SYNC_RX_0_REG, NTL_TSU_META_SYNC_RX_1_REG, NTL_TSU_META_SYNC_RX_2_REG, NTL_TSU_META_SYNC_RX_3_REG, &meta_info_ts)) &&
               (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
        {
            // add to the front of the queue
            ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_sync_rx), &(tsu->data_queue_entry_count_sync_rx), &meta_info_ts, &second, &nanosecond);

            if (ret == 0)
            {
                // wake up any sleeping process
                wake_up_interruptible(&(tsu->wait_queue_sync_rx));
            }

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
        }
    }

#ifdef NTL_TSU_NTP_SUPPORT
    if (tsu->ntp_support != 0)
    {
        if (0 != (irqs & NTL_TSU_TS_IRQ_NTP_RX_BIT))
        {
            delta_time_us = 0; // ensure at least one read
            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_NTP_RX_ERROR_BIT, NTL_TSU_TS_STATUS_NTP_RX_BIT, NTL_TSU_NTP_RX_L_REG, NTL_TSU_NTP_RX_H_REG, &second, &nanosecond,
                                                                 NTL_TSU_META_NTP_RX_0_REG, NTL_TSU_META_NTP_RX_1_REG, NTL_TSU_META_NTP_RX_2_REG, NTL_TSU_META_NTP_RX_3_REG, &meta_info_ts)) &&
                   (delta_time_us < NTL_TSU_RX_TS_TIMEOUT_MICROSECOND))
            {
                // add to the front of the queue
                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_ntp_rx), &(tsu->data_queue_entry_count_ntp_rx), &meta_info_ts, &second, &nanosecond);

                if (ret == 0)
                {
                    // wake up any sleeping process
                    wake_up_interruptible(&(tsu->wait_queue_ntp_rx));
                }

                now_time = ktime_get();
                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
            }
        }
    }
#endif
#endif

#ifdef NTL_TSU_IRQ_MODE_TX
    if (0 != (irqs & NTL_TSU_TS_IRQ_DELAY_REQ_TX_BIT))
    {
        delta_time_us = 0; // ensure at least one read
        while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_DELAY_REQ_TX_ERROR_BIT, NTL_TSU_TS_STATUS_DELAY_REQ_TX_BIT, NTL_TSU_DELAY_REQ_TX_L_REG, NTL_TSU_DELAY_REQ_TX_H_REG, &second, &nanosecond,
                                                             NTL_TSU_META_DELAY_REQ_TX_0_REG, NTL_TSU_META_DELAY_REQ_TX_1_REG, NTL_TSU_META_DELAY_REQ_TX_2_REG, NTL_TSU_META_DELAY_REQ_TX_3_REG, &meta_info_ts)) &&
               (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
        {
            // add to the front of the queue
            ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_delay_req_tx), &(tsu->data_queue_entry_count_delay_req_tx), &meta_info_ts, &second, &nanosecond);

            if (ret == 0)
            {
                // wake up any sleeping process
                wake_up_interruptible(&(tsu->wait_queue_delay_req_tx));
            }

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
        }
    }

    if (0 != (irqs & NTL_TSU_TS_IRQ_PDELAY_REQ_TX_BIT))
    {
        delta_time_us = 0; // ensure at least one read
        while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_PDELAY_REQ_TX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_REQ_TX_BIT, NTL_TSU_PDELAY_REQ_TX_L_REG, NTL_TSU_PDELAY_REQ_TX_H_REG, &second, &nanosecond,
                                                             NTL_TSU_META_PDELAY_REQ_TX_0_REG, NTL_TSU_META_PDELAY_REQ_TX_1_REG, NTL_TSU_META_PDELAY_REQ_TX_2_REG, NTL_TSU_META_PDELAY_REQ_TX_3_REG, &meta_info_ts)) &&
               (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
        {
            // add to the front of the queue
            ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_pdelay_req_tx), &(tsu->data_queue_entry_count_pdelay_req_tx), &meta_info_ts, &second, &nanosecond);

            if (ret == 0)
            {
                // wake up any sleeping process
                wake_up_interruptible(&(tsu->wait_queue_pdelay_req_tx));
            }

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
        }
    }

    if (0 != (irqs & NTL_TSU_TS_IRQ_PDELAY_RESP_TX_BIT))
    {
        delta_time_us = 0; // ensure at least one read
        while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_PDELAY_RESP_TX_ERROR_BIT, NTL_TSU_TS_STATUS_PDELAY_RESP_TX_BIT, NTL_TSU_PDELAY_RESP_TX_L_REG, NTL_TSU_PDELAY_RESP_TX_H_REG, &second, &nanosecond,
                                                             NTL_TSU_META_PDELAY_RESP_TX_0_REG, NTL_TSU_META_PDELAY_RESP_TX_1_REG, NTL_TSU_META_PDELAY_RESP_TX_2_REG, NTL_TSU_META_PDELAY_RESP_TX_3_REG, &meta_info_ts)) &&
               (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
        {
            // add to the front of the queue
            ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_pdelay_resp_tx), &(tsu->data_queue_entry_count_pdelay_resp_tx), &meta_info_ts, &second, &nanosecond);

            if (ret == 0)
            {
                // wake up any sleeping process
                wake_up_interruptible(&(tsu->wait_queue_pdelay_resp_tx));
            }

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
        }
    }

    if (0 != (irqs & NTL_TSU_TS_IRQ_SYNC_TX_BIT))
    {
        delta_time_us = 0; // ensure at least one read
        while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_SYNC_TX_ERROR_BIT, NTL_TSU_TS_STATUS_SYNC_TX_BIT, NTL_TSU_SYNC_TX_L_REG, NTL_TSU_SYNC_TX_H_REG, &second, &nanosecond,
                                                             NTL_TSU_META_SYNC_TX_0_REG, NTL_TSU_META_SYNC_TX_1_REG, NTL_TSU_META_SYNC_TX_2_REG, NTL_TSU_META_SYNC_TX_3_REG, &meta_info_ts)) &&
               (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
        {
            // add to the front of the queue
            ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_sync_tx), &(tsu->data_queue_entry_count_sync_tx), &meta_info_ts, &second, &nanosecond);

            if (ret == 0)
            {
                // wake up any sleeping process
                wake_up_interruptible(&(tsu->wait_queue_sync_tx));
            }

            now_time = ktime_get();
            delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
        }
    }

#ifdef NTL_TSU_NTP_SUPPORT
    if (tsu->ntp_support != 0)
    {
        if (0 != (irqs & NTL_TSU_TS_IRQ_NTP_TX_BIT))
        {
            delta_time_us = 0; // ensure at least one read
            while ((0 < ntl_tsu_get_timestamp_and_meta_info(tsu, NTL_TSU_TS_STATUS_NTP_TX_ERROR_BIT, NTL_TSU_TS_STATUS_NTP_TX_BIT, NTL_TSU_NTP_TX_L_REG, NTL_TSU_NTP_TX_H_REG, &second, &nanosecond,
                                                                 NTL_TSU_META_NTP_TX_0_REG, NTL_TSU_META_NTP_TX_1_REG, NTL_TSU_META_NTP_TX_2_REG, NTL_TSU_META_NTP_TX_3_REG, &meta_info_ts)) &&
                   (delta_time_us < NTL_TSU_TX_TS_TIMEOUT_MICROSECOND))
            {
                // add to the front of the queue
                ret = ntl_tsu_push_timestamp(tsu, &(tsu->data_queue_ntp_tx), &(tsu->data_queue_entry_count_ntp_tx), &meta_info_ts, &second, &nanosecond);

                if (ret == 0)
                {
                    // wake up any sleeping process
                    wake_up_interruptible(&(tsu->wait_queue_ntp_tx));
                }

                now_time = ktime_get();
                delta_time_us = ktime_to_us(ktime_sub(now_time, start_time));
            }
        }
    }
#endif
#endif

    // end spinlock section
    spin_unlock_irqrestore(&(tsu->lock), flags);

    return IRQ_HANDLED;
}
#endif

/*****************************************************************************/
/* Driver probe                                                              */
/*****************************************************************************/
int ntl_tsu_probe(struct ntl_tsu* tsu, struct platform_device *pdev)
{
    struct device_node* of_node_tsu;
    struct platform_device *pdev_tsu;
    struct resource* mem;
#ifdef NTL_TSU_IRQ_MODE
    struct resource* irq;
#endif
    uint32_t reg_data;
    int ret;
    unsigned long flags;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "************************************************************\n");
    NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "NetTimeLogic TSU\n");
    NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "************************************************************\n");

    // create locks
    spin_lock_init(&(tsu->lock));

    // get the tsu handle
    of_node_tsu = of_parse_phandle(pdev->dev.of_node, "tsu-handle", 0);
    if (of_node_tsu == NULL)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "of_parse_phandle failed\n");
        ret = -EINVAL;
        goto err_of_parse_phandle;
    }

    // get the platform device
    pdev_tsu = of_find_device_by_node(of_node_tsu);
    if (pdev_tsu == NULL)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "of_find_device_by_node failed\n");
        ret = -EINVAL;
        goto err_of_find_device_by_node_failed;
    }

    // get reg addr
    mem = platform_get_resource(pdev_tsu, IORESOURCE_MEM, 0);
    if (mem == NULL)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "platform_get_resource mem failed\n");
        ret = -EINVAL;
        goto err_platform_get_resource_mem_failed;
    }

    printk(KERN_ERR "%s ctrl_mem@0x%016llX - 0x%016llX\n", NTL_TSU_DRIVER_NAME, (unsigned long long)mem->start, (unsigned long long)(mem->start + NTL_TSU_REGSET_SIZE -1));

    // save physical address
    tsu->physical_ctrl_base = mem->start;

    // request memory region
    if (NULL == request_mem_region(tsu->physical_ctrl_base, NTL_TSU_REGSET_SIZE, NTL_TSU_DRIVER_NAME))
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "request_mem_region failed\n");
        ret = -ENOMEM;
        goto err_request_mem_region_failed;
    }

    // map memory to ioregion
    tsu->ctrl_base = ioremap(tsu->physical_ctrl_base, NTL_TSU_REGSET_SIZE);
    if (tsu->ctrl_base == NULL)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "ioremap failed\n");
        ret = -ENOMEM;
        goto err_ioremap_failed;
    }

    // read version register
    ntl_tsu_read_reg(tsu, NTL_TSU_VERSION_REG, &reg_data);

    // check if something expected is here, these are the ones we definitely do not expect
    if ((reg_data == 0x00000000) || (reg_data == 0xDEADDEAD))
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "unexpected version received (not our core?): 0x%08X\n", reg_data);
        ret = -EINVAL;
        goto err_our_device_failed;
    }

#ifdef NTL_TSU_NTP_SUPPORT
    // start spinlock section
    spin_lock_irqsave(&(tsu->lock), flags);

    // clear errors first
    reg_data = NTL_TSU_STATUS_TIME_INVALID_BIT | NTL_TSU_STATUS_TIME_JUMP_BIT;

    // write status register
    ntl_tsu_write_reg(tsu, NTL_TSU_STATUS_REG, &reg_data);

    // read status register
    ntl_tsu_read_reg(tsu, NTL_TSU_STATUS_REG, &reg_data);
    if ((reg_data & NTL_TSU_STATUS_NTP_SUPPORT_BIT) != 0)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "NTP support available\n");
        tsu->ntp_support = 1;
    }
    else
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "no NTP support available\n");
        tsu->ntp_support = 0;
    }

    // end spinlock section
    spin_unlock_irqrestore(&(tsu->lock), flags);
#endif


#ifdef NTL_TSU_META_INFO
    // start spinlock section
    spin_lock_irqsave(&(tsu->lock), flags);

    // clear errors first
    reg_data = NTL_TSU_STATUS_TIME_INVALID_BIT | NTL_TSU_STATUS_TIME_JUMP_BIT;

    // write status register
    ntl_tsu_write_reg(tsu, NTL_TSU_STATUS_REG, &reg_data);

    // read status register
    ntl_tsu_read_reg(tsu, NTL_TSU_STATUS_REG, &reg_data);
    if ((reg_data & NTL_TSU_STATUS_META_INFO_BIT) != 0)
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "meta information available, running in meta mode\n");
        tsu->meta_mode = 1;
    }
    else
    {
        NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "no meta information available, running in direct fetch mode\n");
        tsu->meta_mode = 0;
    }

    // end spinlock section
    spin_unlock_irqrestore(&(tsu->lock), flags);

    // no events yet
    INIT_LIST_HEAD(&(tsu->data_queue_delay_req_rx));
    INIT_LIST_HEAD(&(tsu->data_queue_delay_req_tx));
    INIT_LIST_HEAD(&(tsu->data_queue_pdelay_req_rx));
    INIT_LIST_HEAD(&(tsu->data_queue_pdelay_req_tx));
    INIT_LIST_HEAD(&(tsu->data_queue_pdelay_resp_rx));
    INIT_LIST_HEAD(&(tsu->data_queue_pdelay_resp_tx));
    INIT_LIST_HEAD(&(tsu->data_queue_sync_rx));
    INIT_LIST_HEAD(&(tsu->data_queue_sync_tx));
#ifdef NTL_TSU_NTP_SUPPORT
    INIT_LIST_HEAD(&(tsu->data_queue_ntp_rx));
    INIT_LIST_HEAD(&(tsu->data_queue_ntp_tx));
#endif

    // set to empty
    atomic_set(&(tsu->data_queue_entry_count_delay_req_rx), 0);
    atomic_set(&(tsu->data_queue_entry_count_pdelay_req_rx), 0);
    atomic_set(&(tsu->data_queue_entry_count_pdelay_resp_rx), 0);
    atomic_set(&(tsu->data_queue_entry_count_sync_rx), 0);
#ifdef NTL_TSU_NTP_SUPPORT
    atomic_set(&(tsu->data_queue_entry_count_ntp_rx), 0);
#endif
    atomic_set(&(tsu->data_queue_entry_count_delay_req_tx), 0);
    atomic_set(&(tsu->data_queue_entry_count_pdelay_req_tx), 0);
    atomic_set(&(tsu->data_queue_entry_count_pdelay_resp_tx), 0);
    atomic_set(&(tsu->data_queue_entry_count_sync_tx), 0);
#ifdef NTL_TSU_NTP_SUPPORT
    atomic_set(&(tsu->data_queue_entry_count_ntp_tx), 0);
#endif

#endif

#ifdef NTL_TSU_IRQ_MODE
    NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "running in irq fetch mode\n");
    tsu->irq_mode = tsu->meta_mode; // if meta mode is there we can do irqs

    if (tsu->irq_mode != 0)
    {
        // get irq number
        irq = platform_get_resource(pdev_tsu, IORESOURCE_IRQ, 0);
        if (irq == NULL)
        {
            NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "platform_get_resource irq failed\n");
            ret = -EINVAL;
            goto err_platform_get_resource_irq_failed;
        }

        printk(KERN_ERR "%s irq@0x%016llX\n", NTL_TSU_DRIVER_NAME, (unsigned long long)irq->start);

        // save irq number
        tsu->irq = irq->start;

        // request irq
        ret = request_threaded_irq(tsu->irq, NULL, ntl_tsu_irq, IRQF_ONESHOT, NTL_TSU_DRIVER_NAME, (void*)tsu);
        if (ret != 0) {
            NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "request_irq failed\n");
            goto err_request_irq_failed;
        }

        // disable irq
        disable_irq(tsu->irq);

        // init wait queues
#ifdef NTL_TSU_IRQ_MODE_RX
        init_waitqueue_head(&(tsu->wait_queue_delay_req_rx));
        init_waitqueue_head(&(tsu->wait_queue_pdelay_req_rx));
        init_waitqueue_head(&(tsu->wait_queue_pdelay_resp_rx));
        init_waitqueue_head(&(tsu->wait_queue_sync_rx));
#ifdef NTL_TSU_NTP_SUPPORT
        init_waitqueue_head(&(tsu->wait_queue_ntp_rx));
#endif
#endif

#ifdef NTL_TSU_IRQ_MODE_TX
        init_waitqueue_head(&(tsu->wait_queue_delay_req_tx));
        init_waitqueue_head(&(tsu->wait_queue_pdelay_req_tx));
        init_waitqueue_head(&(tsu->wait_queue_pdelay_resp_tx));
        init_waitqueue_head(&(tsu->wait_queue_sync_tx));
#ifdef NTL_TSU_NTP_SUPPORT
        init_waitqueue_head(&(tsu->wait_queue_ntp_tx));
#endif
#endif
    }
#endif

    // start spinlock section
    spin_lock_irqsave(&(tsu->lock), flags);

    // enable first and link to 10Mbit also enable one step per default
    reg_data = NTL_TSU_CONTROL_ENABLE_BIT | NTL_TSU_CONTROL_ENABLE_ONESTEP_BIT;
    reg_data &= ~NTL_TSU_CONTROL_100MBIT_BIT;
    reg_data &= ~NTL_TSU_CONTROL_1000MBIT_BIT;

    // write control register
    ntl_tsu_write_reg(tsu, NTL_TSU_CONTROL_REG, &reg_data);

#ifdef NTL_TSU_IRQ_MODE
    if (tsu->irq_mode != 0)
    {
        // start with no irqs
        reg_data = 0;

#ifdef NTL_TSU_IRQ_MODE_RX
        // enable irq
        reg_data |= NTL_TSU_TS_IRQMASK_DELAY_REQ_RX_BIT |
                    NTL_TSU_TS_IRQMASK_PDELAY_REQ_RX_BIT |
                    NTL_TSU_TS_IRQMASK_PDELAY_RESP_RX_BIT |
                    NTL_TSU_TS_IRQMASK_SYNC_RX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
    if (tsu->ntp_support != 0)
    {
        reg_data |= NTL_TSU_TS_IRQMASK_NTP_RX_BIT;
    }
#endif
#endif

#ifdef NTL_TSU_IRQ_MODE_TX
        // enable irq
        reg_data |= NTL_TSU_TS_IRQMASK_DELAY_REQ_TX_BIT |
                    NTL_TSU_TS_IRQMASK_PDELAY_REQ_TX_BIT |
                    NTL_TSU_TS_IRQMASK_PDELAY_RESP_TX_BIT |
                    NTL_TSU_TS_IRQMASK_SYNC_TX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
    if (tsu->ntp_support != 0)
    {
        reg_data |= NTL_TSU_TS_IRQMASK_NTP_TX_BIT;
    }
#endif
#endif
    }
    else
    {
        // disable irq
        reg_data = 0;
    }
#else
    // disable irq
    reg_data = 0;
#endif

    // write irq mask register
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_IRQMASK_REG, &reg_data);

    // clear potential pending ts and errors
    reg_data = NTL_TSU_TS_STATUS_DELAY_REQ_RX_BIT |
               NTL_TSU_TS_STATUS_DELAY_REQ_TX_BIT |
               NTL_TSU_TS_STATUS_PDELAY_REQ_RX_BIT |
               NTL_TSU_TS_STATUS_PDELAY_REQ_TX_BIT |
               NTL_TSU_TS_STATUS_PDELAY_RESP_RX_BIT |
               NTL_TSU_TS_STATUS_PDELAY_RESP_TX_BIT |
               NTL_TSU_TS_STATUS_SYNC_RX_BIT |
               NTL_TSU_TS_STATUS_SYNC_TX_BIT |
               NTL_TSU_TS_STATUS_DELAY_REQ_RX_ERROR_BIT |
               NTL_TSU_TS_STATUS_DELAY_REQ_TX_ERROR_BIT |
               NTL_TSU_TS_STATUS_PDELAY_REQ_RX_ERROR_BIT |
               NTL_TSU_TS_STATUS_PDELAY_REQ_TX_ERROR_BIT |
               NTL_TSU_TS_STATUS_PDELAY_RESP_RX_ERROR_BIT |
               NTL_TSU_TS_STATUS_PDELAY_RESP_TX_ERROR_BIT |
               NTL_TSU_TS_STATUS_SYNC_RX_ERROR_BIT |
               NTL_TSU_TS_STATUS_SYNC_TX_ERROR_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
        reg_data |= NTL_TSU_TS_STATUS_NTP_RX_BIT |
                    NTL_TSU_TS_STATUS_NTP_TX_BIT |
                    NTL_TSU_TS_STATUS_NTP_RX_ERROR_BIT |
                    NTL_TSU_TS_STATUS_NTP_TX_ERROR_BIT;
#endif

    // write ts status register
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_STATUS_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_TSU_TS_IRQ_DELAY_REQ_RX_BIT |
               NTL_TSU_TS_IRQ_DELAY_REQ_TX_BIT |
               NTL_TSU_TS_IRQ_PDELAY_REQ_RX_BIT |
               NTL_TSU_TS_IRQ_PDELAY_REQ_TX_BIT |
               NTL_TSU_TS_IRQ_PDELAY_RESP_RX_BIT |
               NTL_TSU_TS_IRQ_PDELAY_RESP_TX_BIT |
               NTL_TSU_TS_IRQ_SYNC_RX_BIT |
               NTL_TSU_TS_IRQ_SYNC_TX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
        reg_data |= NTL_TSU_TS_IRQ_NTP_RX_BIT |
                    NTL_TSU_TS_IRQ_NTP_TX_BIT;
#endif

    // write irq register
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_IRQ_REG, &reg_data);

    // enable all ts
    reg_data = NTL_TSU_TS_CONTROL_DELAY_REQ_RX_BIT |
               NTL_TSU_TS_CONTROL_DELAY_REQ_TX_BIT |
               NTL_TSU_TS_CONTROL_PDELAY_REQ_RX_BIT |
               NTL_TSU_TS_CONTROL_PDELAY_REQ_TX_BIT |
               NTL_TSU_TS_CONTROL_PDELAY_RESP_RX_BIT |
               NTL_TSU_TS_CONTROL_PDELAY_RESP_TX_BIT |
               NTL_TSU_TS_CONTROL_SYNC_RX_BIT |
               NTL_TSU_TS_CONTROL_SYNC_TX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
    if (tsu->ntp_support != 0)
    {
        reg_data |= NTL_TSU_TS_CONTROL_NTP_RX_BIT |
                    NTL_TSU_TS_CONTROL_NTP_TX_BIT;
    }
#endif

    // write ts control register
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_CONTROL_REG, &reg_data);

    // set enabled
    atomic_set(&tsu->enable, 1);

    // read version register
    ntl_tsu_read_reg(tsu, NTL_TSU_VERSION_REG, &reg_data);

    // end spinlock section
    spin_unlock_irqrestore(&(tsu->lock), flags);

#ifdef NTL_TSU_IRQ_MODE
    if (tsu->irq_mode != 0)
    {
        // enable irqs
        enable_irq(tsu->irq);
    }
#endif

    NTL_TSU_DPRINTK(NTL_TSU_ERROR_LEVEL, "TSU version: %02x.%02x.%04x registered\n", ((reg_data >> 24) & 0xFF), ((reg_data >> 16) & 0xFF), ((reg_data >> 0) & 0xFFFF));

    return 0;

#ifdef NTL_TSU_IRQ_MODE
    disable_irq(tsu->irq);
    free_irq(tsu->irq, (void*)tsu);
err_request_irq_failed:
err_platform_get_resource_irq_failed:
#endif
err_our_device_failed:
    iounmap(tsu->ctrl_base);
err_ioremap_failed:
    release_mem_region(tsu->physical_ctrl_base, NTL_TSU_REGSET_SIZE);
err_request_mem_region_failed:
err_platform_get_resource_mem_failed:
err_of_find_device_by_node_failed:
err_of_parse_phandle:
    atomic_set(&tsu->enable, 0);

    return ret;
}

/*****************************************************************************/
/* Driver remove                                                             */
/*****************************************************************************/
int ntl_tsu_remove(struct ntl_tsu* tsu, struct platform_device *pdev)
{
#ifdef NTL_TSU_META_INFO
    struct ntl_tsu_timestamp_list* timestamp;
#endif
    uint32_t reg_data;
    unsigned long flags;

    NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "%s:%s entering function\n", NTL_TSU_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&tsu->enable))
    {
        //NTL_TSU_DPRINTK(NTL_TSU_DEBUG_LEVEL, "tsu not enabled\n");
        return 0;
    }

#ifdef NTL_TSU_IRQ_MODE
    if (tsu->irq_mode != 0)
    {
        // disable irqs
        disable_irq(tsu->irq);

        // unregister irq
        free_irq(tsu->irq, (void*)tsu);
    }
#endif

    // start spinlock section
    spin_lock_irqsave(&(tsu->lock), flags);

    // disable first
    reg_data = 0;

    // write register
    ntl_tsu_write_reg(tsu, NTL_TSU_CONTROL_REG, &reg_data);

    // disable all TS
    reg_data = 0;

    // write ts control register
    ntl_tsu_write_reg(tsu, NTL_TSU_TS_CONTROL_REG, &reg_data);

    // set diabled
    atomic_set(&tsu->enable, 0);

#ifdef NTL_TSU_IRQ_MODE
    if (tsu->irq_mode != 0)
    {
        // disable irq
        reg_data = 0;

        // write irq mask register
        ntl_tsu_write_reg(tsu, NTL_TSU_TS_IRQMASK_REG, &reg_data);

        // clear potential pending irq
        reg_data = NTL_TSU_TS_IRQ_DELAY_REQ_RX_BIT |
                   NTL_TSU_TS_IRQ_DELAY_REQ_TX_BIT |
                   NTL_TSU_TS_IRQ_PDELAY_REQ_RX_BIT |
                   NTL_TSU_TS_IRQ_PDELAY_REQ_TX_BIT |
                   NTL_TSU_TS_IRQ_PDELAY_RESP_RX_BIT |
                   NTL_TSU_TS_IRQ_PDELAY_RESP_TX_BIT |
                   NTL_TSU_TS_IRQ_SYNC_RX_BIT |
                   NTL_TSU_TS_IRQ_SYNC_TX_BIT;
#ifdef NTL_TSU_NTP_SUPPORT
        reg_data |= NTL_TSU_TS_IRQ_NTP_RX_BIT |
                    NTL_TSU_TS_IRQ_NTP_TX_BIT;
#endif

        // write irq register
        ntl_tsu_write_reg(tsu, NTL_TSU_TS_IRQ_REG, &reg_data);
    }
#endif

#ifdef NTL_TSU_META_INFO
    // empty all pending timestamps
    while (0 == list_empty(&(tsu->data_queue_delay_req_rx)))
    {
        timestamp = list_entry(tsu->data_queue_delay_req_rx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_delay_req_rx.next);
        kfree(timestamp);
    }
    while (0 == list_empty(&(tsu->data_queue_delay_req_tx)))
    {
        timestamp = list_entry(tsu->data_queue_delay_req_tx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_delay_req_tx.next);
        kfree(timestamp);
    }
    while (0 == list_empty(&(tsu->data_queue_pdelay_req_rx)))
    {
        timestamp = list_entry(tsu->data_queue_pdelay_req_rx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_pdelay_req_rx.next);
        kfree(timestamp);
    }
    while (0 == list_empty(&(tsu->data_queue_pdelay_req_tx)))
    {
        timestamp = list_entry(tsu->data_queue_pdelay_req_tx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_pdelay_req_tx.next);
        kfree(timestamp);
    }
    while (0 == list_empty(&(tsu->data_queue_pdelay_resp_rx)))
    {
        timestamp = list_entry(tsu->data_queue_pdelay_resp_rx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_pdelay_resp_rx.next);
        kfree(timestamp);
    }
    while (0 == list_empty(&(tsu->data_queue_pdelay_resp_tx)))
    {
        timestamp = list_entry(tsu->data_queue_pdelay_resp_tx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_pdelay_resp_tx.next);
        kfree(timestamp);
    }
    while (0 == list_empty(&(tsu->data_queue_sync_rx)))
    {
        timestamp = list_entry(tsu->data_queue_sync_rx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_sync_rx.next);
        kfree(timestamp);
    }
    while (0 == list_empty(&(tsu->data_queue_sync_tx)))
    {
        timestamp = list_entry(tsu->data_queue_sync_tx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_sync_tx.next);
        kfree(timestamp);
    }
#ifdef NTL_TSU_NTP_SUPPORT
    while (0 == list_empty(&(tsu->data_queue_ntp_rx)))
    {
        timestamp = list_entry(tsu->data_queue_ntp_rx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_ntp_rx.next);
        kfree(timestamp);
    }
    while (0 == list_empty(&(tsu->data_queue_ntp_tx)))
    {
        timestamp = list_entry(tsu->data_queue_ntp_tx.next, struct ntl_tsu_timestamp_list, list);
        list_del(tsu->data_queue_ntp_tx.next);
        kfree(timestamp);
    }
#endif

    // set to empty
    atomic_set(&(tsu->data_queue_entry_count_delay_req_rx), 0);
    atomic_set(&(tsu->data_queue_entry_count_pdelay_req_rx), 0);
    atomic_set(&(tsu->data_queue_entry_count_pdelay_resp_rx), 0);
    atomic_set(&(tsu->data_queue_entry_count_sync_rx), 0);
#ifdef NTL_TSU_NTP_SUPPORT
    atomic_set(&(tsu->data_queue_entry_count_ntp_rx), 0);
#endif
    atomic_set(&(tsu->data_queue_entry_count_delay_req_tx), 0);
    atomic_set(&(tsu->data_queue_entry_count_pdelay_req_tx), 0);
    atomic_set(&(tsu->data_queue_entry_count_pdelay_resp_tx), 0);
    atomic_set(&(tsu->data_queue_entry_count_sync_tx), 0);
#ifdef NTL_TSU_NTP_SUPPORT
    atomic_set(&(tsu->data_queue_entry_count_ntp_tx), 0);
#endif

#endif

    // end spinlock section
    spin_unlock_irqrestore(&(tsu->lock), flags);

    // unmap io memory
    iounmap(tsu->ctrl_base);

    // release memory
    release_mem_region(tsu->physical_ctrl_base, NTL_TSU_REGSET_SIZE);


    return 0;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NTL TSU");
MODULE_AUTHOR("Sven Meier (NetTimeLogic GmbH)");