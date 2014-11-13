/* linux/driver/input/w55fa93_keypad_4buttons.c
 *
 * Copyright (c) 2009 Nuvoton technology corporation
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
#include <asm/arch/irqs.h>
#include <linux/interrupt.h>
#include  <linux/completion.h>
#include <asm/arch/w55fa93_keypad.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/regs-clock.h>



#define KPD_IRQ_NUM W55FA93_IRQ(2)  // nIRQ0

static int kpd_block=1;
static struct cdev keypad_dev;
//unsigned char DEV_NAME[10] = "Keypad";

static DECLARE_WAIT_QUEUE_HEAD(read_wait_a);

#define FAST_KPD_DELAY 5 // 0.05 sec
#define DEF_KPD_DELAY 20 // 0.2 sec
static unsigned long delay = DEF_KPD_DELAY;
static unsigned int button_value;
static struct timer_list kpd_timer;
static char timer_active = 0;
static char release = 0; // key release event for block mode
static struct mutex kpd_mutex;
static unsigned int open_cnt = 0;
u32 w55fa93_key_pressing = 0;
EXPORT_SYMBOL(w55fa93_key_pressing);

#define S_INTERVAL_TIME 5
#define L_INTERVAL_TIME 20

// arg = 0 non-block, called from isr
// arg = 1 non-block, called from timer
// arg = 2 block, called from isr
static void read_key(unsigned long arg)
{
        u32 read0;


	printk("arg=  %d\n", arg);
	if(!timer_active) {
	  printk("disable irq\n");
                disable_irq(KPD_IRQ_NUM);
		outl(0x4, REG_AIC_MDCR);

	}
        read0 = inl(REG_GPIOB_PIN) ^ 0x0F98;
	printk("=> %x\n", read0);

        if ((read0 & 0x0F98) == 0) { // released
	  printk("released, enable irq\n");
                del_timer(&kpd_timer);
                button_value = 0;
		w55fa93_key_pressing = 0;
                delay = DEF_KPD_DELAY;		
		if (arg != 0)
                	enable_irq(KPD_IRQ_NUM);
		timer_active = 0;
		release = 1;
		wake_up_interruptible(&read_wait_a);
		outl(inl(REG_IRQTGSRC0) & 0x0F980000, REG_IRQTGSRC0);
                return;
        }


        button_value = read0;

        if (arg == 2) {
                if (button_value != 0) {
                        wake_up_interruptible(&read_wait_a);
			kpd_timer.data = 2;
			//kpd_timer.function = read_key;
			mod_timer(&kpd_timer, jiffies + (timer_active ? S_INTERVAL_TIME : L_INTERVAL_TIME));
			timer_active = 1;
                } else {
		        delay = DEF_KPD_DELAY;
			timer_active = 0;
			printk("enable irq\n");
		        enable_irq(KPD_IRQ_NUM);		        
			del_timer(&kpd_timer);
		}
        } else {
                if (arg == 1) {
                        delay = FAST_KPD_DELAY;
			kpd_timer.data = 1;
                        mod_timer(&kpd_timer, jiffies + S_INTERVAL_TIME);
                } else {
		  timer_active = 1;
		        kpd_timer.data = 1;
                        mod_timer(&kpd_timer, jiffies + L_INTERVAL_TIME);
		}
        }


}

static irqreturn_t w55fa93_kpd_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	if(inl(REG_IRQTGSRC0) & 0x0F980000) {
		w55fa93_key_pressing = 1;
        	if (kpd_block)
                	read_key(2);
        	else
                	read_key(0);
	}
        // clear source
        outl(inl(REG_IRQTGSRC0) & 0x0F980000, REG_IRQTGSRC0);

        return IRQ_HANDLED;
}


int w55fa93_kpd_open(struct inode* i,struct file* f)
{
        mutex_lock(&kpd_mutex);
        if (open_cnt > 0) {
                goto exit;
        }

        kpd_block=1;

        if (f->f_flags & 0x800)
                kpd_block=0;

        init_timer(&kpd_timer);
        kpd_timer.function = read_key;	/* timer handler */
        kpd_timer.data = 1;

        if (request_irq(KPD_IRQ_NUM, w55fa93_kpd_irq, SA_INTERRUPT, "keypad",NULL) != 0) {
                printk("register the keypad_irq failed!\n");
		 mutex_unlock(&kpd_mutex);
                return -1;
        }

exit:
	open_cnt++;
	mutex_unlock(&kpd_mutex);
        return 0;
}



int w55fa93_kpd_close(struct inode* i,struct file* f)
{
        mutex_lock(&kpd_mutex);
        open_cnt--;
        if (open_cnt == 0) {
        	del_timer(&kpd_timer);
        	free_irq(KPD_IRQ_NUM,NULL);
	}
	
	mutex_unlock(&kpd_mutex);	
	
        return 0;
}

