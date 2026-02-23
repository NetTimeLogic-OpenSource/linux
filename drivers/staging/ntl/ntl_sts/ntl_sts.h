// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL STS driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */

#ifndef NTL_STS_H
#define NTL_STS_H

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

#include "ntl_sts_ioctl.h"

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
#define NTL_STS_DRIVER_NAME                         "ntl_sts"
#define NTL_STS_DRIVER_AUTHOR                       "NetTimeLogic GmbH, Sven Meier <sven.meier@nettimelogic.com>"
#define NTL_STS_DRIVER_DESC                         "NetTimeLogic GmbH Driver for the Signal Time Stamper"
#define NTL_STS_DRIVER_LICENSE                      "GPL"
#define NTL_STS_DRIVER_VERSION                      "1.00"

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
/* Register Set */
#define NTL_STS_REGSET_SIZE                         0x10000

#define NTL_STS_CONTROL_REG                         0x00000000
#define NTL_STS_STATUS_REG                          0x00000004
#define NTL_STS_POLARITY_REG                        0x00000008
#define NTL_STS_VERSION_REG                         0x0000000C
#define NTL_STS_CABLEDELAY_REG                      0x00000020
#define NTL_STS_IRQ_REG                             0x00000030
#define NTL_STS_IRQMASK_REG                         0x00000034
#define NTL_STS_EVTCOUNT_REG                        0x00000038
#define NTL_STS_COUNT_REG                           0x00000040
#define NTL_STS_TIMEVALUEL_REG                      0x00000044
#define NTL_STS_TIMEVALUEH_REG                      0x00000048
#define NTL_STS_DATAWIDTH_REG                       0x0000004C
#define NTL_STS_DATA_REG                            0x00000050

/*****************************************************************************/
/* Register definitions                                                      */
/*****************************************************************************/
#define NTL_STS_CONTROL_ENABLE_BIT                  0x00000001

#define NTL_STS_STATUS_DROP_BIT                     0x00000001

#define NTL_STS_POLARITY_BIT                        0x00000001

#define NTL_STS_IRQ_VALID_BIT                       0x00000001

#define NTL_STS_IRQMASK_VALID_BIT                   0x00000001

/*****************************************************************************/
/* Print macro                                                               */
/*****************************************************************************/
#define NTL_STS_DEBUG_LEVEL                         0
#define NTL_STS_INFO_LEVEL                          1
#define NTL_STS_WARNING_LEVEL                       2
#define NTL_STS_ERROR_LEVEL                         3

#define CONFIG_NTL_STS_DEBUG
#ifdef CONFIG_NTL_STS_DEBUG
    unsigned int ntl_sts_debuglevel                 = NTL_STS_ERROR_LEVEL;
    #define NTL_STS_DPRINTK(level, value...)        {if(level >= ntl_sts_debuglevel) {printk(KERN_INFO value);}}
    #define DRIVER_DEBUG                            "!!! DEBUG VERSION  !!!"
#else
    #define NTL_STS_DPRINTK(level, value...)        {if(level >= NTL_STS_ERROR_LEVEL) {printk(KERN_INFO value);}}
#endif

/*****************************************************************************/
/* Internal structure                                                        */
/*****************************************************************************/
struct ntl_sts {
    struct cdev                                     cdev;
    struct device*                                  pdev;
    dev_t                                           dev_nr;
    atomic_t                                        open;
    void*                                           ctrl_base;
    phys_addr_t                                     physical_ctrl_base;
    spinlock_t                                      lock;
    wait_queue_head_t                               wait_queue;
    struct list_head                                data_queue;
    int                                             irq;
    int                                             queue_length;
    int                                             queue_max_length;
};

struct ntl_sts_timestamp_list {
    struct list_head                                list;
    struct ntl_sts_timestamp                        timestamp;
};

#endif