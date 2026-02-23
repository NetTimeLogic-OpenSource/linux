// SPDX-License-Identifier: GPL-2.0-only
/*
 * NTL STS driver
 *
 * Copyright (C) 2023 NetTimeLogic GmbH
 */

#include "ntl_sts.h"

/*****************************************************************************/
/* Function declarations                                                     */
/*****************************************************************************/
static int ntl_sts_open(struct inode* inode, struct file* file);
static int ntl_sts_close(struct inode* inode, struct file* file);
static long ntl_sts_ioctl(struct file* file, unsigned int cmd, unsigned long arg);
static irqreturn_t ntl_sts_irq(int irq, void *dev);

static int ntl_sts_write_reg(struct ntl_sts* sts, uint32_t reg_addr, uint32_t* reg_data);
static int ntl_sts_read_reg(struct ntl_sts* sts, uint32_t reg_addr, uint32_t* reg_data);

/*****************************************************************************/
/* Global Variables                                                          */
/*****************************************************************************/
static unsigned int ntl_sts_major = 0;
static unsigned int ntl_sts_minor = 0;
static struct class* ntl_sts_class = NULL;

/*****************************************************************************/
/* Function mapping                                                          */
/*****************************************************************************/
static const struct file_operations ntl_sts_fops = {
    .owner                                          = THIS_MODULE,
    .open                                           = ntl_sts_open,
    .release                                        = ntl_sts_close,
    .unlocked_ioctl                                 = ntl_sts_ioctl,
};

/*****************************************************************************/
/* Write register                                                            */
/*****************************************************************************/
static int ntl_sts_write_reg(struct ntl_sts* sts, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    if(sts->ctrl_base == NULL)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1;
    }

    iowrite32(*reg_data, (sts->ctrl_base + reg_addr));

    return 0;
}

/*****************************************************************************/
/* Read register                                                             */
/*****************************************************************************/
static int ntl_sts_read_reg(struct ntl_sts* sts, uint32_t reg_addr, uint32_t* reg_data)
{
    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    if(sts->ctrl_base == NULL)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "ctrl_base NULL\n");
        return -1 ;
    }

    *reg_data = ioread32((sts->ctrl_base + reg_addr));

    return 0;
}

/*****************************************************************************/
/* Open device                                                               */
/*****************************************************************************/
static int ntl_sts_open(struct inode* inode, struct file* file)
{
    uint32_t reg_data;
    struct ntl_sts_timestamp_list* timestamp;
    struct ntl_sts* sts = container_of(inode->i_cdev, struct ntl_sts, cdev);
	 unsigned long flags;

    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    if (0 != atomic_read(&sts->open))
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "device already open\n");
        return -EBUSY;
    }

    // start spinlock section
    spin_lock_irqsave(&(sts->lock), flags);

    // increment open count
    atomic_inc(&sts->open);

    // disable
    reg_data = 0;

    // write control register
    ntl_sts_write_reg(sts, NTL_STS_CONTROL_REG, &reg_data);

    // empty all pending timestamps
    while (0 == list_empty(&(sts->data_queue)))
    {
        timestamp = list_entry(sts->data_queue.next, struct ntl_sts_timestamp_list, list);
        list_del(sts->data_queue.next);
        kfree(timestamp);
    }
    sts->queue_length = 0;

    // enable irq
    reg_data = NTL_STS_IRQMASK_VALID_BIT;

    // write irq mask register
    ntl_sts_write_reg(sts, NTL_STS_IRQMASK_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_STS_IRQ_VALID_BIT;

    // write irq register
    ntl_sts_write_reg(sts, NTL_STS_IRQ_REG, &reg_data);

    // now assign the private pointer
    file->private_data = sts;

    // end spinlock section
    spin_unlock_irqrestore(&(sts->lock), flags);

    // enable irqs
    enable_irq(sts->irq);

    return 0;

}

