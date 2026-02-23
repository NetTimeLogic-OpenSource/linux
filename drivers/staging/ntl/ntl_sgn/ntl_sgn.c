// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL SGN driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */

#include "ntl_sgn.h"

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/
static int ntl_sgn_open(struct inode* inode, struct file* file);
static int ntl_sgn_close(struct inode* inode, struct file* file);
static long ntl_sgn_ioctl(struct file* file, unsigned int cmd, unsigned long arg); 
static irqreturn_t ntl_sgn_irq(int irq, void *dev);

static int ntl_sgn_write_reg(struct ntl_sgn* sgn, uint32_t reg_addr, uint32_t* reg_data);
static int ntl_sgn_read_reg(struct ntl_sgn* sgn, uint32_t reg_addr, uint32_t* reg_data);

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
static unsigned int ntl_sgn_major = 0;
static unsigned int ntl_sgn_minor = 0;
static struct class* ntl_sgn_class = NULL;

/*****************************************************************************/
/* Function mapping                                                          */
/*****************************************************************************/
static const struct file_operations ntl_sgn_fops = {
    .owner                                          = THIS_MODULE,
    .open                                           = ntl_sgn_open,
    .release                                        = ntl_sgn_close,
    .unlocked_ioctl                                 = ntl_sgn_ioctl,
};

/*****************************************************************************/
/* Write register                                                            */
/*****************************************************************************/
static int ntl_sgn_write_reg(struct ntl_sgn* sgn, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);

    if(sgn->ctrl_base == NULL)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1;
    }
    
    iowrite32(*reg_data, (sgn->ctrl_base + reg_addr));
    
    return 0;
}

/*****************************************************************************/
/* Read register                                                             */
/*****************************************************************************/
static int ntl_sgn_read_reg(struct ntl_sgn* sgn, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);

    if(sgn->ctrl_base == NULL)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1 ;
    }
    
    *reg_data = ioread32((sgn->ctrl_base + reg_addr));
     
    return 0;
}

/*****************************************************************************/
/* Open device                                                               */
/*****************************************************************************/
static int ntl_sgn_open(struct inode* inode, struct file* file) 
{
    uint32_t reg_data;
    struct ntl_sgn* sgn = container_of(inode->i_cdev, struct ntl_sgn, cdev);
    unsigned long flags;
    
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);
    
    if (0 != atomic_read(&sgn->open))
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "device already open\n");
        return -EBUSY;
    }

    // start spinlock section
    spin_lock_irqsave(&(sgn->lock), flags);
    
    // increment open count
    atomic_inc(&sgn->open);
    
    // disable 
    reg_data = 0;
    
    // write control register
    ntl_sgn_write_reg(sgn, NTL_SGN_CONTROL_REG, &reg_data);
    
    // set error count
    atomic_set(&sgn->error_count, 0);
    
    // enable irq 
    reg_data = NTL_SGN_IRQMASK_VALID_BIT;
    
    // write irq mask register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQMASK_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_SGN_IRQ_VALID_BIT;
    
    // write irq register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQ_REG, &reg_data);
    
    // now assign the private pointer
    file->private_data = sgn;
    
    // end spinlock section
    spin_unlock_irqrestore(&(sgn->lock), flags);
    
    // enable irqs
    enable_irq(sgn->irq);
    
    return 0;
    
}

/*****************************************************************************/
/* Close device                                                               */
/*****************************************************************************/
static int ntl_sgn_close(struct inode* inode, struct file* file) 
{
    uint32_t reg_data;
    struct ntl_sgn* sgn = container_of(inode->i_cdev, struct ntl_sgn, cdev);
    unsigned long flags;
    
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);
        
    if (0 == atomic_read(&sgn->open))
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "device not open\n");
        return -EBUSY;
    }
    
    // disable irqs
    disable_irq(sgn->irq);
    
    // start spinlock section
    spin_lock_irqsave(&(sgn->lock), flags);
    
    // disable 
    reg_data = 0;
    
    // write control register
    ntl_sgn_write_reg(sgn, NTL_SGN_CONTROL_REG, &reg_data);

    // disable irq 
    reg_data = 0;
    
    // write irq mask register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQMASK_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_SGN_IRQ_VALID_BIT;
    
    // write irq register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQ_REG, &reg_data);

    // decrement open count
    atomic_dec(&sgn->open);
    
    // set error count
    atomic_set(&sgn->error_count, 0);
    
    // end spinlock section
    spin_unlock_irqrestore(&(sgn->lock), flags);
    
    return 0;
}

