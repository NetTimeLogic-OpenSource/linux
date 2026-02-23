#ifndef NTL_CTS_IOCTL_H
#define NTL_CTS_IOCTL_H

#include <linux/ioctl.h>

/*****************************************************************************/
/* General definitions                                                       */
/*****************************************************************************/
#define NTL_CTS_IOCTL_MAGIC                         'c'
#define NTL_CTS_IOCTL_MAX_ID                        3

#define NTL_CTS_FLAG_IN_SYNC_MASK                   0x00000001
#define NTL_CTS_FLAG_IN_HOLDOVER_MASK               0x00000002
#define NTL_CTS_FLAG_TIME_VALID_MASK                0x00000010

/*****************************************************************************/
/* IOCTL definitions                                                         */
/*****************************************************************************/
#define NTL_CTS_TRIGGER_TIMESTAMP                   _IO(NTL_CTS_IOCTL_MAGIC, 1)
#define NTL_CTS_GET_TIMESTAMP                       _IOWR(NTL_CTS_IOCTL_MAGIC, 2, int)
#define NTL_CTS_GET_NR_OF_SOURCES                   _IOR(NTL_CTS_IOCTL_MAGIC, 3, int)

/*****************************************************************************/
/* Timestamp structure                                                        */
/*****************************************************************************/
#pragma pack(8)
struct ntl_cts_timestamp {
    uint8_t                                         source_index;
    uint32_t                                        second;
    uint32_t                                        nanosecond;
    uint32_t                                        flags;
};
#pragma pack()

#endif