static u32 resp = 0; // record last response time with a key value
const u32 no_key = 0;
ssize_t w55fa93_kpd_read(struct file *filp, char *buff, size_t read_mode, loff_t *offp)
{
        u32 realkey;

        kpd_block = read_mode ;

        if (kpd_block) {
                if (button_value == 0)
                        wait_event_interruptible(read_wait_a, (button_value != 0) || (release != 0));

		if(release == 1) {
		        release = 0;
			realkey = 0;
		} else { 
		  realkey = (button_value >> 4) & 0xFC;
			if(button_value & 0x8)
			  realkey |= 0x04;
			if(button_value & 0x10)
			  realkey |= 0x02;	
		}
                button_value = 0;
		//			printk("r: %x\n", realkey);
                copy_to_user(buff, (char*)&realkey, sizeof(u32));
                return(1);

        }
	kpd_timer.data = 1;
        kpd_timer.function = read_key;	/* timer handler */
        if (jiffies - resp < delay)	{
                copy_to_user(buff, (char*)&no_key, sizeof(u32));
                return 1;
        }

        if (button_value != 0) {
	  resp = jiffies;
        }
	realkey = (button_value >> 4) & 0xFC;
	if(button_value & 0x8)
	  realkey |= 0x04;
	if(button_value & 0x10)
	  realkey |= 0x02;

	//printk("r: %x\n", realkey);
        copy_to_user(buff, (char*)&realkey, sizeof(u32));
	
        return 1;
}

static int w55fa93_kpd_ioctl(struct inode *inode, struct file *flip,
                            unsigned int cmd, unsigned long arg)
{

        int err = 0;

        if (_IOC_TYPE(cmd) != KEYPAD_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > KEYPAD_MAXNR) return -ENOTTY;

        if (_IOC_DIR(cmd) & _IOC_READ)
                err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));

        if (err)
                return -EFAULT;

        switch (cmd) {
        case KPD_BLOCK:
                kpd_block=1;
                flip->f_flags &= ~0x800;
                break;
        case KPD_NONBLOCK:
                kpd_block=0;
                flip->f_flags |= ~0x800;

                break;
        default:
                break;
        }

        return 0;
}

struct file_operations w55fa93_kpd_fops = {
owner:
        THIS_MODULE,
open:
        w55fa93_kpd_open,
read:
        w55fa93_kpd_read,
ioctl:
        w55fa93_kpd_ioctl,
release:
        w55fa93_kpd_close,
};



static int __init w55fa93_kpd_reg(void)
{

        dev_t dev = MKDEV(KPD_MAJOR, KPD_MINOR);


        // init GPIO
        // PORTB[3,4, 7~12]

        outl(inl(REG_GPIOB_OMD) & ~(0x1F98), REG_GPIOB_OMD); // input
        outl(inl(REG_GPIOB_PUEN) | (0x1F98), REG_GPIOB_PUEN); // pull-up
        outl(inl(REG_IRQSRCGPB) & ~(0x0FFC3C0), REG_IRQSRCGPB);
        outl(inl(REG_IRQENGPB) | 0x0F98, REG_IRQENGPB); // falling edge trigger
//        outl(0xc7, AIC_BA + 0x08);

	outl(inl(REG_IRQLHSEL) | 0x101, REG_IRQLHSEL);
// ccc 0512, owner need to fix it
//	outl(inl(REG_GPBFUN) & ~0x3FCF0000, REG_GPBFUN);
	outl(inl(REG_GPBFUN) & ~0x03FFC3C0, REG_GPBFUN);
	
        if (register_chrdev_region(dev,1,"keypad")) {
                printk("initial the device error!\n");
                return -1;
        }

        cdev_init(&keypad_dev, &w55fa93_kpd_fops);
        keypad_dev.owner = THIS_MODULE;
        keypad_dev.ops = &w55fa93_kpd_fops;

        if (cdev_add(&keypad_dev, dev, 1))
                printk(KERN_NOTICE "Error adding w55fa93 Keypad\n");


        init_waitqueue_head(&read_wait_a);
        printk("w55fa93 Keypad driver has been initialized successfully!\n");

	mutex_init(&kpd_mutex);
        return 0;
}

static void __exit w55fa93_kpd_exit(void)
{

        dev_t devno;


        cdev_init(&keypad_dev, &w55fa93_kpd_fops);
        keypad_dev.owner = THIS_MODULE;
        keypad_dev.ops = &w55fa93_kpd_fops;

        cdev_del(&keypad_dev);

        devno=MKDEV(KPD_MAJOR,KPD_MINOR);
        unregister_chrdev_region(devno,1);

}

module_init(w55fa93_kpd_reg);
module_exit(w55fa93_kpd_exit);
