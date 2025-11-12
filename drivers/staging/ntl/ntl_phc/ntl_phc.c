// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL PHC driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */

#include "ntl_phc.h"

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
static int ntl_phc_adjfine(struct ptp_clock_info* clock, long scaled_ppm);
#endif
static int ntl_phc_adjfreq(struct ptp_clock_info* clock, s32 delta);
static int ntl_phc_adjtime(struct ptp_clock_info* clock, s64 delta);
static int ntl_phc_gettime(struct ptp_clock_info* clock, struct timespec64* ts);
static int ntl_phc_settime(struct ptp_clock_info* clock, const struct timespec64* ts);
#ifdef NTL_PHC_ENABLE_CLOCK
static int ntl_phc_enable(struct ptp_clock_info* clock, struct ptp_clock_request* request, int on);
#endif

static int ntl_phc_write_reg(struct ntl_phc* phc, uint32_t reg_addr, uint32_t* reg_data);
static int ntl_phc_read_reg(struct ntl_phc* phc, uint32_t reg_addr, uint32_t* reg_data);

/*****************************************************************************/
/* Function mapping                                                          */
/*****************************************************************************/
static const struct ptp_clock_info ntl_phc_clock_info = {
    .owner                                          = THIS_MODULE,
    .name                                           = NTL_PHC_DRIVER_NAME,
    .max_adj                                        = 50000000, // 50ms/s = 50000000ppb => 1ns every clock cycle
    .n_alarm                                        = 0,
    .n_ext_ts                                       = 0,
    .n_per_out                                      = 0,
    .n_pins                                         = 0,
    .pps                                            = 0,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
    .adjfine                                        = ntl_phc_adjfine,
#endif
    .adjfreq                                        = ntl_phc_adjfreq, // deprecated
    .adjtime                                        = ntl_phc_adjtime,
    .gettime64                                      = ntl_phc_gettime,
    .settime64                                      = ntl_phc_settime,
#ifdef NTL_PHC_ENABLE_CLOCK
    .enable                                         = ntl_phc_enable,
#endif
};

/*****************************************************************************/
/* Write register                                                            */
/*****************************************************************************/
static int ntl_phc_write_reg(struct ntl_phc* phc, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    if(phc->ctrl_base == NULL)
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1;
    }
    
    iowrite32(*reg_data, (phc->ctrl_base + reg_addr));
    
    return 0;
}

/*****************************************************************************/
/* Read register                                                             */
/*****************************************************************************/
static int ntl_phc_read_reg(struct ntl_phc* phc, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    if(phc->ctrl_base == NULL)
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1 ;
    }
    
    *reg_data = ioread32((phc->ctrl_base + reg_addr));
     
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
/*****************************************************************************/
/* Adjust clock frequency fine                                               */
/*****************************************************************************/
static int ntl_phc_adjfine(struct ptp_clock_info* clock, long scaled_ppm)
{
    s32 delta;
  
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    // ppbs
    delta = 1000 * (abs(scaled_ppm) >> 16); 
    
    // if there are fractions
    if ((abs(scaled_ppm) & 0xFFFF) != 0)
    {
        delta += 1000 / (0x10000 / (abs(scaled_ppm) & 0xFFFF)); // fractional ppms rounded to 1ns
    }
    
    // recover sign
    if (scaled_ppm < 0)
    {
        delta = -1 * delta;
    }
    
    // use other function
    return ntl_phc_adjfreq(clock, delta);

}
#endif