/*****************************************************************************/
/* Close device                                                               */
/*****************************************************************************/
static int ntl_sts_close(struct inode* inode, struct file* file)
{
    uint32_t reg_data;
    struct ntl_sts_timestamp_list* timestamp;
    struct ntl_sts* sts = container_of(inode->i_cdev, struct ntl_sts, cdev);
	unsigned long flags;

    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    if (0 == atomic_read(&sts->open))
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "device not open\n");
        return -EBUSY;
    }

    // disable irqs
    disable_irq(sts->irq);

    // start spinlock section
    spin_lock_irqsave(&(sts->lock), flags);

    // disable
    reg_data = 0;

    // write control register
    ntl_sts_write_reg(sts, NTL_STS_CONTROL_REG, &reg_data);

    // disable irq
    reg_data = 0;

    // write irq mask register
    ntl_sts_write_reg(sts, NTL_STS_IRQMASK_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_STS_IRQ_VALID_BIT;

    // write irq register
    ntl_sts_write_reg(sts, NTL_STS_IRQ_REG, &reg_data);

    // decrement open count
    atomic_dec(&sts->open);

    // empty all pending timestamps
    while (0 == list_empty(&(sts->data_queue)))
    {
        timestamp = list_entry(sts->data_queue.next, struct ntl_sts_timestamp_list, list);
        list_del(sts->data_queue.next);
        kfree(timestamp);
    }
    sts->queue_length = 0;

    // end spinlock section
    spin_unlock_irqrestore(&(sts->lock), flags);

    return 0;
}