/*****************************************************************************/
/* Ioctl                                                                */
/*****************************************************************************/
static long ntl_sgn_ioctl(struct file* file, unsigned int cmd, unsigned long arg) 
{
    uint32_t reg_data;
    struct ntl_sgn_generation generation;
    unsigned int polarity = 0;
    unsigned int cable_delay = 0;
    unsigned int error = 0;
    unsigned int wait_error = 0;
    struct ntl_sgn* sgn = (struct ntl_sgn*)file->private_data;
    unsigned long flags;
    
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);
    
    // check if open
    if (0 == atomic_read(&sgn->open))
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "device not open\n");
        return -EBUSY;
    }
        
    if (_IOC_TYPE(cmd) != NTL_SGN_IOCTL_MAGIC)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "not an IOCTL for us\n");
        return -EINVAL;         
    }   
        
    if (_IOC_NR(cmd) > NTL_SGN_IOCTL_MAX_ID)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "IOCTL out of range\n");
        return -EINVAL;         
    }   
        
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "IOCTL nr: %d\n", _IOC_NR(cmd));
        
    switch (cmd)   
    {
        case NTL_SGN_ENABLE:
        {
            // start spinlock section
            spin_lock_irqsave(&(sgn->lock), flags);
    
            // enable irq 
            reg_data = NTL_SGN_IRQMASK_VALID_BIT;
            
            // write irq mask register
            ntl_sgn_write_reg(sgn, NTL_SGN_IRQMASK_REG, &reg_data);

            // clear potential pending irq
            reg_data = NTL_SGN_IRQ_VALID_BIT;
            
            // write irq register
            ntl_sgn_write_reg(sgn, NTL_SGN_IRQ_REG, &reg_data);

            // enable
            reg_data = NTL_SGN_CONTROL_ENABLE_BIT;
            
            // write control register
            ntl_sgn_write_reg(sgn, NTL_SGN_CONTROL_REG, &reg_data);
    
            NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "signal generator enabled\n");
        
            // end spinlock section
            spin_unlock_irqrestore(&(sgn->lock), flags);
            
            break;
        }
        case NTL_SGN_DISABLE:
        {
            // start spinlock section
            spin_lock_irqsave(&(sgn->lock), flags);
            
            // disable 
            reg_data = 0;
            
            // write control register
            ntl_sgn_write_reg(sgn, NTL_SGN_CONTROL_REG, &reg_data);

            // disable irq 
            reg_data = 0;
            
            // write irq mask register
            ntl_sgn_write_reg(sgn, NTL_SGN_IRQMASK_REG, &reg_data);

            // clear potential pending irq
            reg_data = NTL_SGN_IRQ_VALID_BIT;
            
            // write irq register
            ntl_sgn_write_reg(sgn, NTL_SGN_IRQ_REG, &reg_data);
            
            NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "signal generator disabled\n");
        
            // end spinlock section
            spin_unlock_irqrestore(&(sgn->lock), flags);
            
            break;
        }
        case NTL_SGN_POLARITY:
        {
            if (copy_from_user(&polarity, (void*)arg, sizeof(unsigned int))) {
                NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }   
            
            // start spinlock section
            spin_lock_irqsave(&(sgn->lock), flags);
            
            if (polarity == 0)
            {
                NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "setting polarity to falling edge\n");
                
                // falling edge
                reg_data = 0;
                
                // write polarity register
                ntl_sgn_write_reg(sgn, NTL_SGN_POLARITY_REG, &reg_data);
            }
            else
            {
                NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "setting polarity to rising edge\n");
                
                // rising edge
                reg_data = NTL_SGN_POLARITY_BIT;
                
                // write polarity register
                ntl_sgn_write_reg(sgn, NTL_SGN_POLARITY_REG, &reg_data);
            }
            
            // end spinlock section
            spin_unlock_irqrestore(&(sgn->lock), flags);
            
            break;
        }
        case NTL_SGN_CABLE_DELAY:
        {
            if (copy_from_user(&cable_delay, (void*)arg, sizeof(unsigned int))) {
                NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }   
            
            // start spinlock section
            spin_lock_irqsave(&(sgn->lock), flags);
            
            NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "setting cable delay to %d ns\n", cable_delay);
                
            // set cable delay in nanosecond
            reg_data = cable_delay;
            
            // write cable delay register
            ntl_sgn_write_reg(sgn, NTL_SGN_CABLEDELAY_REG, &reg_data);
            
            // end spinlock section
            spin_unlock_irqrestore(&(sgn->lock), flags);
            
            break;
        }
        case NTL_SGN_ERROR:
        {
            // start spinlock section
            spin_lock_irqsave(&(sgn->lock), flags);
            
            // read status
            ntl_sgn_read_reg(sgn, NTL_SGN_STATUS_REG, &reg_data);
            
            error = 0;
            // error bits set
            if ((0 == (reg_data & NTL_SGN_STATUS_ERROR_BIT)) && (0 == (reg_data & NTL_SGN_STATUS_TIME_JUMP_BIT))) 
            {
                NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "no error\n");
                error = 0;
            }
            else
            {
                NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "some error\n");
                error = 1;
            }
            
            // clear error bits
            ntl_sgn_write_reg(sgn, NTL_SGN_STATUS_REG, &reg_data);
            
            // end spinlock section
            spin_unlock_irqrestore(&(sgn->lock), flags);
            
            if (copy_to_user((void*)arg, &error, sizeof(unsigned int))) {
                NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "copy to user failed\n");
                return -EFAULT;
            }   
            
            break;
        }
        case NTL_SGN_WAIT_ERROR:
        {
            // start spinlock section
            spin_lock_irqsave(&(sgn->lock), flags);
            
            while (0 == atomic_read(&sgn->error_count))
            {
                NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "no error, waiting\n");
                // end spinlock section
                spin_unlock_irqrestore(&(sgn->lock), flags);
                if (wait_event_interruptible(sgn->wait_queue, (0 != atomic_read(&sgn->error_count))))
                {
                    // tell the fs to handle this
                    return -ERESTARTSYS; 
                }
                // start spinlock section
                spin_lock_irqsave(&(sgn->lock), flags);
            }    
        
            NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "error occured\n");
            
            wait_error = atomic_read(&sgn->error_count);
            
            // end spinlock section
            spin_unlock_irqrestore(&(sgn->lock), flags);
            
            if (copy_to_user((void*)arg, &wait_error, sizeof(unsigned int))) {
                NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "copy to user failed\n");
                return -EFAULT;
            }   
            
            // decrement error count
            atomic_dec(&sgn->error_count);
        
            break;
        }
        case NTL_SGN_SET_GENERATION:
        {
            if (copy_from_user(&generation, (void*)arg, sizeof(struct ntl_sgn_generation))) {
                NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }   
            
            // start spinlock section
            spin_lock_irqsave(&(sgn->lock), flags);
            
            // set start time in nanosecond
            reg_data = generation.start_nanosecond;
            
            // write start time low register
            ntl_sgn_write_reg(sgn, NTL_SGN_STARTTIMEVALUEL_REG, &reg_data);
            
            // set start time in second
            reg_data = generation.start_second;
            
            // write start time high register
            ntl_sgn_write_reg(sgn, NTL_SGN_STARTTIMEVALUEH_REG, &reg_data);
            
            // set pulse width in nanosecond
            reg_data = generation.pulse_nanosecond;
            
            // write pulse width low register
            ntl_sgn_write_reg(sgn, NTL_SGN_PULSEWIDTHVALUEL_REG, &reg_data);
            
            // set pulse width in second
            reg_data = generation.pulse_second;
            
            // write pulse width high register
            ntl_sgn_write_reg(sgn, NTL_SGN_PULSEWIDTHVALUEH_REG, &reg_data);
            
            // set period in nanosecond
            reg_data = generation.period_nanosecond;
            
            // write period low register
            ntl_sgn_write_reg(sgn, NTL_SGN_PERIODVALUEL_REG, &reg_data);
            
            // set period in second
            reg_data = generation.period_second;
            
            // write period high register
            ntl_sgn_write_reg(sgn, NTL_SGN_PERIODVALUEH_REG, &reg_data);
            
            // set repeat count
            reg_data = generation.repeat_count;
            
            // write repeat count register
            ntl_sgn_write_reg(sgn, NTL_SGN_REPEATCOUNT_REG, &reg_data);
            
            // read control reg
            ntl_sgn_read_reg(sgn, NTL_SGN_CONTROL_REG, &reg_data);
            
            // set valid
            reg_data |= NTL_SGN_CONTROL_SIGNAL_VALID_BIT;
            
            // write control register
            ntl_sgn_write_reg(sgn, NTL_SGN_CONTROL_REG, &reg_data);
            
            
            NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "start time: %d s %d ns\n", generation.start_second, generation.start_nanosecond);
            
            NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "pulse width: %d s %d ns\n", generation.pulse_second, generation.pulse_nanosecond);
            
            NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "period: %d s %d ns\n", generation.period_second, generation.period_nanosecond);
            
            if (generation.repeat_count == 0)
            {
                NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "repeat count: infinite\n");
            }
            else
            {
                NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "repeat count: %d\n", generation.repeat_count);
            }
        
            // end spinlock section
            spin_unlock_irqrestore(&(sgn->lock), flags);
            
            break;
        }
        default:
        {
            NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "unknown ioctl\n");
            return -EINVAL;
        }
    }    
    
    return 0;
}