/*****************************************************************************/
/* Adjust clock frequency                                                    */
/*****************************************************************************/
static int ntl_phc_adjfreq(struct ptp_clock_info* clock, s32 delta)
{
    struct ntl_phc *phc = container_of(clock, struct ntl_phc,
                             clock_info);
    uint32_t reg_data;
    unsigned long flags;
  
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&phc->enable))
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "clock not enabled\n");
        return -1;
    }
    
    // start spinlock section
    spin_lock_irqsave(&(phc->lock), flags);
    
    // delta is ppb = 1/10^9 = ns/s
    
    // always 10^9 = 1s    
    reg_data = NTL_PHC_SECOND_IN_NANOSECOND; 
    
    // write drift interval register
    ntl_phc_write_reg(phc, NTL_PHC_DRIFT_ADJ_INTERVAL_REG, &reg_data);
    
    // always absolute value
    reg_data = abs(delta); 
    
    // print adjustments
    if (delta < 0)
    {
        NTL_PHC_DPRINTK(NTL_PHC_INFO_LEVEL, "adjusting clock by -%d ppb\n", reg_data);
    }
    else
    {
        NTL_PHC_DPRINTK(NTL_PHC_INFO_LEVEL, "adjusting clock by +%d ppb\n", reg_data);
    }
    
    // sign bit
    if (delta < 0)
    {
        reg_data |= 0x80000000; // set sign bit
    } 
    
    // write drift value register
    ntl_phc_write_reg(phc, NTL_PHC_DRIFT_ADJ_VALUE_REG, &reg_data);
     
    // read control register
    ntl_phc_read_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
    
    // set drift bit
    reg_data |= NTL_PHC_CONTROL_DRIFT_ADJ_BIT;
    
    // write control register
    ntl_phc_write_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
     
    // end spinlock section
    spin_unlock_irqrestore(&(phc->lock), flags);
   
    return 0;
}

/*****************************************************************************/
/* Adjust clock phase                                                        */
/*****************************************************************************/
static int ntl_phc_adjtime(struct ptp_clock_info* clock, s64 delta)
{
    struct ntl_phc *phc = container_of(clock, struct ntl_phc,
                             clock_info);
    struct timespec64 ts_old;
    struct timespec64 ts_new;
    struct timespec64 ts;
    uint32_t reg_data;
    int64_t temp_delta;
    int ret;
    unsigned long flags;
  
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&phc->enable))
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "clock not enabled\n");
        return -1;
    }
    
    if (delta < 0)
    {
        temp_delta = -1 * delta;
    }
    else
    {
        temp_delta = delta;
    }
        
    
    // if adjustment is bigger or equal than a second we set the time
    if (temp_delta >= NTL_PHC_SECOND_IN_NANOSECOND)
    {
        NTL_PHC_DPRINTK(NTL_PHC_WARNING_LEVEL, "adjustment is larger than a second, setting time\n");
        
        // get old time
        ret = ntl_phc_gettime(clock, &ts_old);
        if (ret < 0)
        {
            NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "get time failed\n");
            return -1;
        }
        
        // set the time to 400 ns later, since it takes some time to read and write the time and do calculations
        timespec64_add_ns(&ts_old, 400);
        
        // fill timespec
        ts.tv_sec = temp_delta;
        // 64bit division and also remainder is modulo part (do_div makes a pointer out of the value)
        ts.tv_nsec = do_div(ts.tv_sec, NTL_PHC_SECOND_IN_NANOSECOND); 
        
        // add or subtract
        if (delta < 0)
        {
            ts_new = timespec64_sub(ts_old, ts);
        }
        else
        {
            ts_new = timespec64_add(ts_old, ts);
        }
        
        // write new time
        return ntl_phc_settime(clock, (const struct timespec64*)&ts_new);
    }
    
    // start spinlock section
    spin_lock_irqsave(&(phc->lock), flags);
    
    // delta is in naoseconds
    
    // always 80% of max sync interval => max sync rate is 2/s = 500ms => 0.8 * 500ms    
    reg_data = ((NTL_PHC_SECOND_IN_NANOSECOND / 2) / 10) * 8; 
    
    // write offset interval register
    ntl_phc_write_reg(phc, NTL_PHC_OFFSET_ADJ_INTERVAL_REG, &reg_data);
    
    // if time is larger than 0.8 * 500ms the core will make a timejump itself
    reg_data = temp_delta;
    
    // print adjustments
    if (delta < 0)
    {
        NTL_PHC_DPRINTK(NTL_PHC_INFO_LEVEL, "adjusting clock by -%d ns\n", reg_data);
    }
    else
    {
        NTL_PHC_DPRINTK(NTL_PHC_INFO_LEVEL, "adjusting clock by +%d ns\n", reg_data);
    }
    // sign bit
    if (delta < 0)
    {
        reg_data |= 0x80000000; // set sign bit
    } 
    
    // write offset value register
    ntl_phc_write_reg(phc, NTL_PHC_OFFSET_ADJ_VALUE_REG, &reg_data);
     
    // read control register
    ntl_phc_read_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
    
    // set offset bit
    reg_data |= NTL_PHC_CONTROL_OFFSET_ADJ_BIT;
    
    // write control register
    ntl_phc_write_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
     
    // end spinlock section
    spin_unlock_irqrestore(&(phc->lock), flags);
    
    return 0;
}

