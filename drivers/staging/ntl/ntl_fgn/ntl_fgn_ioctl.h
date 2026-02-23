#ifndef NTL_FGN_IOCTL_H
#define NTL_FGN_IOCTL_H

#include <linux/ioctl.h>

/*****************************************************************************/
/* General definitions                                                       */
/*****************************************************************************/
#define NTL_FGN_IOCTL_MAGIC                         'c'
#define NTL_FGN_IOCTL_MAX_ID                        5

/*****************************************************************************/
/* IOCTL definitions                                                         */
/*****************************************************************************/
#define NTL_FGN_ENABLE                              _IO(NTL_FGN_IOCTL_MAGIC, 1)
#define NTL_FGN_DISABLE                             _IO(NTL_FGN_IOCTL_MAGIC, 2)
#define NTL_FGN_POLARITY                            _IOW(NTL_FGN_IOCTL_MAGIC, 3, int)
#define NTL_FGN_CABLE_DELAY                         _IOW(NTL_FGN_IOCTL_MAGIC, 4, int)
#define NTL_FGN_EMBEDDED_PPS                        _IOW(NTL_FGN_IOCTL_MAGIC, 5, int)
#define NTL_FGN_SET_FREQUENCY                       _IOW(NTL_FGN_IOCTL_MAGIC, 6, int)


/*****************************************************************************/
/* Timestamp structure                                                        */
/*****************************************************************************/
#pragma pack(8)
struct ntl_fgn_generation {
    uint32_t                                        frequency;
};
#pragma pack()

#endif