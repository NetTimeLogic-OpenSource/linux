// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL FGN driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */
 
#include "ntl_fgn.h"

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/
static int ntl_fgn_open(struct inode* inode, struct file* file);
static int ntl_fgn_close(struct inode* inode, struct file* file);
static long ntl_fgn_ioctl(struct file* file, unsigned int cmd, unsigned long arg); 

static int ntl_fgn_write_reg(struct ntl_fgn* fgn, uint32_t reg_addr, uint32_t* reg_data);
static int ntl_fgn_read_reg(struct ntl_fgn* fgn, uint32_t reg_addr, uint32_t* reg_data);

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
static unsigned int ntl_fgn_major = 0;
static unsigned int ntl_fgn_minor = 0;
static struct class* ntl_fgn_class = NULL;

/*****************************************************************************/
/* Function mapping                                                          */
/*****************************************************************************/
static const struct file_operations ntl_fgn_fops = {
    .owner                                          = THIS_MODULE,
    .open                                           = ntl_fgn_open,
    .release                                        = ntl_fgn_close,
    .unlocked_ioctl                                 = ntl_fgn_ioctl,
};

/*****************************************************************************/
/* Write register                                                            */
/*****************************************************************************/
static int ntl_fgn_write_reg(struct ntl_fgn* fgn, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);

    if(fgn->ctrl_base == NULL)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1;
    }
    
    iowrite32(*reg_data, (fgn->ctrl_base + reg_addr));
    
    return 0;
}

/*****************************************************************************/
/* Read register                                                             */
/*****************************************************************************/
static int ntl_fgn_read_reg(struct ntl_fgn* fgn, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);

    if(fgn->ctrl_base == NULL)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1 ;
    }
    
    *reg_data = ioread32((fgn->ctrl_base + reg_addr));
     
    return 0;
}

/*****************************************************************************/
/* Open device                                                               */
/*****************************************************************************/
static int ntl_fgn_open(struct inode* inode, struct file* file) 
{
    uint32_t reg_data;
    struct ntl_fgn* fgn = container_of(inode->i_cdev, struct ntl_fgn, cdev);
    unsigned long flags;
    
    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);
    
    if (0 != atomic_read(&fgn->open))
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "device already open\n");
        return -EBUSY;
    }

    // start spinlock section
    spin_lock_irqsave(&(fgn->lock), flags);
    
    // increment open count
    atomic_inc(&fgn->open);
    
    // disable 
    reg_data = 0;
    
    // write control register
    ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
        
    // now assign the private pointer
    file->private_data = fgn;
    
    // end spinlock section
    spin_unlock_irqrestore(&(fgn->lock), flags);
        
    return 0;
    
}

/*****************************************************************************/
/* Close device                                                               */
/*****************************************************************************/
static int ntl_fgn_close(struct inode* inode, struct file* file) 
{
    uint32_t reg_data;
    struct ntl_fgn* fgn = container_of(inode->i_cdev, struct ntl_fgn, cdev);
    unsigned long flags;
    
    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);
        
    if (0 == atomic_read(&fgn->open))
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "device not open\n");
        return -EBUSY;
    }
    
    // start spinlock section
    spin_lock_irqsave(&(fgn->lock), flags);
    
    // disable 
    reg_data = 0;
    
    // write control register
    ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);

    // decrement open count
    atomic_dec(&fgn->open);
    
    // end spinlock section
    spin_unlock_irqrestore(&(fgn->lock), flags);
    
    return 0;
}

