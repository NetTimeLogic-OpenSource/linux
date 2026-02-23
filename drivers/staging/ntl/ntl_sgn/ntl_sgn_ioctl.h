#ifndef NTL_SGN_IOCTL_H
#define NTL_SGN_IOCTL_H

#include <linux/ioctl.h>

/*****************************************************************************/
/* General definitions                                                       */
/*****************************************************************************/
#define NTL_SGN_IOCTL_MAGIC                         'c'
#define NTL_SGN_IOCTL_MAX_ID                        7

/*****************************************************************************/
/* IOCTL definitions                                                         */
/*****************************************************************************/
#define NTL_SGN_ENABLE                              _IO(NTL_SGN_IOCTL_MAGIC, 1)
#define NTL_SGN_DISABLE                             _IO(NTL_SGN_IOCTL_MAGIC, 2)
#define NTL_SGN_POLARITY                            _IOW(NTL_SGN_IOCTL_MAGIC, 3, int)
#define NTL_SGN_CABLE_DELAY                         _IOW(NTL_SGN_IOCTL_MAGIC, 4, int)
#define NTL_SGN_ERROR                               _IOR(NTL_SGN_IOCTL_MAGIC, 5, int)
#define NTL_SGN_WAIT_ERROR                          _IOR(NTL_SGN_IOCTL_MAGIC, 6, int)
#define NTL_SGN_SET_GENERATION                      _IOW(NTL_SGN_IOCTL_MAGIC, 7, int)


/*****************************************************************************/
/* Timestamp structure                                                        */
/*****************************************************************************/
#pragma pack(8)
struct ntl_sgn_generation {
    uint32_t                                        start_second;
    uint32_t                                        start_nanosecond;
    uint32_t                                        pulse_second;
    uint32_t                                        pulse_nanosecond;
    uint32_t                                        period_second;
    uint32_t                                        period_nanosecond;
    uint32_t                                        repeat_count;
};
#pragma pack()

#endif