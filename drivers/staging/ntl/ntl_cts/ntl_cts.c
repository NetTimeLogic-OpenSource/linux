// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL CTS driver
 *
 * Copyright (C) 2026 NetTimeLogic GmbH
 */

#include "ntl_cts.h"

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/
static int ntl_cts_open(struct inode* inode, struct file* file);
static int ntl_cts_close(struct inode* inode, struct file* file);
static long ntl_cts_ioctl(struct file* file, unsigned int cmd, unsigned long arg);

static int ntl_cts_write_reg(struct ntl_cts* cts, uint32_t reg_addr, uint32_t* reg_data);
static int ntl_cts_read_reg(struct ntl_cts* cts, uint32_t reg_addr, uint32_t* reg_data);

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
static unsigned int ntl_cts_major = 0;
static unsigned int ntl_cts_minor = 0;
static struct class* ntl_cts_class = NULL;

/*****************************************************************************/
/* Function mapping                                                          */
/*****************************************************************************/
static const struct file_operations ntl_cts_fops = {
    .owner                                          = THIS_MODULE,
    .open                                           = ntl_cts_open,
    .release                                        = ntl_cts_close,
    .unlocked_ioctl                                 = ntl_cts_ioctl,
};

/*****************************************************************************/
/* Write register                                                            */
/*****************************************************************************/
static int ntl_cts_write_reg(struct ntl_cts* cts, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    if(cts->ctrl_base == NULL)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1;
    }

    iowrite32(*reg_data, (cts->ctrl_base + reg_addr));

    return 0;
}

/*****************************************************************************/
/* Read register                                                             */
/*****************************************************************************/
static int ntl_cts_read_reg(struct ntl_cts* cts, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    if(cts->ctrl_base == NULL)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1 ;
    }

    *reg_data = ioread32((cts->ctrl_base + reg_addr));

    return 0;
}

/*****************************************************************************/
/* Open device                                                               */
/*****************************************************************************/
static int ntl_cts_open(struct inode* inode, struct file* file)
{
    uint32_t reg_data;
    struct ntl_cts* cts = container_of(inode->i_cdev, struct ntl_cts, cdev);
	unsigned long flags;

    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    if (0 != atomic_read(&cts->open))
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "device already open\n");
        return -EBUSY;
    }

    // start spinlock section
    spin_lock_irqsave(&(cts->lock), flags);

    // increment open count
    atomic_inc(&cts->open);

    // enable
    reg_data = NTL_CTS_CONTROL_ENABLE_BIT;

    // write control register
    ntl_cts_write_reg(cts, NTL_CTS_CONTROL_REG, &reg_data);

    // now assign the private pointer
    file->private_data = cts;

    // end spinlock section
    spin_unlock_irqrestore(&(cts->lock), flags);

    return 0;

}

/*****************************************************************************/
/* Close device                                                               */
/*****************************************************************************/
static int ntl_cts_close(struct inode* inode, struct file* file)
{
    uint32_t reg_data;
    struct ntl_cts* cts = container_of(inode->i_cdev, struct ntl_cts, cdev);
	unsigned long flags;

    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&cts->open))
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "device not open\n");
        return -EBUSY;
    }

    // start spinlock section
    spin_lock_irqsave(&(cts->lock), flags);

    // disable
    reg_data = 0;

    // write control register
    ntl_cts_write_reg(cts, NTL_CTS_CONTROL_REG, &reg_data);

    // decrement open count
    atomic_dec(&cts->open);

    // end spinlock section
    spin_unlock_irqrestore(&(cts->lock), flags);

    return 0;
}