/*****************************************************************************/
/* Ioctl                                                                */
/*****************************************************************************/
static long ntl_sts_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    uint32_t reg_data;
    struct ntl_sts_timestamp_list* timestamp;
    unsigned int polarity = 0;
    unsigned int cable_delay = 0;
    unsigned int timestamp_dropped = 0;
    unsigned int timestamp_ready = 0;
    unsigned int timestamp_data_length = 0;
    unsigned int event_count = 0;
    int queue_max_length = 0;
    struct ntl_sts* sts = (struct ntl_sts*)file->private_data;
	unsigned long flags;


    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    // check if open
    if (0 == atomic_read(&sts->open))
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "device not open\n");
        return -EBUSY;
    }

    if (_IOC_TYPE(cmd) != NTL_STS_IOCTL_MAGIC)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "not an IOCTL for us\n");
        return -EINVAL;
    }

    if (_IOC_NR(cmd) > NTL_STS_IOCTL_MAX_ID)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "IOCTL out of range\n");
        return -EINVAL;
    }

    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "IOCTL nr: %d\n", _IOC_NR(cmd));

    switch (cmd)
    {
        case NTL_STS_ENABLE:
        {
            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            // empty all pending timestamps
            while (0 == list_empty(&(sts->data_queue)))
            {
                timestamp = list_entry(sts->data_queue.next, struct ntl_sts_timestamp_list, list);
                list_del(sts->data_queue.next);
                kfree(timestamp);
            }
            sts->queue_length = 0;

            // enable irq
            reg_data = NTL_STS_IRQMASK_VALID_BIT;

            // write irq mask register
            ntl_sts_write_reg(sts, NTL_STS_IRQMASK_REG, &reg_data);

            // clear potential pending irq
            reg_data = NTL_STS_IRQ_VALID_BIT;

            // write irq register
            ntl_sts_write_reg(sts, NTL_STS_IRQ_REG, &reg_data);

            // enable
            reg_data = NTL_STS_CONTROL_ENABLE_BIT;

            // write control register
            ntl_sts_write_reg(sts, NTL_STS_CONTROL_REG, &reg_data);

            NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "timestamping enabled\n");

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            break;
        }
        case NTL_STS_DISABLE:
        {
            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            // disable
            reg_data = 0;

            // write control register
            ntl_sts_write_reg(sts, NTL_STS_CONTROL_REG, &reg_data);

            // disable irq
            reg_data = 0;

            // write irq mask register
            ntl_sts_write_reg(sts, NTL_STS_IRQMASK_REG, &reg_data);

            // clear potential pending irq
            reg_data = NTL_STS_IRQ_VALID_BIT;

            // write irq register
            ntl_sts_write_reg(sts, NTL_STS_IRQ_REG, &reg_data);

            // empty all pending timestamps
            while (0 == list_empty(&(sts->data_queue)))
            {
                timestamp = list_entry(sts->data_queue.next, struct ntl_sts_timestamp_list, list);
                list_del(sts->data_queue.next);
                kfree(timestamp);
            }
            sts->queue_length = 0;

            NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "timestamping disabled\n");

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            break;
        }
        case NTL_STS_POLARITY:
        {
            if (copy_from_user(&polarity, (void*)arg, sizeof(unsigned int))) {
                NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }

            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            if (polarity == 0)
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "setting polarity to falling edge\n");

                // falling edge
                reg_data = 0;

                // write polarity register
                ntl_sts_write_reg(sts, NTL_STS_POLARITY_REG, &reg_data);
            }
            else
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "setting polarity to rising edge\n");

                // rising edge
                reg_data = NTL_STS_POLARITY_BIT;

                // write polarity register
                ntl_sts_write_reg(sts, NTL_STS_POLARITY_REG, &reg_data);
            }

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            break;
        }
        case NTL_STS_CABLE_DELAY:
        {
            if (copy_from_user(&cable_delay, (void*)arg, sizeof(unsigned int))) {
                NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }

            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "setting cable delay to %d ns\n", cable_delay);

            // set cable delay in nanosecond
            reg_data = cable_delay;

            // write cable delay register
            ntl_sts_write_reg(sts, NTL_STS_CABLEDELAY_REG, &reg_data);

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            break;
        }
        case NTL_STS_TIMESTAMPS_DROPPED:
        {
            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            // read status
            ntl_sts_read_reg(sts, NTL_STS_STATUS_REG, &reg_data);

            // drop bit set
            if (0 == (reg_data & NTL_STS_STATUS_DROP_BIT))
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "no timestamps dropped\n");
                timestamp_dropped = 0;
            }
            else
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "some timestamps dropped\n");
                timestamp_dropped = 1;
            }

            // clear dropped bit
            ntl_sts_write_reg(sts, NTL_STS_STATUS_REG, &reg_data);

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            if (copy_to_user((void*)arg, &timestamp_dropped, sizeof(unsigned int))) {
                NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "copy to user failed\n");
                return -EFAULT;
            }

            break;
        }
        case NTL_STS_TIMESTAMP_READY:
        {
            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            if (0 == list_empty(&(sts->data_queue)))
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "timestamp ready\n");
                timestamp_ready = 1;
            }
            else
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "timestamp not ready\n");
                timestamp_ready = 0;
            }

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            if (copy_to_user((void*)arg, &timestamp_ready, sizeof(unsigned int))) {
                NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "copy to user failed\n");
                return -EFAULT;
            }

            break;
        }
        case NTL_STS_GET_TIMESTAMP_DATA_LENGTH:
        {
            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            // read data width
            ntl_sts_read_reg(sts, NTL_STS_DATAWIDTH_REG, &reg_data);

            if ((reg_data % 8) == 0)
            {
                timestamp_data_length = reg_data / 8;
            }
            else
            {
                timestamp_data_length = (reg_data / 8) + 1;
            }

            if (timestamp_data_length > NTL_STS_MAX_DATA_LENGTH)
            {
                timestamp_data_length = NTL_STS_MAX_DATA_LENGTH;
            }

            NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "timestamp data length: %d\n", timestamp_data_length);

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            if (copy_to_user((void*)arg, &timestamp_data_length, sizeof(unsigned int))) {
                NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "copy to user failed\n");
                return -EFAULT;
            }

            break;
        }
        case NTL_STS_GET_TIMESTAMP:
        {
            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            while (0 != list_empty(&(sts->data_queue)))
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "timestamp not ready yet, waiting\n");
                // end spinlock section
                spin_unlock_irqrestore(&(sts->lock), flags);
                if (wait_event_interruptible(sts->wait_queue, (0 == list_empty(&(sts->data_queue)))))
                {
                    // tell the fs to handle this
                    return -ERESTARTSYS;
                }
                // start spinlock section
                spin_lock_irqsave(&(sts->lock), flags);
            }

            NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "timestamp ready\n");

            timestamp = list_entry(sts->data_queue.next, struct ntl_sts_timestamp_list, list);

            NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "timestamp read nr: %d\n", timestamp->timestamp.count);

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            if (copy_to_user((void*)arg, &(timestamp->timestamp), sizeof(struct ntl_sts_timestamp))) {
                NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "copy to user failed\n");
                return -EFAULT;
            }

            list_del(sts->data_queue.next);
            kfree(timestamp);
            if (sts->queue_length > 0)
            {
               sts->queue_length--;
            }

            break;
        }
        case NTL_STS_GET_EVENT_COUNT:
        {
            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            // read event count
            ntl_sts_read_reg(sts, NTL_STS_EVTCOUNT_REG, &reg_data);

            event_count = reg_data & 0xFFFFFFFF;

            NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "event count: %d\n", event_count);

            if (copy_to_user((void*)arg, &event_count, sizeof(unsigned int))) {
                NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "copy to user failed\n");
                spin_unlock_irqrestore(&(sts->lock), flags);
                return -EFAULT;
            }

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            break;
        }
        case NTL_STS_SET_QUEUE_MAX_LENGTH:
        {
            if (copy_from_user(&queue_max_length, (void*)arg, sizeof(int))) {
                NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "copy from user failed\n");
                return -EFAULT;
            }

            // start spinlock section
            spin_lock_irqsave(&(sts->lock), flags);

            if (queue_max_length <= 0)
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "setting queue max length to infinite\n");
        
                sts->queue_max_length = 0;
            }
            else
            {
                NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "setting queue max length to: %d\n", queue_max_length);

                sts->queue_max_length = queue_max_length;
            }

            // end spinlock section
            spin_unlock_irqrestore(&(sts->lock), flags);

            break;
        }
        default:
        {
            NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "unknown ioctl\n");
            return -EINVAL;
        }
    }

    return 0;
}

