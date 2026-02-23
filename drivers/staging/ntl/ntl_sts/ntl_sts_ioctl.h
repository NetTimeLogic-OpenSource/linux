#ifndef NTL_STS_IOCTL_H
#define NTL_STS_IOCTL_H

#include <linux/ioctl.h>

/*****************************************************************************/
/* General definitions                                                       */
/*****************************************************************************/
#define NTL_STS_IOCTL_MAGIC                         'c'
#define NTL_STS_IOCTL_MAX_ID                        9
#define NTL_STS_MAX_DATA_LENGTH                     1024
#define NTL_STS_DEFAULT_MAX_QUEUE_LENGTH            512

/*****************************************************************************/
/* IOCTL definitions                                                         */
/*****************************************************************************/
#define NTL_STS_ENABLE                              _IO(NTL_STS_IOCTL_MAGIC, 1)
#define NTL_STS_DISABLE                             _IO(NTL_STS_IOCTL_MAGIC, 2)
#define NTL_STS_POLARITY                            _IOW(NTL_STS_IOCTL_MAGIC, 3, int)
#define NTL_STS_CABLE_DELAY                         _IOW(NTL_STS_IOCTL_MAGIC, 4, int)
#define NTL_STS_TIMESTAMPS_DROPPED                  _IOR(NTL_STS_IOCTL_MAGIC, 5, int)
#define NTL_STS_TIMESTAMP_READY                     _IOR(NTL_STS_IOCTL_MAGIC, 6, int)
#define NTL_STS_GET_TIMESTAMP_DATA_LENGTH           _IOR(NTL_STS_IOCTL_MAGIC, 7, int)
#define NTL_STS_GET_TIMESTAMP                       _IOR(NTL_STS_IOCTL_MAGIC, 8, int)
#define NTL_STS_GET_EVENT_COUNT                     _IOR(NTL_STS_IOCTL_MAGIC, 9, int)
#define NTL_STS_SET_QUEUE_MAX_LENGTH                _IOW(NTL_STS_IOCTL_MAGIC, 10, int)


/*****************************************************************************/
/* Timestamp structure                                                        */
/*****************************************************************************/
#pragma pack(8)
struct ntl_sts_timestamp {
    uint32_t                                        count;
    uint32_t                                        second;
    uint32_t                                        nanosecond;
    uint32_t                                        data_length;
    uint8_t                                         data[NTL_STS_MAX_DATA_LENGTH];
};
#pragma pack()

#endif