/*****************************************************************************/
/* Ioctl                                                                */
/*****************************************************************************/
static long ntl_fgn_ioctl(struct file* file, unsigned int cmd, unsigned long arg) 
{
    uint32_t reg_data;
    struct ntl_fgn_generation generation;
    unsigned int polarity = 0;
    unsigned int embedded_pps = 0;
    unsigned int cable_delay = 0;
    struct ntl_fgn* fgn = (struct ntl_fgn*)file->private_data;
    unsigned long flags;
    
    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);
    
    // check if open
    if (0 == atomic_read(&fgn->open))
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "device not open\n");
        return -EBUSY;
    }
        
    if (_IOC_TYPE(cmd) != NTL_FGN_IOCTL_MAGIC)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "not an IOCTL for us\n");
        return -EINVAL;         
    }   
        
    if (_IOC_NR(cmd) > NTL_FGN_IOCTL_MAX_ID)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "IOCTL out of range\n");
        return -EINVAL;         
    }   
        
    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "IOCTL nr: %d\n", _IOC_NR(cmd));
        
    switch (cmd)   
    {
        case NTL_FGN_ENABLE:
        {
            // start spinlock section
            spin_lock_irqsave(&(fgn->lock), flags);
    
            // read control reg
            ntl_fgn_read_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
                
            // enable
            reg_data |= NTL_FGN_CONTROL_ENABLE_BIT;
            
            // write control register
            ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
    
            NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "frequency generator enabled\n");
        
            // end spinlock section
            spin_unlock_irqrestore(&(fgn->lock), flags);
            
            break;
        }
        case NTL_FGN_DISABLE:
        {
            // start spinlock section
            spin_lock_irqsave(&(fgn->lock), flags);
            
            // read control reg
            ntl_fgn_read_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
                
            // disable 
            reg_data &= ~NTL_FGN_CONTROL_ENABLE_BIT;
            
            // write control register
            ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
            
            NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "frequency generator disabled\n");
        
            // end spinlock section
            spin_unlock_irqrestore(&(fgn->lock), flags);
            
            break;
        }
        case NTL_FGN_POLARITY:
        {
            if (copy_from_user(&polarity, (void*)arg, sizeof(unsigned int))) {
                NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }   
            
            // start spinlock section
            spin_lock_irqsave(&(fgn->lock), flags);
            
            if (polarity == 0)
            {
                NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "setting polarity to falling edge\n");
                
                // falling edge
                reg_data = 0;
                
                // write polarity register
                ntl_fgn_write_reg(fgn, NTL_FGN_POLARITY_REG, &reg_data);
            }
            else
            {
                NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "setting polarity to rising edge\n");
                
                // rising edge
                reg_data = NTL_FGN_POLARITY_BIT;
                
                // write polarity register
                ntl_fgn_write_reg(fgn, NTL_FGN_POLARITY_REG, &reg_data);
            }
            
            // end spinlock section
            spin_unlock_irqrestore(&(fgn->lock), flags);
            
            break;
        }
        case NTL_FGN_CABLE_DELAY:
        {
            if (copy_from_user(&cable_delay, (void*)arg, sizeof(unsigned int))) {
                NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }   
            
            // start spinlock section
            spin_lock_irqsave(&(fgn->lock), flags);
            
            NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "setting cable delay to %d ns\n", cable_delay);
                
            // set cable delay in nanosecond
            reg_data = cable_delay;
            
            // write cable delay register
            ntl_fgn_write_reg(fgn, NTL_FGN_CABLEDELAY_REG, &reg_data);
            
            // end spinlock section
            spin_unlock_irqrestore(&(fgn->lock), flags);
            
            break;
        }
        case NTL_FGN_EMBEDDED_PPS:
        {
            if (copy_from_user(&embedded_pps, (void*)arg, sizeof(unsigned int))) {
                NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }   
            
            // start spinlock section
            spin_lock_irqsave(&(fgn->lock), flags);
            
            if (embedded_pps == 0)
            {
                NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "Disable PPS embedding\n");
                
                // read control reg
                ntl_fgn_read_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
                
                // clear embedded PPS
                reg_data &= ~NTL_FGN_CONTROL_EMBEDDED_PPS_BIT;
                
                // write control register
                ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
            }
            else
            {
                NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "Enable PPS embedding\n");
                
                // read control reg
                ntl_fgn_read_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
                
                // set embedded PPS
                reg_data |= NTL_FGN_CONTROL_EMBEDDED_PPS_BIT;
                
                // write control register
                ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
                
            }
                        
            // end spinlock section
            spin_unlock_irqrestore(&(fgn->lock), flags);
            
            break;
        }
        case NTL_FGN_SET_FREQUENCY:
        {
            if (copy_from_user(&generation, (void*)arg, sizeof(struct ntl_fgn_generation))) {
                NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }   
            
            // start spinlock section
            spin_lock_irqsave(&(fgn->lock), flags);
            
            // set frequency
            reg_data = generation.frequency;
            
            // write frequency register
            ntl_fgn_write_reg(fgn, NTL_FGN_FREQUENCY_REG, &reg_data);
            
            // read control reg
            ntl_fgn_read_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
            
            // set valid
            reg_data |= NTL_FGN_CONTROL_FREQUENCY_VALID_BIT;
            
            // write control register
            ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);
            
            NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "frequency: %d hz\n", generation.frequency);
                                
            // end spinlock section
            spin_unlock_irqrestore(&(fgn->lock), flags);
            
            break;
        }
        default:
        {
            NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "unknown ioctl\n");
            return -EINVAL;
        }
    }    
    
    return 0;
}