/*****************************************************************************/
/* Irq                                                                       */
/*****************************************************************************/
static irqreturn_t ntl_sts_irq(int irq, void *dev)
{
    int i;
    uint32_t reg_data;
    struct ntl_sts_timestamp_list* timestamp;
    struct ntl_sts* sts = (struct ntl_sts*)dev;
	 unsigned long flags;

    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    // start spinlock section
    spin_lock_irqsave(&(sts->lock), flags);

    // read irq register
    ntl_sts_read_reg(sts, NTL_STS_IRQ_REG, &reg_data);

    // do we have a pending irq?
    if (0 == (reg_data & NTL_STS_IRQ_VALID_BIT))
    {
        // end spinlock section
        spin_unlock_irqrestore(&(sts->lock), flags);
        return IRQ_HANDLED;
    }

    if (0 == atomic_read(&sts->open))
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "device not open\n");
        goto clear_irq;
    }

    // alloc timestamp memory
    timestamp = (struct ntl_sts_timestamp_list*)kmalloc(sizeof(struct ntl_sts_timestamp_list), GFP_ATOMIC);
    if (!timestamp)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "timestamp malloc failed\n");
        goto clear_irq;
    }

    // read count
    ntl_sts_read_reg(sts, NTL_STS_COUNT_REG, &reg_data);
    timestamp->timestamp.count = reg_data;

    // read second
    ntl_sts_read_reg(sts, NTL_STS_TIMEVALUEH_REG, &reg_data);
    timestamp->timestamp.second = reg_data;

    // read nanosecond
    ntl_sts_read_reg(sts, NTL_STS_TIMEVALUEL_REG, &reg_data);
    timestamp->timestamp.nanosecond = reg_data;

    // read data_length
    ntl_sts_read_reg(sts, NTL_STS_DATAWIDTH_REG, &reg_data);
    if ((reg_data % 8) == 0)
    {
        timestamp->timestamp.data_length = reg_data / 8;
    }
    else
    {
        timestamp->timestamp.data_length = (reg_data / 8) + 1;
    }

    // clear out data first
    memset(timestamp->timestamp.data, 0, NTL_STS_MAX_DATA_LENGTH);

    // get data (max NTL_STS_MAX_DATA_LENGTH bytes)
    for (i = 0; ((i < timestamp->timestamp.data_length) && (i < NTL_STS_MAX_DATA_LENGTH)); i++)
    {
        if ((i % 4) == 0)
        {
            ntl_sts_read_reg(sts, (NTL_STS_DATA_REG+i), &reg_data);
        }
        timestamp->timestamp.data[i] = (reg_data & 0x000000FF);
        reg_data = reg_data >> 8;
    }

    // add to the end of the queue if not reached max or no limit (-1)
    if ((sts->queue_length < sts->queue_max_length) || (sts->queue_max_length <= 0))
    {
       list_add_tail(&(timestamp->list), &(sts->data_queue));
       sts->queue_length++;
    }
    // queue full, don't add new ts and free it
    else
    {
       kfree(timestamp);
    }

    // wake up any sleeping process
    wake_up_interruptible(&(sts->wait_queue));