/*****************************************************************************/
/* Irq                                                                       */
/*****************************************************************************/
static irqreturn_t ntl_sgn_irq(int irq, void *dev)
{
    uint32_t reg_data;
    struct ntl_sgn* sgn = (struct ntl_sgn*)dev;
    unsigned long flags;
    
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);
    
    // start spinlock section
    spin_lock_irqsave(&(sgn->lock), flags);

    // read irq register
    ntl_sgn_read_reg(sgn, NTL_SGN_IRQ_REG, &reg_data);
    
    // do we have a pending irq?
    if (0 == (reg_data & NTL_SGN_IRQ_VALID_BIT)) 
    {
        // end spinlock section
        spin_unlock_irqrestore(&(sgn->lock), flags);
        return IRQ_HANDLED;
    }
    
    if (0 == atomic_read(&sgn->open))
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "device not open\n");
        goto clear_irq;
    }
       
    // increment error count count
    atomic_inc(&sgn->error_count);
        
    // wake up any sleeping process
    wake_up_interruptible(&(sgn->wait_queue));
    
clear_irq:        
    // clear potential pending irq
    reg_data = NTL_SGN_IRQ_VALID_BIT;
    
    // write irq register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQ_REG, &reg_data);
    
    // end spinlock section
    spin_unlock_irqrestore(&(sgn->lock), flags);
    
    return IRQ_HANDLED;
}