/*****************************************************************************/
/* Ioctl                                                                */
/*****************************************************************************/
static long ntl_cts_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    int i;
    unsigned int nr_of_sources = 0;
    uint32_t reg_data;
    struct ntl_cts_timestamp timestamp;
    struct ntl_cts* cts = (struct ntl_cts*)file->private_data;
	unsigned long flags;


    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    // check if open
    if (0 == atomic_read(&cts->open))
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "device not open\n");
        return -EBUSY;
    }

    if (_IOC_TYPE(cmd) != NTL_CTS_IOCTL_MAGIC)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "not an IOCTL for us\n");
        return -EINVAL;
    }

    if (_IOC_NR(cmd) > NTL_CTS_IOCTL_MAX_ID)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "IOCTL out of range\n");
        return -EINVAL;
    }

    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "IOCTL nr: %d\n", _IOC_NR(cmd));

    switch (cmd)
    {
        case NTL_CTS_TRIGGER_TIMESTAMP:
        {
            // start spinlock section
            spin_lock_irqsave(&(cts->lock), flags);

            // read status
            ntl_cts_read_reg(cts, NTL_CTS_CONTROL_REG, &reg_data);

            // Trigger
            reg_data = NTL_CTS_CONTROL_TRIGGER_BIT;

            // write control register
            ntl_cts_write_reg(cts, NTL_CTS_CONTROL_REG, &reg_data);
            
            // check if triggered
            for (i = 0; i < 10; i++)
            {
                // read status
                ntl_cts_read_reg(cts, NTL_CTS_CONTROL_REG, &reg_data);
                
                if ((reg_data & NTL_CTS_CONTROL_TRIGGERED_BIT) != 0)
                {
                    break;
                }
            }
            
            // end spinlock section
            spin_unlock_irqrestore(&(cts->lock), flags);

            if ((reg_data & NTL_CTS_CONTROL_TRIGGERED_BIT) != 0)
            {
                NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "cross timestamping triggered\n");
            }
            else
            {
                NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "could to trigger cross timestamping\n");
                return -EFAULT;
            }

            break;
        }
        case NTL_CTS_GET_TIMESTAMP:
        {
            if (copy_from_user(&timestamp, (void*)arg, sizeof(struct ntl_cts_timestamp))) {
                NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }

            if (timestamp.source_index >= cts->nr_of_sources)
            {
                NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "requested source_index: %d is out of range 0 - %d\n", timestamp.source_index, cts->nr_of_sources);
                return -EFAULT;
            }
            
            // start spinlock section
            spin_lock_irqsave(&(cts->lock), flags);
            
            // read seconds
            ntl_cts_read_reg(cts, (NTL_CTS_TIMEVALUEH_REG + (timestamp.source_index * 16)), &reg_data);
            timestamp.second = reg_data;
            
            // read nanoseconds
            ntl_cts_read_reg(cts, (NTL_CTS_TIMEVALUEL_REG + (timestamp.source_index * 16)), &reg_data);
            timestamp.nanosecond = reg_data;
                
            // read status
            ntl_cts_read_reg(cts, (NTL_CTS_TIMESTATUS_REG + (timestamp.source_index * 16)), &reg_data);
            timestamp.flags = reg_data;
                
            // end spinlock section
            spin_unlock_irqrestore(&(cts->lock), flags);

            if (copy_to_user((void*)arg, &timestamp, sizeof(struct ntl_cts_timestamp))) {
                NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "copy to user failed\n");
                return -EFAULT;
            }

            break;
        }
        case NTL_CTS_GET_NR_OF_SOURCES:
        {
            nr_of_sources = cts->nr_of_sources;

            if (copy_to_user((void*)arg, &nr_of_sources, sizeof(unsigned int))) {
                NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "copy to user failed\n");
                return -EFAULT;
            }

            break;
        }
        default:
        {
            NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "unknown ioctl\n");
            return -EINVAL;
        }
    }

    return 0;
}