clear_irq:
    // clear potential pending irq
    reg_data = NTL_STS_IRQ_VALID_BIT;

    // write irq register
    ntl_sts_write_reg(sts, NTL_STS_IRQ_REG, &reg_data);

    // end spinlock section
    spin_unlock_irqrestore(&(sts->lock), flags);

    return IRQ_HANDLED;
}

/*****************************************************************************/
/* Driver probe                                                              */
/*****************************************************************************/
static int ntl_sts_probe(struct platform_device *pdev)
{
    struct device* dev;
    char dev_filename[80];
    const char* ts_source = "";
    struct ntl_sts* sts;
    struct resource* mem;
    struct resource* irq;
    uint32_t reg_data;
    int ret;
	unsigned long flags;

    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "************************************************************\n");
    NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "NetTimeLogic STS\n");
    NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "************************************************************\n");

    // alloc structure
    sts = (struct ntl_sts*)kzalloc(sizeof(struct ntl_sts), GFP_KERNEL);
    if (sts == NULL)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "kzalloc failed\n");
        ret = -ENOMEM;
        goto err_sts_alloc_failed;
    }

    // create locks
    spin_lock_init(&(sts->lock));

    // init wait queue
    init_waitqueue_head(&(sts->wait_queue));

    // no events yet
    INIT_LIST_HEAD(&(sts->data_queue));
    sts->queue_max_length = NTL_STS_DEFAULT_MAX_QUEUE_LENGTH;
    sts->queue_length = 0;

    // set closed
    atomic_set(&sts->open, 0);

    // map the private structure to the device
    dev_set_drvdata(&pdev->dev, sts);
    sts->pdev = &pdev->dev;

    // get reg addr
    mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (mem == NULL)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "platform_get_resource mem failed\n");
        ret = -EINVAL;
        goto err_platform_get_resource_mem_failed;
    }

    printk(KERN_ERR "%s ctrl_mem@0x%016llX - 0x%016llX\n", NTL_STS_DRIVER_NAME, (unsigned long long)mem->start, (unsigned long long)(mem->start + NTL_STS_REGSET_SIZE -1));

    // save physical address
    sts->physical_ctrl_base = mem->start;

    // request memory region
    if (NULL == request_mem_region(sts->physical_ctrl_base, NTL_STS_REGSET_SIZE, NTL_STS_DRIVER_NAME))
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "request_mem_region failed\n");
        ret = -ENOMEM;
        goto err_request_mem_region_failed;
    }

    // map memory to ioregion
    sts->ctrl_base = ioremap(sts->physical_ctrl_base, NTL_STS_REGSET_SIZE);
    if (sts->ctrl_base == NULL)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "ioremap failed\n");
        ret = -ENOMEM;
        goto err_ioremap_failed;
    }

    // read version register
    ntl_sts_read_reg(sts, NTL_STS_VERSION_REG, &reg_data);

    // check if something expected is here, these are the ones we definitely do not expect, 0 and DEADDEAD
    if ((reg_data == 0x00000000) || (reg_data == 0xDEADDEAD)) 
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "not a ntl timestamp device, version number wrong: 0x%08X\n", reg_data);
        ret = -EINVAL;
        goto err_our_device_failed;
    }
    else
    {
        // read data width register
        ntl_sts_read_reg(sts, NTL_STS_DATAWIDTH_REG, &reg_data);
        // the data width must be in this range (fixed by generic range)
        if ((reg_data < 32) || (reg_data > 8192))
        {
            NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "not a ntl timestamp device, data length wrong\n");
            ret = -EINVAL;
            goto err_our_device_failed;
        }
    }
    
    // get irq number
    irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
    if (irq == NULL)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "platform_get_resource irq failed\n");
        ret = -EINVAL;
        goto err_platform_get_resource_irq_failed;
    }

    printk(KERN_ERR "%s irq@0x%016llX\n", NTL_STS_DRIVER_NAME, (unsigned long long)irq->start);

    // save irq number
    sts->irq = irq->start;

    // request irq
    ret = request_threaded_irq(sts->irq, NULL, ntl_sts_irq, IRQF_ONESHOT, NTL_STS_DRIVER_NAME, (void*)sts);
    if (ret != 0) {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "request_irq failed\n");
        goto err_request_irq_failed;
    }

    // disable irq
    disable_irq(sts->irq);

    // start spinlock section
    spin_lock_irqsave(&(sts->lock), flags);

    // disable
    reg_data = 0;

    // write control register
    ntl_sts_write_reg(sts, NTL_STS_CONTROL_REG, &reg_data);

    // disable irq
    reg_data = 0;

    // write irq mask register
    ntl_sts_write_reg(sts, NTL_STS_IRQMASK_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_STS_IRQ_VALID_BIT;

    // write irq register
    ntl_sts_write_reg(sts, NTL_STS_IRQ_REG, &reg_data);

    // read version register
    ntl_sts_read_reg(sts, NTL_STS_VERSION_REG, &reg_data);

    // end spinlock section
    spin_unlock_irqrestore(&(sts->lock), flags);

    // get device number
    sts->dev_nr = MKDEV(MAJOR(ntl_sts_major), MINOR(ntl_sts_major) + ntl_sts_minor);

    // Creat device file name
    memset(dev_filename, '\0', sizeof(dev_filename));
    if (of_property_read_string(pdev->dev.of_node, "ntl,ts-name", &ts_source) == 0)
    {
        NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "ts-source: %s\n", ts_source);
        sprintf(dev_filename, "%s%d_%s", NTL_STS_DRIVER_NAME, ntl_sts_minor, ts_source);
    }
    else
    {
        sprintf(dev_filename, "%s%d", NTL_STS_DRIVER_NAME, ntl_sts_minor);
    }

    // create device
    dev = device_create(ntl_sts_class, NULL, sts->dev_nr, NULL, dev_filename);
    if(dev == NULL)
    {
        ret = -ENOMEM;
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "device_create failed\n");
        goto err_device_create_failed;
    }

    // init device
    cdev_init(&(sts->cdev), &ntl_sts_fops);
    sts->cdev.owner = THIS_MODULE;
    sts->cdev.ops = &ntl_sts_fops;
    sts->cdev.dev = sts->dev_nr;

    // register device
    ret = cdev_add(&(sts->cdev), sts->dev_nr, 1);
    if (ret != 0)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "cdev_add failed\n");
        goto err_cdev_add_failed;
    }

    // increment count for next
    ntl_sts_minor++;

    NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "STS version: %02x.%02x.%04x registered\n", ((reg_data >> 24) & 0xFF), ((reg_data >> 16) & 0xFF), ((reg_data >> 0) & 0xFFFF));

    return 0;