/*****************************************************************************/
/* Driver probe                                                              */
/*****************************************************************************/
static int ntl_fgn_probe(struct platform_device *pdev)
{
    struct device* dev;
    char dev_filename[80];
    struct ntl_fgn* fgn;
    struct resource* mem;
    uint32_t reg_data;
    int ret;
    unsigned long flags;

    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);

    NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "************************************************************\n");
    NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "NetTimeLogic FGN\n");
    NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "************************************************************\n");

    // alloc structure 
    fgn = (struct ntl_fgn*)kzalloc(sizeof(struct ntl_fgn), GFP_KERNEL);
    if (fgn == NULL)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "kzalloc failed\n");
        ret = -ENOMEM;
        goto err_sts_alloc_failed;
    }
        
    // create locks
    spin_lock_init(&(fgn->lock));
    
    // set closed
    atomic_set(&fgn->open, 0);
    
    // map the private structure to the device
    dev_set_drvdata(&pdev->dev, fgn);
    fgn->pdev = &pdev->dev;
    
    // get reg addr    
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (mem == NULL)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "platform_get_resource mem failed\n");
        ret = -EINVAL;
        goto err_platform_get_resource_mem_failed;
    }  

    printk(KERN_ERR "%s ctrl_mem@0x%016llX - 0x%016llX\n", NTL_FGN_DRIVER_NAME, (unsigned long long)mem->start, (unsigned long long)(mem->start + NTL_FGN_REGSET_SIZE -1));
    
    // save physical address
    fgn->physical_ctrl_base = mem->start;
    
    // request memory region
    if (NULL == request_mem_region(fgn->physical_ctrl_base, NTL_FGN_REGSET_SIZE, NTL_FGN_DRIVER_NAME))
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "request_mem_region failed\n");
        ret = -ENOMEM;
        goto err_request_mem_region_failed;
    }

    // map memory to ioregion
    fgn->ctrl_base = ioremap(fgn->physical_ctrl_base, NTL_FGN_REGSET_SIZE);
    if (fgn->ctrl_base == NULL)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "ioremap failed\n");
        ret = -ENOMEM;
        goto err_ioremap_failed;
    }

    // read version register
    ntl_fgn_read_reg(fgn, NTL_FGN_VERSION_REG, &reg_data);
    
    // check if something expected is here, these are the ones we definitely do not expect, 0 and DEADDEAD
    if ((reg_data == 0x00000000) || (reg_data == 0xDEADDEAD)) 
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "not a ntl frequency generator device, version number wrong: 0x%08X\n", reg_data);
        ret = -EINVAL;
        goto err_our_device_failed;
    } 

    // start spinlock section
    spin_lock_irqsave(&(fgn->lock), flags);

    // disable
    reg_data = 0;
    
    // write control register
    ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);

    // read version register
    ntl_fgn_read_reg(fgn, NTL_FGN_VERSION_REG, &reg_data);
    
    // end spinlock section
    spin_unlock_irqrestore(&(fgn->lock), flags);
    
    // get device number
    fgn->dev_nr = MKDEV(MAJOR(ntl_fgn_major), MINOR(ntl_fgn_major) + ntl_fgn_minor);
    
    // Creat device file name 
    memset(dev_filename, '\0', sizeof(dev_filename));
    sprintf(dev_filename, "%s%d", NTL_FGN_DRIVER_NAME, ntl_fgn_minor);
    
    // create device
    dev = device_create(ntl_fgn_class, NULL, fgn->dev_nr, NULL, dev_filename);
    if(dev == NULL)
    {
        ret = -ENOMEM;
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "device_create failed\n");
        goto err_device_create_failed;
    }

    // init device
    cdev_init(&(fgn->cdev), &ntl_fgn_fops);
    fgn->cdev.owner = THIS_MODULE;
    fgn->cdev.ops = &ntl_fgn_fops;
    fgn->cdev.dev = fgn->dev_nr;
    
    // register device
    ret = cdev_add(&(fgn->cdev), fgn->dev_nr, 1);
    if (ret != 0)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "cdev_add failed\n");
        goto err_cdev_add_failed;
    }
    
    // increment count for next
    ntl_fgn_minor++;
    
    NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "FGN version: %02x.%02x.%04x registered\n", ((reg_data >> 24) & 0xFF), ((reg_data >> 16) & 0xFF), ((reg_data >> 0) & 0xFFFF));
    
    return 0;
