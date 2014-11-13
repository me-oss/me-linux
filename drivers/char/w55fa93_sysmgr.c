/* linux/driver/input/w55fa93_sysmgr.c
 *
 * Copyright (c) 2010 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Changelog:
 *
 */
 /* This is a skeleton of system manager driver. A real one is highly system dependent, so not implemented here.*/

#include <linux/init.h>
#include <linux/slab.h>
#include <asm/errno.h>
#include <asm/delay.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <asm/arch/irqs.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <asm/arch/w55fa93_sysmgr.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/regs-clock.h>

typedef struct
{
	// no spmaphore protect for data here. seems not necessary for now. 
	u32 num;
	u32 opened;
	u32 status;
} sysmgr_data;

static DECLARE_WAIT_QUEUE_HEAD(sysmgr_q);
static struct cdev sysmgr_dev[4];
static sysmgr_data sdata[4];
static u32 usbd_state, audio_state, usbh_state, usb_share;


void sysmgr_report(unsigned status)
{
	int i;

	for (i=0; i<4; i++) {
		if (sdata[i].opened != 0) {
			if (status == SYSMGR_STATUS_SD_INSERT)
				if (sdata[i].status & SYSMGR_STATUS_SD_REMOVE)
					sdata[i].status &= ~SYSMGR_STATUS_SD_REMOVE;
				else
					sdata[i].status |= SYSMGR_STATUS_SD_INSERT;
			else if (status == SYSMGR_STATUS_SD_REMOVE)
				if (sdata[i].status & SYSMGR_STATUS_SD_INSERT)
					sdata[i].status &= ~SYSMGR_STATUS_SD_INSERT;
				else
					sdata[i].status |= SYSMGR_STATUS_SD_REMOVE;
			else if ((status == SYSMGR_STATUS_USBD_PLUG) || (status == SYSMGR_STATUS_USBD_CONNECT_PC))
				if (sdata[i].status & SYSMGR_STATUS_USBD_UNPLUG)
					sdata[i].status &= ~SYSMGR_STATUS_USBD_UNPLUG;
				else
					sdata[i].status |= status;
			else if (status == SYSMGR_STATUS_USBD_UNPLUG) {
				if (sdata[i].status & SYSMGR_STATUS_USBD_PLUG)
					sdata[i].status &= ~SYSMGR_STATUS_USBD_PLUG;
				else
					sdata[i].status |= SYSMGR_STATUS_USBD_UNPLUG;
				if (sdata[i].status & SYSMGR_STATUS_USBD_CONNECT_PC)
					sdata[i].status &= ~SYSMGR_STATUS_USBD_CONNECT_PC;
			}
			else if (status == SYSMGR_STATUS_USBH_PLUG)
				if (sdata[i].status & SYSMGR_STATUS_USBH_UNPLUG)
					sdata[i].status &= ~SYSMGR_STATUS_USBH_UNPLUG;
				else
					sdata[i].status |= SYSMGR_STATUS_USBH_PLUG;
			else if (status == SYSMGR_STATUS_USBH_UNPLUG)
				if (sdata[i].status & SYSMGR_STATUS_USBH_PLUG)
					sdata[i].status &= ~SYSMGR_STATUS_USBH_PLUG;
				else
					sdata[i].status |= SYSMGR_STATUS_USBH_UNPLUG;
			else if ((status >= SYSMGR_STATUS_NORMAL) && (status <= SYSMGR_STATUS_POWER_DOWN)) {
				if (!(sdata[i].status & (SYSMGR_STATUS_RTC_POWER_OFF|SYSMGR_STATUS_POWER_OFF)))
					sdata[i].status = (sdata[i].status & ~(0x7F000000)) | status;
			}
			else if ((status == SYSMGR_STATUS_RTC_POWER_OFF) || (status == SYSMGR_STATUS_POWER_OFF))
				sdata[i].status = (sdata[i].status & ~(0x7F000000)) | status;
			else
				sdata[i].status |= status;
		}
	}

	if (status == SYSMGR_STATUS_USBD_PLUG)
		usbd_state = 1;
	else if (status == SYSMGR_STATUS_USBD_UNPLUG)
		usbd_state = 0;
	if (status == SYSMGR_STATUS_USBH_PLUG)
		usbh_state = 1;
	else if (status == SYSMGR_STATUS_USBH_UNPLUG)
		usbh_state = 0;
	if (status == SYSMGR_STATUS_AUDIO_OPEN)
		audio_state = 1;
	else if (status == SYSMGR_STATUS_AUDIO_CLOSE)
		audio_state = 0;

	wake_up_interruptible(&sysmgr_q);
}
EXPORT_SYMBOL(sysmgr_report);