err_cdev_add_failed:
    device_destroy(ntl_sts_class, sts->dev_nr);
err_device_create_failed:
    free_irq(sts->irq, (void*)sts);
err_request_irq_failed:
err_platform_get_resource_irq_failed:
err_our_device_failed:
    iounmap(sts->ctrl_base);
err_ioremap_failed:
    release_mem_region(sts->physical_ctrl_base, NTL_STS_REGSET_SIZE);
err_request_mem_region_failed:
err_platform_get_resource_mem_failed:
    kfree(sts);
err_sts_alloc_failed:

    return ret;
}

/*****************************************************************************/
/* Driver remove                                                             */
/*****************************************************************************/
static int ntl_sts_remove(struct platform_device *pdev)
{
    struct ntl_sts_timestamp_list* timestamp;
    struct ntl_sts *sts = dev_get_drvdata(&pdev->dev);
    uint32_t reg_data;
	unsigned long flags;

    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    // disable irqs
    disable_irq(sts->irq);

    // unregister irq
    free_irq(sts->irq, (void*)sts);

    // start spinlock section
    spin_lock_irqsave(&(sts->lock), flags);

    // disable
    reg_data = 0;

    // write control register
    ntl_sts_write_reg(sts, NTL_STS_CONTROL_REG, &reg_data);

    // disable irq
    reg_data = 0;

    // write irq mask register
    ntl_sts_write_reg(sts, NTL_STS_IRQMASK_REG, &reg_data);

    // clear potential pending irq
    reg_data = NTL_STS_IRQ_VALID_BIT;

    // write irq register
    ntl_sts_write_reg(sts, NTL_STS_IRQ_REG, &reg_data);

    // empty all pending timestamps
    while (0 == list_empty(&(sts->data_queue)))
    {
        timestamp = list_entry(sts->data_queue.next, struct ntl_sts_timestamp_list, list);
        list_del(sts->data_queue.next);
        kfree(timestamp);
    }
    sts->queue_length = 0;

    // end spinlock section
    spin_unlock_irqrestore(&(sts->lock), flags);

    // set closed
    atomic_set(&sts->open, 0);

    // unregister device
    cdev_del(&(sts->cdev));

    // destroy device
    device_destroy(ntl_sts_class, sts->dev_nr);

    // unmap io memory
    iounmap(sts->ctrl_base);

    // release memory
    release_mem_region(sts->physical_ctrl_base, NTL_STS_REGSET_SIZE);

    // unmap the private structure to the device
    dev_set_drvdata(&pdev->dev, NULL);

    // free structure
    kfree(sts);

    return 0;
}