/*****************************************************************************/
/* Driver probe                                                              */
/*****************************************************************************/
static int ntl_sgn_probe(struct platform_device *pdev)
{
    struct device* dev;
    char dev_filename[80];
    struct ntl_sgn* sgn;
    struct resource* mem;
    struct resource* irq;
    uint32_t reg_data;
    int ret;
    unsigned long flags;

    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);

    NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "************************************************************\n");
    NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "NetTimeLogic SGN\n");
    NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "************************************************************\n");

    // alloc structure 
    sgn = (struct ntl_sgn*)kzalloc(sizeof(struct ntl_sgn), GFP_KERNEL);
    if (sgn == NULL)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "kzalloc failed\n");
        ret = -ENOMEM;
        goto err_sts_alloc_failed;
    }
        
    // create locks
    spin_lock_init(&(sgn->lock));
    
    // init wait queue
    init_waitqueue_head(&(sgn->wait_queue));
    
    // set error count
    atomic_set(&sgn->error_count, 0);
    
    // set closed
    atomic_set(&sgn->open, 0);
    
    // map the private structure to the device
    dev_set_drvdata(&pdev->dev, sgn);
    sgn->pdev = &pdev->dev;
    
    // get reg addr    
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (mem == NULL)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "platform_get_resource mem failed\n");
        ret = -EINVAL;
        goto err_platform_get_resource_mem_failed;
    }  

    printk(KERN_ERR "%s ctrl_mem@0x%016llX - 0x%016llX\n", NTL_SGN_DRIVER_NAME, (unsigned long long)mem->start, (unsigned long long)(mem->start + NTL_SGN_REGSET_SIZE -1));
    
    // save physical address
    sgn->physical_ctrl_base = mem->start;
    
    // request memory region
    if (NULL == request_mem_region(sgn->physical_ctrl_base, NTL_SGN_REGSET_SIZE, NTL_SGN_DRIVER_NAME))
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "request_mem_region failed\n");
        ret = -ENOMEM;
        goto err_request_mem_region_failed;
    }

    // map memory to ioregion
    sgn->ctrl_base = ioremap(sgn->physical_ctrl_base, NTL_SGN_REGSET_SIZE);
    if (sgn->ctrl_base == NULL)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "ioremap failed\n");
        ret = -ENOMEM;
        goto err_ioremap_failed;
    }

    // read version register
    ntl_sgn_read_reg(sgn, NTL_SGN_VERSION_REG, &reg_data);
    
    // check if something expected is here, these are the ones we definitely do not expect, 0 and DEADDEAD
    if ((reg_data == 0x00000000) || (reg_data == 0xDEADDEAD)) 
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "not a ntl signal generator device, version number wrong: 0x%08X\n", reg_data);
        ret = -EINVAL;
        goto err_our_device_failed;
    } 

    // get irq number    
    irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
    if (irq == NULL)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "platform_get_resource irq failed\n");
        ret = -EINVAL;
        goto err_platform_get_resource_irq_failed;
    }  
    
    printk(KERN_ERR "%s irq@0x%016llX\n", NTL_SGN_DRIVER_NAME, (unsigned long long)irq->start);

    // save irq number
    sgn->irq = irq->start;
    
    // request irq
    ret = request_threaded_irq(sgn->irq, NULL, ntl_sgn_irq, IRQF_ONESHOT, NTL_SGN_DRIVER_NAME, (void*)sgn);
    if (ret != 0) {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "request_irq failed\n");
        goto err_request_irq_failed;
    }

    // disable irq
    disable_irq(sgn->irq);
    
    // start spinlock section
    spin_lock_irqsave(&(sgn->lock), flags);

    // disable
    reg_data = 0;
    
    // write control register
    ntl_sgn_write_reg(sgn, NTL_SGN_CONTROL_REG, &reg_data);

    // disable irq
    reg_data = 0;
    
    // write irq mask register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQMASK_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_SGN_IRQ_VALID_BIT;
    
    // write irq register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQ_REG, &reg_data);

    // read version register
    ntl_sgn_read_reg(sgn, NTL_SGN_VERSION_REG, &reg_data);
    
    // end spinlock section
    spin_unlock_irqrestore(&(sgn->lock), flags);
    
    // get device number
    sgn->dev_nr = MKDEV(MAJOR(ntl_sgn_major), MINOR(ntl_sgn_major) + ntl_sgn_minor);
    
    // Creat device file name 
    memset(dev_filename, '\0', sizeof(dev_filename));
    sprintf(dev_filename, "%s%d", NTL_SGN_DRIVER_NAME, ntl_sgn_minor);
    
    // create device
    dev = device_create(ntl_sgn_class, NULL, sgn->dev_nr, NULL, dev_filename);
    if(dev == NULL)
    {
        ret = -ENOMEM;
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "device_create failed\n");
        goto err_device_create_failed;
    }

    // init device
    cdev_init(&(sgn->cdev), &ntl_sgn_fops);
    sgn->cdev.owner = THIS_MODULE;
    sgn->cdev.ops = &ntl_sgn_fops;
    sgn->cdev.dev = sgn->dev_nr;
    
    // register device
    ret = cdev_add(&(sgn->cdev), sgn->dev_nr, 1);
    if (ret != 0)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "cdev_add failed\n");
        goto err_cdev_add_failed;
    }
    
    // increment count for next
    ntl_sgn_minor++;
    
    NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "SGN version: %02x.%02x.%04x registered\n", ((reg_data >> 24) & 0xFF), ((reg_data >> 16) & 0xFF), ((reg_data >> 0) & 0xFFFF));
    
    return 0;