/*****************************************************************************/
/* Get clock time                                                            */
/*****************************************************************************/
static int ntl_phc_gettime(struct ptp_clock_info* clock, struct timespec64* ts)
{
    struct ntl_phc *phc = container_of(clock, struct ntl_phc,
                             clock_info);
    uint32_t reg_data;
    int i;
    unsigned long flags;
  
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    
    if (0 == atomic_read(&phc->enable))
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "clock not enabled\n");
        return -1;
    }
    
    // start spinlock section
    spin_lock_irqsave(&(phc->lock), flags);
    
    // read control register
    ntl_phc_read_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
    
    // set time read bit
    reg_data |= NTL_PHC_CONTROL_TIME_BIT;
    
    // write control register
    ntl_phc_write_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);

    // check if done flag is there
    for (i = 0; i < 100; i++)
    {
        // read control register
        ntl_phc_read_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
        
        if ((reg_data & NTL_PHC_CONTROL_TIME_VAL_BIT) != 0)
        {
            // read did complete
            i = 0;
            break;
        }
    }
    
    // check if timed out
    if (i >= 100)
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "getting time did not complete\n");
        // end spinlock section
        spin_unlock_irqrestore(&(phc->lock), flags);
        return -1;
    }
    
    // read time nanosecond value register
    ntl_phc_read_reg(phc, NTL_PHC_TIME_VALUE_L_REG, &reg_data);
    
    // assign nanoseconds value
    ts->tv_nsec = reg_data;
     
    // read time second value register
    ntl_phc_read_reg(phc, NTL_PHC_TIME_VALUE_H_REG, &reg_data);
     
    // assign seconds value, only 32bit
    ts->tv_sec = reg_data;

    NTL_PHC_DPRINTK(NTL_PHC_INFO_LEVEL, "got time %d s %d ns\n", (uint32_t)ts->tv_sec, (uint32_t)ts->tv_nsec);

    // end spinlock section
    spin_unlock_irqrestore(&(phc->lock), flags);
        
    return 0;
}

/*****************************************************************************/
/* Set clock time                                                            */
/*****************************************************************************/
static int ntl_phc_settime(struct ptp_clock_info* clock, const struct timespec64* ts)
{
    struct ntl_phc *phc = container_of(clock, struct ntl_phc,
                             clock_info);
    uint32_t reg_data;
    unsigned long flags;
  
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&phc->enable))
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "clock not enabled\n");
        return -1;
    }
    
    if (ts->tv_nsec >= NTL_PHC_SECOND_IN_NANOSECOND)
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "set time nanosecond value larger than one second\n");
        return -1;
    }
    
    if (ts->tv_sec >= 0x100000000)
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "set time second value larger than 32bit, skipping high part\n");
    }
    
    // start spinlock section
    spin_lock_irqsave(&(phc->lock), flags);
    
    // nanosecond value
    reg_data = ts->tv_nsec;
    
    // write time nanosecond value register
    ntl_phc_write_reg(phc, NTL_PHC_TIME_ADJ_VALUE_L_REG, &reg_data);
     
    // only 32bit second value
    reg_data = ts->tv_sec;
    
    // write time second value register
    ntl_phc_write_reg(phc, NTL_PHC_TIME_ADJ_VALUE_H_REG, &reg_data);
     
    // read control register
    ntl_phc_read_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
    
    // set time bit
    reg_data |= NTL_PHC_CONTROL_TIME_ADJ_BIT;
    
    // write control register
    ntl_phc_write_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
 
    NTL_PHC_DPRINTK(NTL_PHC_INFO_LEVEL, "set time %d s %d ns\n", (uint32_t)ts->tv_sec, (uint32_t)ts->tv_nsec);
    
    // end spinlock section
    spin_unlock_irqrestore(&(phc->lock), flags);
    
    return 0;
}