/*****************************************************************************/
/* Driver device mapping                                                     */
/*****************************************************************************/
static struct of_device_id ntl_sts_of_match[] = {
    { .compatible = "ntl,ntl_sts", },
    { .compatible = "ntl_sts", },
    { /* end of list */ }
};

MODULE_DEVICE_TABLE(of, ntl_sts_of_match);

static struct platform_driver ntl_sts_of_driver = {
    .probe = ntl_sts_probe,
    .remove = ntl_sts_remove,
    .driver = {
        .name = NTL_STS_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = ntl_sts_of_match,
    },
};

static int __init ntl_sts_init(void)
{
    int ret;

    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    // register char device region
    ret = alloc_chrdev_region(&ntl_sts_major, 0, 256, NTL_STS_DRIVER_NAME);
    if (ret < 0)
    {
        NTL_STS_DPRINTK(NTL_STS_ERROR_LEVEL, "alloc_chrdev_region failed\n");
        return ret;
    }

    // create device class
    ntl_sts_class = class_create(THIS_MODULE, NTL_STS_DRIVER_NAME);

    return platform_driver_register(&ntl_sts_of_driver);
}

static void __exit ntl_sts_exit(void)
{
    NTL_STS_DPRINTK(NTL_STS_DEBUG_LEVEL, "%s:%s entering function\n", NTL_STS_DRIVER_NAME, __FUNCTION__);

    // destroy device class
    class_destroy(ntl_sts_class);

    // unregister char device region
    unregister_chrdev_region(ntl_sts_major, 256);

    platform_driver_unregister(&ntl_sts_of_driver);
}

module_init(ntl_sts_init);
module_exit(ntl_sts_exit);

/*****************************************************************************/
/* Driver definitions                                                        */
/*****************************************************************************/
MODULE_AUTHOR(NTL_STS_DRIVER_AUTHOR);
MODULE_DESCRIPTION(NTL_STS_DRIVER_DESC);
MODULE_LICENSE(NTL_STS_DRIVER_LICENSE);