err_cdev_add_failed:
    device_destroy(ntl_sgn_class, sgn->dev_nr);
err_device_create_failed:
    free_irq(sgn->irq, (void*)sgn);
err_request_irq_failed:
err_platform_get_resource_irq_failed:
err_our_device_failed:
    iounmap(sgn->ctrl_base);
err_ioremap_failed:
    release_mem_region(sgn->physical_ctrl_base, NTL_SGN_REGSET_SIZE);
err_request_mem_region_failed:
err_platform_get_resource_mem_failed:
    kfree(sgn);
err_sts_alloc_failed:

    return ret;
}

/*****************************************************************************/
/* Driver remove                                                             */
/*****************************************************************************/
static int ntl_sgn_remove(struct platform_device *pdev)
{
    struct ntl_sgn *sgn = dev_get_drvdata(&pdev->dev);
    uint32_t reg_data;
    unsigned long flags;

    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);

    // disable irqs
    disable_irq(sgn->irq);
    
    // unregister irq
    free_irq(sgn->irq, (void*)sgn);

    // start spinlock section
    spin_lock_irqsave(&(sgn->lock), flags);
    
    // disable 
    reg_data = 0;
    
    // write control register
    ntl_sgn_write_reg(sgn, NTL_SGN_CONTROL_REG, &reg_data);

    // disable irq 
    reg_data = 0;
    
    // write irq mask register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQMASK_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_SGN_IRQ_VALID_BIT;
    
    // write irq register
    ntl_sgn_write_reg(sgn, NTL_SGN_IRQ_REG, &reg_data);

    // set error count
    atomic_set(&sgn->error_count, 0);

    // end spinlock section
    spin_unlock_irqrestore(&(sgn->lock), flags);
    
    // set closed
    atomic_set(&sgn->open, 0);
    
    // unregister device 
    cdev_del(&(sgn->cdev));

    // destroy device
    device_destroy(ntl_sgn_class, sgn->dev_nr);

    // unmap io memory
    iounmap(sgn->ctrl_base);
    
    // release memory
    release_mem_region(sgn->physical_ctrl_base, NTL_SGN_REGSET_SIZE);

    // unmap the private structure to the device
    dev_set_drvdata(&pdev->dev, NULL);

    // free structure 
    kfree(sgn);
    
    return 0;
}