err_cdev_add_failed:
    device_destroy(ntl_fgn_class, fgn->dev_nr);
err_device_create_failed:
err_our_device_failed:
    iounmap(fgn->ctrl_base);
err_ioremap_failed:
    release_mem_region(fgn->physical_ctrl_base, NTL_FGN_REGSET_SIZE);
err_request_mem_region_failed:
err_platform_get_resource_mem_failed:
    kfree(fgn);
err_sts_alloc_failed:

    return ret;
}

/*****************************************************************************/
/* Driver remove                                                             */
/*****************************************************************************/
static int ntl_fgn_remove(struct platform_device *pdev)
{
    struct ntl_fgn *fgn = dev_get_drvdata(&pdev->dev);
    uint32_t reg_data;
    unsigned long flags;

    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);

    // start spinlock section
    spin_lock_irqsave(&(fgn->lock), flags);
    
    // disable 
    reg_data = 0;
    
    // write control register
    ntl_fgn_write_reg(fgn, NTL_FGN_CONTROL_REG, &reg_data);

    // end spinlock section
    spin_unlock_irqrestore(&(fgn->lock), flags);
    
    // set closed
    atomic_set(&fgn->open, 0);
    
    // unregister device 
    cdev_del(&(fgn->cdev));

    // destroy device
    device_destroy(ntl_fgn_class, fgn->dev_nr);

    // unmap io memory
    iounmap(fgn->ctrl_base);
    
    // release memory
    release_mem_region(fgn->physical_ctrl_base, NTL_FGN_REGSET_SIZE);

    // unmap the private structure to the device
    dev_set_drvdata(&pdev->dev, NULL);

    // free structure 
    kfree(fgn);
    
    return 0;
}

/*****************************************************************************/
/* Driver device mapping                                                     */
/*****************************************************************************/
static struct of_device_id ntl_fgn_of_match[] = {
    { .compatible = "ntl,ntl_fgn", },
    { .compatible = "ntl_fgn", },
    { /* end of list */ }
};

MODULE_DEVICE_TABLE(of, ntl_fgn_of_match);

static struct platform_driver ntl_fgn_of_driver = {
    .probe = ntl_fgn_probe,
    .remove = ntl_fgn_remove,
    .driver = {
        .name = NTL_FGN_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = ntl_fgn_of_match,
    },
};

static int __init ntl_fgn_init(void)
{
    int ret;
    
    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);

    // register char device region
    ret = alloc_chrdev_region(&ntl_fgn_major, 0, 256, NTL_FGN_DRIVER_NAME);
    if (ret < 0)
    {
        NTL_FGN_DPRINTK(NTL_FGN_ERROR_LEVEL, "alloc_chrdev_region failed\n");
        return ret;
    }

    // create device class
    ntl_fgn_class = class_create(THIS_MODULE, NTL_FGN_DRIVER_NAME);
    
    return platform_driver_register(&ntl_fgn_of_driver);
}

static void __exit ntl_fgn_exit(void)
{
    NTL_FGN_DPRINTK(NTL_FGN_DEBUG_LEVEL, "%s:%s entering function\n", NTL_FGN_DRIVER_NAME, __FUNCTION__);
    
    // destroy device class
    class_destroy(ntl_fgn_class);
    
    // unregister char device region
    unregister_chrdev_region(ntl_fgn_major, 256);

    platform_driver_unregister(&ntl_fgn_of_driver);
}

module_init(ntl_fgn_init);
module_exit(ntl_fgn_exit);

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
MODULE_AUTHOR(NTL_FGN_DRIVER_AUTHOR);
MODULE_DESCRIPTION(NTL_FGN_DRIVER_DESC);
MODULE_LICENSE(NTL_FGN_DRIVER_LICENSE);