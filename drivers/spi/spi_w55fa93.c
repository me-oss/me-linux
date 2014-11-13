/* linux/driver/spi/spi_w55fa93.c
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Changelog:
 *
 *   2007/01/26     jzsun add this file for nuvoton spi driver.
 */
 
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>

#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/string.h>
#include <asm/atomic.h>

#include <asm/arch/w55fa93_spi.h>
#include <asm/arch/w55fa93_reg.h>
//#define SPI_DEBUG
//#define SPI_DEBUG_ENABLE_ENTER_LEAVE
//#define SPI_DEBUG_ENABLE_MSG
//#define SPI_DEBUG_ENABLE_MSG2

#ifdef SPI_DEBUG
#define PDEBUG(fmt, arg...)		printk(fmt, ##arg)
#else
#define PDEBUG(fmt, arg...)
#endif

#ifdef SPI_DEBUG_ENABLE_ENTER_LEAVE
#define ENTER()					PDEBUG("[%-20s] : Enter...\n", __FUNCTION__)
#define LEAVE()					PDEBUG("[%-20s] : Leave...\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef SPI_DEBUG_ENABLE_MSG
#define MSG(msg)				PDEBUG("[%-20s] : %s\n", __FUNCTION__, msg)
#else
#define MSG(msg)
#endif

#ifdef SPI_DEBUG_ENABLE_MSG2
#define MSG2(fmt, arg...)			PDEBUG("[%-20s] : "fmt, __FUNCTION__, ##arg)
#define PRNBUF(buf, count)		{int i;MSG2("Data: ");for(i=0;i<count;i++)\
									PDEBUG("%02x ", buf[i]);PDEBUG("\n");}
#else
#define MSG2(fmt, arg...)
#define PRNBUF(buf, count)
#endif


#define SPI0_CTRL_I   0x20084 // clk idle low, MSB first, 16 bits, Tx rising, Rx falling, enable interrupt
#define SPICLK  100 // 0.1M
extern unsigned int w55fa93_apb_clock;

static atomic_t spi_available = ATOMIC_INIT(1);
static struct spi_parameter global_parameter;
static wait_queue_head_t wq;
static volatile int trans_finish, slave_select;

void spi_deselect_slave(void)
{
  //  printk("d\n");	
  	outl(inl(REG_SPI0_SSR) & 0xfe, REG_SPI0_SSR);	// CS0    	
	slave_select = 0;
}

void spi_select_slave(int x)
{
  //  printk("s\n");	
  	outl(inl(REG_SPI0_SSR) | 0x01, REG_SPI0_SSR);	// CS0  	
	slave_select = 1;
}

static irqreturn_t spi_irq(int irq, void * dev_id, struct pt_regs *regs)
{
	u32 reg;
	ENTER();

	reg = inl(REG_SPI0_CNTRL);

	if (!(reg & 0x10000))	/* it not me ? */
		return IRQ_NONE;

	reg |= 0x10000;
	outl(reg, REG_SPI0_CNTRL);		/* clear interrupt flag */

	trans_finish = 1;

	wake_up_interruptible(&wq);

	LEAVE();
	return IRQ_HANDLED;
}

static int spi_transit(struct spi_data *data_ptr)
{
	u32 reg,mask;	
	ENTER();

	if (slave_select == 0)
		return -ENODEV;

	mask = (1 << data_ptr->bit_len) - 1;

	MSG2("bit_len : %d, mask : %x\n", data_ptr->bit_len, mask);

	outl(data_ptr->write_data & mask , REG_SPI0_TX0);		/* write data to hardware buffer */

	MSG2("write_data: %x -> %x\n", data_ptr->write_data & mask, REG_SPI0_TX0);

	reg = (global_parameter.sleep << 12) |
		(global_parameter.lsb << 10) |
		(data_ptr->bit_len << 3) |
		(global_parameter.tx_neg << 2) |
		(global_parameter.rx_neg << 1) | 0x20001;

	trans_finish = 0;
	outl(reg, REG_SPI0_CNTRL);		/* start */
	wait_event_interruptible(wq, trans_finish != 0);
	
	data_ptr->read_data = inl(REG_SPI0_RX0) & mask;

	MSG2("read_data: %x <- %x\n", data_ptr->read_data & mask, REG_SPI0_RX0);

	LEAVE();
	return 0;
}