/*****************************************************************************/
/* Driver device mapping                                                     */
/*****************************************************************************/
static struct of_device_id ntl_sgn_of_match[] = {
    { .compatible = "ntl,ntl_sgn", },
    { .compatible = "ntl_sgn", },
    { /* end of list */ }
};

MODULE_DEVICE_TABLE(of, ntl_sgn_of_match);

static struct platform_driver ntl_sgn_of_driver = {
    .probe = ntl_sgn_probe,
    .remove = ntl_sgn_remove,
    .driver = {
        .name = NTL_SGN_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = ntl_sgn_of_match,
    },
};

static int __init ntl_sgn_init(void)
{
    int ret;
    
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);

    // register char device region
    ret = alloc_chrdev_region(&ntl_sgn_major, 0, 256, NTL_SGN_DRIVER_NAME);
    if (ret < 0)
    {
        NTL_SGN_DPRINTK(NTL_SGN_ERROR_LEVEL, "alloc_chrdev_region failed\n");
        return ret;
    }

    // create device class
    ntl_sgn_class = class_create(THIS_MODULE, NTL_SGN_DRIVER_NAME);
    
    return platform_driver_register(&ntl_sgn_of_driver);
}

static void __exit ntl_sgn_exit(void)
{
    NTL_SGN_DPRINTK(NTL_SGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_SGN_DRIVER_NAME, __FUNCTION__);
    
    // destroy device class
    class_destroy(ntl_sgn_class);
    
    // unregister char device region
    unregister_chrdev_region(ntl_sgn_major, 256);

    platform_driver_unregister(&ntl_sgn_of_driver);
}

module_init(ntl_sgn_init);
module_exit(ntl_sgn_exit);

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
MODULE_AUTHOR(NTL_SGN_DRIVER_AUTHOR);
MODULE_DESCRIPTION(NTL_SGN_DRIVER_DESC);
MODULE_LICENSE(NTL_SGN_DRIVER_LICENSE);