#ifdef NTL_PHC_ENABLE_CLOCK
/*****************************************************************************/
/* Enable/Disable auxilary functions                                         */
/*****************************************************************************/
static int ntl_phc_enable(struct ptp_clock_info* clock, struct ptp_clock_request* request, int on)
{
    struct ntl_phc *phc = container_of(clock, struct ntl_phc,
                             clock_info);
    uint32_t reg_data;
    unsigned long flags;
  
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    // start spinlock section
    spin_lock_irqsave(&(phc->lock), flags);
    
    // read control register
    ntl_phc_read_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
    
    // change enable bit
    if (on == 0)
    {
        reg_data &= ~NTL_PHC_CONTROL_ENABLE_BIT;
    }
    else
    {
        reg_data |= NTL_PHC_CONTROL_ENABLE_BIT;
    }
    
    // write control register
    ntl_phc_write_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
    
    // change enable flag
    if (on == 0)
    {
        atomic_set(&phc->enable, 0);
    }
    else
    {
        atomic_set(&phc->enable, 1);
    }
    
    // end spinlock section
    spin_unlock_irqrestore(&(phc->lock), flags);
    
    return 0;
}
#endif

/*****************************************************************************/
/* Driver probe                                                              */
/*****************************************************************************/
static int ntl_phc_probe(struct platform_device *pdev)
{
    struct ntl_phc *phc;
    struct resource* mem;
    uint32_t reg_data;
    int ret;
    unsigned long flags;

    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "************************************************************\n");
    NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "NetTimeLogic PHC\n");
    NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "************************************************************\n");

    // alloc structure 
    phc = (struct ntl_phc*)kzalloc(sizeof(struct ntl_phc), GFP_KERNEL);
    if (phc == NULL)
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "kzalloc failed\n");
        ret = -ENOMEM;
        goto err_phc_alloc_failed;
    }
    
    // init structure
    phc->clock_info = ntl_phc_clock_info;
    phc->clock = NULL;

    // create locks
    spin_lock_init(&(phc->lock));

    // map the private structure to the device
    dev_set_drvdata(&pdev->dev, phc);
    phc->pdev = &pdev->dev;

    // get reg addr    
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (mem == NULL)
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "platform_get_resource failed\n");
        ret = -EINVAL;
        goto err_platform_get_resource_failed;
    }  

    printk(KERN_ERR "%s ctrl_mem@0x%08X - 0x%08X\n", NTL_PHC_DRIVER_NAME, mem->start, (mem->start + NTL_PHC_REGSET_SIZE -1));

    // save physical address
    phc->physical_ctrl_base = mem->start;
    
    // request memory region
    if (NULL == request_mem_region(phc->physical_ctrl_base, NTL_PHC_REGSET_SIZE, NTL_PHC_DRIVER_NAME))
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "request_mem_region failed\n");
        ret = -ENOMEM;
        goto err_request_mem_region_failed;
    }

    // map memory to ioregion
    phc->ctrl_base = ioremap(phc->physical_ctrl_base, NTL_PHC_REGSET_SIZE);
    if (phc->ctrl_base == NULL)
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "ioremap failed\n");
        ret = -ENOMEM;
        goto err_ioremap_failed;
    }

    // read version register
    ntl_phc_read_reg(phc, NTL_PHC_VERSION_REG, &reg_data);
    
    // check if something expected is here, these are the ones we definitely do not expect, 0 and DEADDEAD
    if ((reg_data == 0x00000000) || (reg_data == 0xDEADDEAD)) 
    {
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "not a ntl adjustable clock device, version number wrong: 0x%08X\n", reg_data);
        ret = -EINVAL;
        goto err_our_device_failed;
    } 

    // start spinlock section
    spin_lock_irqsave(&(phc->lock), flags);

    // enable first
    reg_data = NTL_PHC_CONTROL_ENABLE_BIT;
    
    // write control register
    ntl_phc_write_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);

    // set enabled
    atomic_set(&phc->enable, 1);

    // select register as access
    reg_data = NTL_PHC_SELECT_REGS;
    
    // write select register
    ntl_phc_write_reg(phc, NTL_PHC_SELECT_REG, &reg_data);

    // set threshold for in sync to 1us 
    reg_data = 1000;
    
    // write threshold register
    ntl_phc_write_reg(phc, NTL_PHC_IN_SYNC_THRESHOLD_REG, &reg_data);

    // read version register
    ntl_phc_read_reg(phc, NTL_PHC_VERSION_REG, &reg_data);
    
    // end spinlock section
    spin_unlock_irqrestore(&(phc->lock), flags);

    // register clock device
    phc->clock = ptp_clock_register(&phc->clock_info, phc->pdev);
    if (IS_ERR(phc->clock)) {
        phc->clock = NULL;
        NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "ptp_clock_register failed\n");
        ret = -EINVAL;
        goto err_ptp_clock_register_failed;
    }
    
    NTL_PHC_DPRINTK(NTL_PHC_ERROR_LEVEL, "PHC version: %02x.%02x.%04x registered\n", ((reg_data >> 24) & 0xFF), ((reg_data >> 16) & 0xFF), ((reg_data >> 0) & 0xFFFF));
    
    return 0;