static int spi_ioctl(struct inode *inode, struct file *flip, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct spi_parameter tmp_parameter;
	struct spi_data tmp_data;

	ENTER();
	
	if(_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;
	if(_IOC_NR(cmd) > SPI_IOC_MAXNR)
		return -ENOTTY;
	if(_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));

	if(err) 
		return -EFAULT;

	switch(cmd) {
		case SPI_IOC_GETPARAMETER:
			if (copy_to_user((void *)arg, &global_parameter, 
				sizeof(struct spi_parameter)))
				return -EFAULT;
			break;
		case SPI_IOC_SETPARAMETER:
			if (copy_from_user(&tmp_parameter, (void *)arg, 
				sizeof(struct spi_parameter)))
				return -EFAULT;
			memcpy(&global_parameter, &tmp_parameter,
				sizeof(struct spi_parameter));
			outl(global_parameter.divider, REG_SPI0_DIVIDER);	/* update clock */
			break;
		case SPI_IOC_SELECTSLAVE:
			if (arg < -1 && arg > 1) 
				return -EPERM;			
			if (arg == 1) 
				spi_deselect_slave();						
			else
				spi_select_slave(arg);
			break;
		case SPI_IOC_TRANSIT:
			if (copy_from_user(&tmp_data, (void *)arg, sizeof(tmp_data)))
				return -EFAULT;
			err = spi_transit(&tmp_data);
			if (err)
				return err;
			if (copy_to_user((void *)arg, &tmp_data, sizeof(tmp_data)))
				return -EFAULT;
			break;
		default:
			return -ENOTTY;			
	}

	LEAVE();			
	return 0;
}

static int spi_open(struct inode *inode, struct file *filp)
{
	u32 reg;
	int retval = -EBUSY;
	ENTER();

	if (! atomic_dec_and_test (&spi_available)) 
		goto failed;	

	global_parameter.active_level = 0;
	global_parameter.lsb = 0;
	global_parameter.rx_neg = 0;
	global_parameter.tx_neg = 1;
	global_parameter.divider = 0x1;
	global_parameter.sleep = 0;
	slave_select = 0;


    if (request_irq(IRQ_SPI0, spi_irq, SA_INTERRUPT, "spi0", NULL) != 0) {
                printk("register the keypad_irq failed!\n");
                return -1;
    }

	LEAVE();
	return 0; /* success */

failed:
	atomic_inc(&spi_available); /* release the device */
	return retval;
}

static int spi_release(struct inode *inode, struct file *flip)
{
	u32 reg;	
	ENTER();

	reg = inl(REG_SPI0_CNTRL);
	reg &= 0xffff;
	outl(reg, REG_SPI0_CNTRL);

	free_irq(IRQ_SPI0, NULL);
	spi_deselect_slave();
	atomic_inc(&spi_available); /* release the device */

	LEAVE();	
	return 0;
}


struct file_operations spi_fops =                                                 
{
	owner: 		THIS_MODULE,
	open:		spi_open,
	release:		spi_release,
	ioctl:		spi_ioctl,
};

static struct cdev *spi_cdev; 

static int __init spi_init(void)
{
	int result;
	u32 divider = (w55fa93_apb_clock/(SPICLK * 2)) - 1;	

	ENTER(); 
	
	outl(inl(REG_APBCLK) | SPIMS0_CKE, REG_APBCLK);

        // init SPI0 interface
       outl(inl(REG_APBIPRST) | SPI0RST, REG_APBIPRST);	//reset spi0
	outl(inl(REG_APBIPRST) & ~SPI0RST, REG_APBIPRST);
	
        outl(inl(REG_GPDFUN) | 0xFF000000, REG_GPDFUN);  // configuer pin function
        outl(divider, REG_SPI0_DIVIDER);
        outl(0, REG_SPI0_SSR);        
               
	init_waitqueue_head(&wq);

	/* every things ok, now, we can register char device safely */
	result = register_chrdev_region(MKDEV(SPI_MAJOR, 0), 1, "spi0");
	if (result < 0)
	{
		printk("usi : can't get major %d\n", SPI_MAJOR);
		goto failed;
	}	

	spi_cdev = cdev_alloc();
	if (spi_cdev == NULL)
	{
		printk("usi : alloc cdev error\n");
		result = -ENOMEM;
		goto failed_unregister;
	}

	spi_cdev->ops = & spi_fops;
	spi_cdev->owner = THIS_MODULE;

	result = cdev_add(spi_cdev, MKDEV(SPI_MAJOR, 0), 1);
	if (result < 0)
	{
		printk("usi : cdev add error\n");
		goto failed_unregister;
	}

	printk("W55FA93 SPI0 driver has been installed successfully!\n");
	LEAVE();
	return 0;

failed_unregister:
	unregister_chrdev_region(MKDEV(SPI_MAJOR, 0), 1);

failed:
	return result;
}

static void __exit spi_exit(void)
{
	cdev_del(spi_cdev);
	unregister_chrdev_region(MKDEV(SPI_MAJOR, 0), 1);
}

module_init(spi_init);
module_exit(spi_exit);