/*****************************************************************************/
/* Driver probe                                                              */
/*****************************************************************************/
static int ntl_cts_probe(struct platform_device *pdev)
{
    struct device* dev;
    char dev_filename[80];
    struct ntl_cts* cts;
    struct resource* mem;
    uint32_t reg_data;
    int ret;
	unsigned long flags;

    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "************************************************************\n");
    NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "NetTimeLogic CTS\n");
    NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "************************************************************\n");

    // alloc structure
    cts = (struct ntl_cts*)kzalloc(sizeof(struct ntl_cts), GFP_KERNEL);
    if (cts == NULL)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "kzalloc failed\n");
        ret = -ENOMEM;
        goto err_cts_alloc_failed;
    }

    // create locks
    spin_lock_init(&(cts->lock));

    // set closed
    atomic_set(&cts->open, 0);

    // map the private structure to the device
    dev_set_drvdata(&pdev->dev, cts);
    cts->pdev = &pdev->dev;

    // get reg addr
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (mem == NULL)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "platform_get_resource mem failed\n");
        ret = -EINVAL;
        goto err_platform_get_resource_mem_failed;
    }

    printk(KERN_ERR "%s ctrl_mem@0x%016llX - 0x%016llX\n", NTL_CTS_DRIVER_NAME, (unsigned long long)mem->start, (unsigned long long)(mem->start + NTL_CTS_REGSET_SIZE -1));

    // save physical address
    cts->physical_ctrl_base = mem->start;

    // request memory region
    if (NULL == request_mem_region(cts->physical_ctrl_base, NTL_CTS_REGSET_SIZE, NTL_CTS_DRIVER_NAME))
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "request_mem_region failed\n");
        ret = -ENOMEM;
        goto err_request_mem_region_failed;
    }

    // map memory to ioregion
    cts->ctrl_base = ioremap(cts->physical_ctrl_base, NTL_CTS_REGSET_SIZE);
    if (cts->ctrl_base == NULL)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "ioremap failed\n");
        ret = -ENOMEM;
        goto err_ioremap_failed;
    }

    // read version register
    ntl_cts_read_reg(cts, NTL_CTS_VERSION_REG, &reg_data);

    // check if something expected is here, these are the ones we definitely do not expect, 0 and DEADDEAD
    if ((reg_data == 0x00000000) || (reg_data == 0xDEADDEAD)) 
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "not a ntl cross timestamp device, version number wrong: 0x%08X\n", reg_data);
        ret = -EINVAL;
        goto err_our_device_failed;
    }
    else
    {
        // read status register which containts the number of sources
        ntl_cts_read_reg(cts, NTL_CTS_STATUS_REG, &reg_data);
        // the number of sources must be in this range (fixed by generic range)
        if (((reg_data & 0x1FF) < 2) || ((reg_data & 0x1FF) > 256))
        {
            NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "not a ntl cross timestamp device, number of sources wrong\n");
            ret = -EINVAL;
            goto err_our_device_failed;
        }
        else
        {
            // save nr of sources
            cts->nr_of_sources = reg_data;
        }            
    }
    
    // start spinlock section
    spin_lock_irqsave(&(cts->lock), flags);

    // disable
    reg_data = 0;

    // write control register
    ntl_cts_write_reg(cts, NTL_CTS_CONTROL_REG, &reg_data);

    // read version register
    ntl_cts_read_reg(cts, NTL_CTS_VERSION_REG, &reg_data);

    // end spinlock section
    spin_unlock_irqrestore(&(cts->lock), flags);

    // get device number
    cts->dev_nr = MKDEV(MAJOR(ntl_cts_major), MINOR(ntl_cts_major) + ntl_cts_minor);
    
    // Creat device file name 
    memset(dev_filename, '\0', sizeof(dev_filename));
    sprintf(dev_filename, "%s%d", NTL_CTS_DRIVER_NAME, ntl_cts_minor);

    // create device
    dev = device_create(ntl_cts_class, NULL, cts->dev_nr, NULL, dev_filename);
    if(dev == NULL)
    {
        ret = -ENOMEM;
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "device_create failed\n");
        goto err_device_create_failed;
    }

    // init device
    cdev_init(&(cts->cdev), &ntl_cts_fops);
    cts->cdev.owner = THIS_MODULE;
    cts->cdev.ops = &ntl_cts_fops;
    cts->cdev.dev = cts->dev_nr;

    // register device
    ret = cdev_add(&(cts->cdev), cts->dev_nr, 1);
    if (ret != 0)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "cdev_add failed\n");
        goto err_cdev_add_failed;
    }

    // increment count for next
    ntl_cts_minor++;

    NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "CTS version: %02x.%02x.%04x registered\n", ((reg_data >> 24) & 0xFF), ((reg_data >> 16) & 0xFF), ((reg_data >> 0) & 0xFFFF));

    return 0;