err_ptp_clock_register_failed:
err_our_device_failed:
    iounmap(phc->ctrl_base);
err_ioremap_failed:
    release_mem_region(phc->physical_ctrl_base, NTL_PHC_REGSET_SIZE);
err_request_mem_region_failed:
err_platform_get_resource_failed:
    kfree(phc);
err_phc_alloc_failed:

    return ret;
}

/*****************************************************************************/
/* Driver remove                                                             */
/*****************************************************************************/
static int ntl_phc_remove(struct platform_device *pdev)
{
    struct ntl_phc *phc = dev_get_drvdata(&pdev->dev);
    uint32_t reg_data;
    unsigned long flags;

    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    // start spinlock section
    spin_lock_irqsave(&(phc->lock), flags);

    // disable first
    reg_data = 0;
    
    // write register
    ntl_phc_write_reg(phc, NTL_PHC_CONTROL_REG, &reg_data);
    
    // set diabled
    atomic_set(&phc->enable, 0);

    // unregister clock device
    ptp_clock_unregister(phc->clock);
    
    // unmap io memory
    iounmap(phc->ctrl_base);
    
    // release memory
    release_mem_region(phc->physical_ctrl_base, NTL_PHC_REGSET_SIZE);

    // end spinlock section
    spin_unlock_irqrestore(&(phc->lock), flags);
    
    // unmap the private structure to the device
    dev_set_drvdata(&pdev->dev, NULL);

    // free structure 
    kfree(phc);
    
    return 0;
}

/*****************************************************************************/
/* Driver device mapping                                                */
/*****************************************************************************/
static struct of_device_id ntl_phc_of_match[] = {
    { .compatible = "ntl,ntl_phc", },
    { /* end of list */ }
};

MODULE_DEVICE_TABLE(of, ntl_phc_of_match);

static struct platform_driver ntl_phc_of_driver = {
    .probe = ntl_phc_probe,
    .remove = ntl_phc_remove,
    .driver = {
        .name = NTL_PHC_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = ntl_phc_of_match,
    },
};

static int __init ntl_phc_init(void)
{
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    return platform_driver_register(&ntl_phc_of_driver);
}

static void __exit ntl_phc_exit(void)
{
    NTL_PHC_DPRINTK(NTL_PHC_DEBUG_LEVEL, "%s:%s entering function\n", NTL_PHC_DRIVER_NAME, __FUNCTION__);

    platform_driver_unregister(&ntl_phc_of_driver);
}

module_init(ntl_phc_init);
module_exit(ntl_phc_exit);

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
MODULE_AUTHOR(NTL_PHC_DRIVER_AUTHOR);
MODULE_DESCRIPTION(NTL_PHC_DRIVER_DESC);
MODULE_LICENSE(NTL_PHC_DRIVER_LICENSE);