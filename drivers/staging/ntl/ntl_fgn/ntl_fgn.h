// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL FGN driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */
 
#ifndef NTL_FGN_H
#define NTL_FGN_H

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
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include "ntl_fgn_ioctl.h"

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
#define NTL_FGN_DRIVER_NAME                         "ntl_fgn"
#define NTL_FGN_DRIVER_AUTHOR                       "NetTimeLogic GmbH, Sven Meier <sven.meier@nettimelogic.com>"
#define NTL_FGN_DRIVER_DESC                         "NetTimeLogic GmbH Driver for the Frequency Generator"
#define NTL_FGN_DRIVER_LICENSE                      "GPL"
#define NTL_FGN_DRIVER_VERSION                      "1.00"

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
/* Register Set */
#define NTL_FGN_REGSET_SIZE                         0x10000

#define NTL_FGN_CONTROL_REG                         0x00000000
#define NTL_FGN_STATUS_REG                          0x00000004
#define NTL_FGN_POLARITY_REG                        0x00000008
#define NTL_FGN_VERSION_REG                         0x0000000C
#define NTL_FGN_CABLEDELAY_REG                      0x00000020
#define NTL_FGN_FREQUENCY_REG                       0x00000030
#define NTL_FGN_CYCLESPERSECOND_REG                 0x00000034

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
#define NTL_FGN_CONTROL_ENABLE_BIT                  0x00000001
#define NTL_FGN_CONTROL_FREQUENCY_VALID_BIT         0x00000002
#define NTL_FGN_CONTROL_EMBEDDED_PPS_BIT            0x00000004

#define NTL_FGN_STATUS_ERROR_BIT                    0x00000001
#define NTL_FGN_STATUS_TIME_JUMP_BIT                0x00000002

#define NTL_FGN_POLARITY_BIT                        0x00000001

/*****************************************************************************/
/* Print macro                                                               */
/*****************************************************************************/
#define NTL_FGN_DEBUG_LEVEL                         0
#define NTL_FGN_INFO_LEVEL                          1
#define NTL_FGN_WARNING_LEVEL                       2
#define NTL_FGN_ERROR_LEVEL                         3

#define CONFIG_NTL_FGN_DEBUG
#ifdef CONFIG_NTL_FGN_DEBUG
    unsigned int ntl_fgn_debuglevel                 = NTL_FGN_ERROR_LEVEL;
    #define NTL_FGN_DPRINTK(level, value...)        {if(level >= ntl_fgn_debuglevel) {printk(KERN_INFO value);}}
    #define DRIVER_DEBUG                            "!!! DEBUG VERSION  !!!"
#else
    #define NTL_FGN_DPRINTK(level, value...)        {if(level >= NTL_FGN_ERROR_LEVEL) {printk(KERN_INFO value);}}
#endif

/*****************************************************************************/
/* Internal structure                                                        */
/*****************************************************************************/
struct ntl_fgn {
    struct cdev                                     cdev;
    struct device*                                  pdev;
    dev_t                                           dev_nr;
    atomic_t                                        open;
    void*                                           ctrl_base;
    phys_addr_t                                     physical_ctrl_base;
    spinlock_t                                      lock;
};

#endif