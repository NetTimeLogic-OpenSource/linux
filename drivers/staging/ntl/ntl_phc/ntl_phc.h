// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL PHC driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */

#ifndef NTL_PHC_H
#define NTL_PHC_H

#include <linux/ptp_clock_kernel.h>
#include <linux/clocksource.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
#define NTL_PHC_DRIVER_NAME                         "ntl_phc"
#define NTL_PHC_DRIVER_AUTHOR                       "NetTimeLogic GmbH, Sven Meier <sven.meier@nettimelogic.com>"
#define NTL_PHC_DRIVER_DESC                         "NetTimeLogic GmbH PHC for the Adjustable Counter Clock"
#define NTL_PHC_DRIVER_LICENSE                      "GPL"
#define NTL_PHC_DRIVER_VERSION                      "1.00"

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
/* Register Set */
#define NTL_PHC_REGSET_SIZE                         0x10000

#define NTL_PHC_CONTROL_REG                         0x00000000
#define NTL_PHC_STATUS_REG                          0x00000004
#define NTL_PHC_SELECT_REG                          0x00000008
#define NTL_PHC_VERSION_REG                         0x0000000C
#define NTL_PHC_TIME_VALUE_L_REG                    0x00000010
#define NTL_PHC_TIME_VALUE_H_REG                    0x00000014
#define NTL_PHC_TIME_ADJ_VALUE_L_REG                0x00000020
#define NTL_PHC_TIME_ADJ_VALUE_H_REG                0x00000024
#define NTL_PHC_OFFSET_ADJ_VALUE_REG                0x00000030
#define NTL_PHC_OFFSET_ADJ_INTERVAL_REG             0x00000034
#define NTL_PHC_DRIFT_ADJ_VALUE_REG                 0x00000040
#define NTL_PHC_DRIFT_ADJ_INTERVAL_REG              0x00000044
#define NTL_PHC_IN_SYNC_THRESHOLD_REG               0x00000050


/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
#define NTL_PHC_CONTROL_ENABLE_BIT                  0x00000001
#define NTL_PHC_CONTROL_TIME_ADJ_BIT                0x00000002
#define NTL_PHC_CONTROL_OFFSET_ADJ_BIT              0x00000004
#define NTL_PHC_CONTROL_DRIFT_ADJ_BIT               0x00000008
#define NTL_PHC_CONTROL_TIME_BIT                    0x40000000
#define NTL_PHC_CONTROL_TIME_VAL_BIT                0x80000000

#define NTL_PHC_STATUS_IN_SYNC_BIT                  0x00000001

#define NTL_PHC_SELECT_NONE                         0
#define NTL_PHC_SELECT_TOD                          1
#define NTL_PHC_SELECT_IRIG                         2
#define NTL_PHC_SELECT_PPS                          3
#define NTL_PHC_SELECT_PTP                          4
#define NTL_PHC_SELECT_RTC                          5
#define NTL_PHC_SELECT_DCF                          6
#define NTL_PHC_SELECT_REGS                         254
#define NTL_PHC_SELECT_EXT                          255

/*****************************************************************************/
/* Other definitions                                                         */
/*****************************************************************************/
#define NTL_PHC_SECOND_IN_NANOSECOND                1000000000

/*****************************************************************************/
/* Print macro                                                               */
/*****************************************************************************/
#define NTL_PHC_DEBUG_LEVEL                         0
#define NTL_PHC_INFO_LEVEL                          1
#define NTL_PHC_WARNING_LEVEL                       2
#define NTL_PHC_ERROR_LEVEL                         3

#define CONFIG_NTL_PHC_DEBUG
#ifdef CONFIG_NTL_PHC_DEBUG
    unsigned int ntl_phc_debuglevel                 = NTL_PHC_ERROR_LEVEL;
    #define NTL_PHC_DPRINTK(level, value...)        {if(level >= ntl_phc_debuglevel) {printk(KERN_INFO value);}}
    #define DRIVER_DEBUG                            "!!! DEBUG VERSION  !!!"
#else
    #define NTL_PHC_DPRINTK(level, value...)        {if(level >= NTL_PHC_ERROR_LEVEL) {printk(KERN_INFO value);}}
#endif

/*****************************************************************************/
/* Internal structure                                                        */
/*****************************************************************************/
struct ntl_phc {
    struct ptp_clock_info                           clock_info;
    struct ptp_clock*                               clock;
    struct device*                                  pdev;
    atomic_t                                        enable;
    void*                                           ctrl_base;
    phys_addr_t                                     physical_ctrl_base;
    spinlock_t                                      lock;
    int                                             irq;
};

#endif