err_cdev_add_failed:
    device_destroy(ntl_cts_class, cts->dev_nr);
err_device_create_failed:
err_our_device_failed:
    iounmap(cts->ctrl_base);
err_ioremap_failed:
    release_mem_region(cts->physical_ctrl_base, NTL_CTS_REGSET_SIZE);
err_request_mem_region_failed:
err_platform_get_resource_mem_failed:
    kfree(cts);
err_cts_alloc_failed:

    return ret;
}

/*****************************************************************************/
/* Driver remove                                                             */
/*****************************************************************************/
static int ntl_cts_remove(struct platform_device *pdev)
{
    struct ntl_cts *cts = dev_get_drvdata(&pdev->dev);
    uint32_t reg_data;
	unsigned long flags;

    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    // start spinlock section
    spin_lock_irqsave(&(cts->lock), flags);

    // disable
    reg_data = 0;

    // write control register
    ntl_cts_write_reg(cts, NTL_CTS_CONTROL_REG, &reg_data);

    // end spinlock section
    spin_unlock_irqrestore(&(cts->lock), flags);

    // set closed
    atomic_set(&cts->open, 0);

    // unregister device
    cdev_del(&(cts->cdev));

    // destroy device
    device_destroy(ntl_cts_class, cts->dev_nr);

    // unmap io memory
    iounmap(cts->ctrl_base);

    // release memory
    release_mem_region(cts->physical_ctrl_base, NTL_CTS_REGSET_SIZE);

    // unmap the private structure to the device
    dev_set_drvdata(&pdev->dev, NULL);

    // free structure
    kfree(cts);

    return 0;
}

/*****************************************************************************/
/* Driver device mapping                                                     */
/*****************************************************************************/
static struct of_device_id ntl_cts_of_match[] = {
    { .compatible = "ntl,ntl_cts", },
    { .compatible = "ntl_cts", },
    { /* end of list */ }
};

MODULE_DEVICE_TABLE(of, ntl_cts_of_match);

static struct platform_driver ntl_cts_of_driver = {
    .probe = ntl_cts_probe,
    .remove = ntl_cts_remove,
    .driver = {
        .name = NTL_CTS_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = ntl_cts_of_match,
    },
};

static int __init ntl_cts_init(void)
{
    int ret;

    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    // register char device region
    ret = alloc_chrdev_region(&ntl_cts_major, 0, 256, NTL_CTS_DRIVER_NAME);
    if (ret < 0)
    {
        NTL_CTS_DPRINTK(NTL_CTS_ERROR_LEVEL, "alloc_chrdev_region failed\n");
        return ret;
    }

    // create device class
    ntl_cts_class = class_create(THIS_MODULE, NTL_CTS_DRIVER_NAME);

    return platform_driver_register(&ntl_cts_of_driver);
}

static void __exit ntl_cts_exit(void)
{
    NTL_CTS_DPRINTK(NTL_CTS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_CTS_DRIVER_NAME, __FUNCTION__);

    // destroy device class
    class_destroy(ntl_cts_class);

    // unregister char device region
    unregister_chrdev_region(ntl_cts_major, 256);

    platform_driver_unregister(&ntl_cts_of_driver);
}

module_init(ntl_cts_init);
module_exit(ntl_cts_exit);

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
MODULE_AUTHOR(NTL_CTS_DRIVER_AUTHOR);
MODULE_DESCRIPTION(NTL_CTS_DRIVER_DESC);
MODULE_LICENSE(NTL_CTS_DRIVER_LICENSE);