int w55fa93_sysmgr_open(struct inode* i,struct file* file)
{
	sdata[MINOR(i->i_rdev)].opened++;
	file->private_data = &sdata[MINOR(i->i_rdev)];

	return 0;
}

int w55fa93_sysmgr_close(struct inode* i,struct file* file)
{
	int num;

	sdata[(num = MINOR(i->i_rdev))].opened--;
	if (sdata[num].opened == 0)
		sdata[num].status = 0;

	return 0;
}

ssize_t w55fa93_sysmgr_read(struct file *file, char *buff, size_t read_mode, loff_t *offp)
{
	sysmgr_data *data = (sysmgr_data *)file->private_data;

	while (data->status == 0) {
		if (wait_event_interruptible(sysmgr_q, (data->status != 0))) {
			return -ERESTARTSYS;
		}
	}
	copy_to_user(buff, &data->status, 4);
	data->status = 0;

	return 4;
}

ssize_t w55fa93_sysmgr_write(struct file *file, const char *buf, size_t count, loff_t *f_pos)
{
	unsigned int cmd;
	unsigned int flags;

	if (copy_from_user(&cmd, buf, 4))
		return -EFAULT;

	return 4;
}

static int w55fa93_sysmgr_ioctl(struct inode *inode, struct file *file, unsigned int cmd, void *arg)
{
	unsigned int cmd_data, key_state;
	int ret = 0;

	switch (cmd) {
	case SYSMGR_IOC_SET_POWER_STATE:
		if (copy_from_user((void *)&cmd_data, (void *)arg, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
	 	if (cmd_data == SYSMGR_CMD_NORMAL)
			sysmgr_report(SYSMGR_STATUS_NORMAL);
	 	else if (cmd_data == SYSMGR_CMD_DISPLAY_OFF)
			sysmgr_report(SYSMGR_STATUS_DISPLAY_OFF);
	 	else if (cmd_data == SYSMGR_CMD_IDLE)
			sysmgr_report(SYSMGR_STATUS_IDLE);
		else if (cmd_data == SYSMGR_CMD_MEMORY_IDLE)
			sysmgr_report(SYSMGR_STATUS_MEMORY_IDLE);
		else if (cmd_data == SYSMGR_CMD_POWER_DOWN)
			sysmgr_report(SYSMGR_STATUS_POWER_DOWN);
		else if (cmd_data == SYSMGR_CMD_RTC_POWER_OFF)
			sysmgr_report(SYSMGR_STATUS_RTC_POWER_OFF);
		else if (cmd_data == SYSMGR_CMD_POWER_OFF)
			sysmgr_report(SYSMGR_STATUS_POWER_OFF);
		else
			ret = -ENOIOCTLCMD;
		break;

	case SYSMGR_IOC_GET_USBD_STATE:
		if (copy_to_user((void *)arg, (void *)&usbd_state, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		break;

	case SYSMGR_IOC_GET_USBH_STATE:
		if (copy_to_user((void *)arg, (void *)&usbh_state, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		break;

	case SYSMGR_IOC_GET_AUDIO_STATE:
		if (copy_to_user((void *)arg, (void *)&audio_state, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		break;

	case SYSMGR_IOC_GET_POWER_KEY:
		if ((inl(REG_RTC_AER) & 0x10000) == 0x0) {
			// set RTC register access enable password
			outl(0xA965, REG_RTC_AER);
			// make sure RTC register read/write enable
			while ((inl(REG_RTC_AER) & 0x10000) == 0x0) ;
		}

		if ((inl(REG_RTC_PWRON) & (1<<7)))
			key_state = 1;
		else
			key_state = 0;
		if (copy_to_user((void *)arg, (void *)&key_state, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		break;

	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

static unsigned int w55fa93_sysmgr_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	sysmgr_data *data = (sysmgr_data *)file->private_data;
	poll_wait(file, &sysmgr_q, wait);
	if (data->status != 0)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

struct file_operations w55fa93_sysmgr_fops = {
owner:
	THIS_MODULE,
open:
	w55fa93_sysmgr_open,
read:
	w55fa93_sysmgr_read,
write:
	w55fa93_sysmgr_write,
ioctl:
	w55fa93_sysmgr_ioctl,
poll:
	w55fa93_sysmgr_poll,
release:
	w55fa93_sysmgr_close,
};


static int __init w55fa93_sysmgr_reg(void)
{
	dev_t dev = MKDEV(SYSMGR_MAJOR, SYSMGR_MINOR0);

	if (register_chrdev_region(dev, 4, "sysmgr")) {
		printk("Sysmgr initial the device error!\n");
		return -1;
	}

	cdev_init(&sysmgr_dev[0], &w55fa93_sysmgr_fops);
	sysmgr_dev[0].owner = THIS_MODULE;
	sysmgr_dev[0].ops = &w55fa93_sysmgr_fops;

	cdev_init(&sysmgr_dev[1], &w55fa93_sysmgr_fops);
	sysmgr_dev[1].owner = THIS_MODULE;
	sysmgr_dev[1].ops = &w55fa93_sysmgr_fops;

	cdev_init(&sysmgr_dev[2], &w55fa93_sysmgr_fops);
	sysmgr_dev[2].owner = THIS_MODULE;
	sysmgr_dev[2].ops = &w55fa93_sysmgr_fops;

	cdev_init(&sysmgr_dev[3], &w55fa93_sysmgr_fops);
	sysmgr_dev[3].owner = THIS_MODULE;
	sysmgr_dev[3].ops = &w55fa93_sysmgr_fops;

	if (cdev_add(&sysmgr_dev[0], dev, 1))
	printk("Error adding w55fa93 Sysmgr\n");

	dev = MKDEV(SYSMGR_MAJOR, SYSMGR_MINOR1);
	if (cdev_add(&sysmgr_dev[1], dev, 1))
	printk("Error adding w55fa93 Sysmgr1\n");

	dev = MKDEV(SYSMGR_MAJOR, SYSMGR_MINOR2);
	if (cdev_add(&sysmgr_dev[2], dev, 1))
	printk("Error adding w55fa93 Sysmgr2\n");

	dev = MKDEV(SYSMGR_MAJOR, SYSMGR_MINOR3);
	if (cdev_add(&sysmgr_dev[3], dev, 1))
	printk("Error adding w55fa93 Sysmgr3\n");

	memset(sdata, 0, sizeof(sdata));
	sdata[0].num = 0;
	sdata[1].num = 1;
	sdata[2].num = 2;
	sdata[3].num = 3;

	init_waitqueue_head(&sysmgr_q);
	printk("w55fa93 Sysmgr driver has been initialized successfully!\n");

	return 0;
}

static void __exit w55fa93_sysmgr_exit(void)
{
	dev_t devno = MKDEV(SYSMGR_MAJOR, SYSMGR_MINOR0);

	cdev_del(&sysmgr_dev[0]);
	cdev_del(&sysmgr_dev[1]);
	cdev_del(&sysmgr_dev[2]);
	cdev_del(&sysmgr_dev[3]);

	unregister_chrdev_region(devno, 4);
}

module_init(w55fa93_sysmgr_reg);
module_exit(w55fa93_sysmgr_exit);

