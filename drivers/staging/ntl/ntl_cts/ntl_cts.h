// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL CTS driver
 *
 * Copyright (C) 2026 NetTimeLogic GmbH
 */

#ifndef NTL_CTS_H
#define NTL_CTS_H

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

#include "ntl_cts_ioctl.h"

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
#define NTL_CTS_DRIVER_NAME                         "ntl_cts"
#define NTL_CTS_DRIVER_AUTHOR                       "NetTimeLogic GmbH, Sven Meier <sven.meier@nettimelogic.com>"
#define NTL_CTS_DRIVER_DESC                         "NetTimeLogic GmbH Driver for the Cross Time Stamper"
#define NTL_CTS_DRIVER_LICENSE                      "GPL"
#define NTL_CTS_DRIVER_VERSION                      "1.00"

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
/* Register Set */
#define NTL_CTS_REGSET_SIZE                         0x10000

#define NTL_CTS_CONTROL_REG                         0x00000000
#define NTL_CTS_STATUS_REG                          0x00000004
#define NTL_CTS_VERSION_REG                         0x0000000C
#define NTL_CTS_TIMEVALUEL_REG                      0x00001000
#define NTL_CTS_TIMEVALUEH_REG                      0x00001004
#define NTL_CTS_TIMESTATUS_REG                      0x00001008

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
#define NTL_CTS_CONTROL_ENABLE_BIT                  0x00000001
#define NTL_CTS_CONTROL_TRIGGER_BIT                 0x40000000
#define NTL_CTS_CONTROL_TRIGGERED_BIT               0x80000000

/*****************************************************************************/
/* Print macro                                                               */
/*****************************************************************************/
#define NTL_CTS_DEBUG_LEVEL                         0
#define NTL_CTS_INFO_LEVEL                          1
#define NTL_CTS_WARNING_LEVEL                       2
#define NTL_CTS_ERROR_LEVEL                         3

#define CONFIG_NTL_CTS_DEBUG
#ifdef CONFIG_NTL_CTS_DEBUG
    unsigned int ntl_cts_debuglevel                 = NTL_CTS_ERROR_LEVEL;
    #define NTL_CTS_DPRINTK(level, value...)        {if(level >= ntl_cts_debuglevel) {printk(KERN_INFO value);}}
    #define DRIVER_DEBUG                            "!!! DEBUG VERSION  !!!"
#else
    #define NTL_CTS_DPRINTK(level, value...)        {if(level >= NTL_CTS_ERROR_LEVEL) {printk(KERN_INFO value);}}
#endif

/*****************************************************************************/
/* Internal structure                                                        */
/*****************************************************************************/
struct ntl_cts {
    struct cdev                                     cdev;
    struct device*                                  pdev;
    dev_t                                           dev_nr;
    atomic_t                                        open;
    void*                                           ctrl_base;
    phys_addr_t                                     physical_ctrl_base;
    spinlock_t                                      lock;
    unsigned int                                    nr_of_sources;
};

#endif