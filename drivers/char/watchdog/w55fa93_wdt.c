/*
 * Copyright (c) 2009 Nuvoton technology corporation.
 *
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;version 2 of the License.
 *
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/watchdog.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/arch/w55fa93_reg.h>

#define WDT_TIMEOUT		(2 * HZ)
#define WDT_HEARTBEAT		10


static int heartbeat = WDT_HEARTBEAT;
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog heartbeats in seconds. "
	"(default = " __MODULE_STRING(WDT_HEARTBEAT) ")");

static int nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, int, 0);
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started "
	"(default=" __MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

struct w55fa93_wdt {
	struct resource  *res;
	struct clk	 *wdt_clock;
	struct platform_device *pdev;
	void __iomem	 *wdt_base;
	char		 expect_close;
	struct timer_list timer;
	spinlock_t       wdt_lock;
	unsigned long next_heartbeat;
};

static unsigned long w55fa93wdt_busy;
struct w55fa93_wdt *w55fa93_wdt;

static inline unsigned int resource_size(struct resource *res)
{
	return res->end - res->start + 1;
}

static inline void w55fa93_wdt_keepalive(void)
{
	unsigned int val;
	//spin_lock(&w55fa93_wdt->wdt_lock);

	outl(0xc, REG_WTCR); // clear reset flag
	outl(0x1, REG_WTCR); // reset WDT
	while(inl(REG_WTCR) & 1); // wait 'til reset complete
	outl(0x4F2, REG_WTCR); // reconfigure WDT

	//spin_unlock(&w55fa93_wdt->wdt_lock);
}

static inline void w55fa93_wdt_start(void)
{
	unsigned int val;

	//spin_lock(&w55fa93_wdt->wdt_lock);
	outl(0x1, REG_WTCR);
	while(inl(REG_WTCR) & 1); // wait 'til reset complete
	outl(0x4F2, REG_WTCR);  
	//spin_unlock(&w55fa93_wdt->wdt_lock);
}


static int w55fa93_wdt_open(struct inode *inode, struct file *file)
{

	if (test_and_set_bit(0, &w55fa93wdt_busy))
		return -EBUSY;

	w55fa93_wdt_start();

	w55fa93_wdt->next_heartbeat = jiffies + heartbeat * HZ;
	mod_timer(&w55fa93_wdt->timer, jiffies + WDT_TIMEOUT);

	return nonseekable_open(inode, file);
}

static int w55fa93_wdt_close(struct inode *inode, struct file *file)
{
	clear_bit(0, &w55fa93wdt_busy);

	if (!w55fa93_wdt->expect_close)
		del_timer(&w55fa93_wdt->timer);

	outl(0xc, REG_WTCR); // clear reset flag
	outl(0x1, REG_WTCR); // reset WDT
	w55fa93_wdt->expect_close = 0;
	return 0;
}

static struct watchdog_info w55fa93_wdt_info = {
	.identity	= "w55fa93 watchdog",
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING |
						WDIOF_MAGICCLOSE,
};

static long w55fa93_wdt_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int new_value;

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user(argp, &w55fa93_wdt_info,
				sizeof(w55fa93_wdt_info)) ? -EFAULT : 0;
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, p);

	case WDIOC_KEEPALIVE:
		w55fa93_wdt->next_heartbeat = jiffies + heartbeat * HZ;
		return 0;

	case WDIOC_SETTIMEOUT:
		if (get_user(new_value, p))
			return -EFAULT;

		heartbeat = new_value;
		w55fa93_wdt->next_heartbeat = jiffies + heartbeat * HZ;

		return put_user(new_value, p);
	case WDIOC_GETTIMEOUT:
		return put_user(heartbeat, p); 
	default:
		return -ENOTTY;
	}
}

static ssize_t w55fa93_wdt_write(struct file *file, const char __user *data,
						size_t len, loff_t *ppos)
{
	/* Scan for magic character */
	if (!nowayout) {
		size_t i;

		w55fa93_wdt->expect_close = 0;

		for (i = 0; i < len; i++) {
			char c;
			if (get_user(c, data + i))
				return -EFAULT;
			if (c == 'V') {
				w55fa93_wdt->expect_close = 42;
				break;
			}
		}
	}

	w55fa93_wdt->next_heartbeat = jiffies + heartbeat * HZ;
	return len;
}

static void w55fa93_ping(unsigned long data)
{
	
	if (time_before(jiffies, w55fa93_wdt->next_heartbeat) ||
			(!nowayout && !w55fa93wdt_busy)) {
		w55fa93_wdt_keepalive();
		mod_timer(&w55fa93_wdt->timer, jiffies + WDT_TIMEOUT);
	} else {
		dev_warn(&w55fa93_wdt->pdev->dev, "Will reset the machine !\n");
		printk("<0>" "WDT driver: timeout %d seconds,Restart system now!!!\n",heartbeat);
		outl(0x1, REG_WTCR); // reset WDT
		while(inl(REG_WTCR) & 1); // wait 'til reset complete
		outl(0x4C2, REG_WTCR); // reconfigure WDT, reset within 1.56 sec		
	}
}

static const struct file_operations w55fa93wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.unlocked_ioctl	= w55fa93_wdt_ioctl,
	.open		= w55fa93_wdt_open,
	.release	= w55fa93_wdt_close,
	.write		= w55fa93_wdt_write,
};

static struct miscdevice w55fa93wdt_miscdev = {
	.minor		= WATCHDOG_MINOR,
	.name		= "watchdog",
	.fops		= &w55fa93wdt_fops,
};

static int __devinit w55fa93wdt_probe(struct platform_device *pdev)
{
	int ret = 0;

	w55fa93_wdt = kzalloc(sizeof(struct w55fa93_wdt), GFP_KERNEL);
	if (!w55fa93_wdt)
		return -ENOMEM;

	w55fa93_wdt->pdev = pdev;

	spin_lock_init(&w55fa93_wdt->wdt_lock);

	w55fa93_wdt->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (w55fa93_wdt->res == NULL) {
		dev_err(&pdev->dev, "no memory resource specified\n");
		ret = -ENOENT;
		goto err_get;
	}

	if (!request_mem_region(w55fa93_wdt->res->start,
				resource_size(w55fa93_wdt->res), pdev->name)) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_get;
	}

	w55fa93_wdt->wdt_base = ioremap(w55fa93_wdt->res->start,
					resource_size(w55fa93_wdt->res));
	if (w55fa93_wdt->wdt_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap() region\n");
		ret = -EINVAL;
		goto err_req;
	}

	outl(inl(REG_APBCLK) | WDCLK_CKE, REG_APBCLK);
	outl(inl(REG_WTCR) & 0xc, REG_WTCR);  // need to clear flag if system reboot from wdt

	if (misc_register(&w55fa93wdt_miscdev)) {
		dev_err(&pdev->dev, "err register miscdev on minor=%d (%d)\n",
			WATCHDOG_MINOR, ret);
		goto err_clk;
	}

	setup_timer(&w55fa93_wdt->timer, w55fa93_ping, 0);

	return 0;

err_clk:
	outl(inl(REG_APBCLK) & ~WDCLK_CKE, REG_APBCLK);
err_map:
	iounmap(w55fa93_wdt->wdt_base);
err_req:
	release_mem_region(w55fa93_wdt->res->start,
					resource_size(w55fa93_wdt->res));
err_get:
	kfree(w55fa93_wdt);
	return ret;
}

static int __devexit w55fa93wdt_remove(struct platform_device *pdev)
{
	misc_deregister(&w55fa93wdt_miscdev);

	outl(inl(REG_APBCLK) & ~WDCLK_CKE, REG_APBCLK);

	iounmap(w55fa93_wdt->wdt_base);

	release_mem_region(w55fa93_wdt->res->start,
					resource_size(w55fa93_wdt->res));

	kfree(w55fa93_wdt);

	return 0;
}

static struct platform_driver w55fa93wdt_driver = {
	.probe		= w55fa93wdt_probe,
	.remove		= __devexit_p(w55fa93wdt_remove),
	.driver		= {
		.name	= "w55fa93-wdt",
		.owner	= THIS_MODULE,
	},
};

static int __init w55fa93_wdt_init(void)
{

	return platform_driver_register(&w55fa93wdt_driver);
}

static void __exit w55fa93_wdt_exit(void)
{
	platform_driver_unregister(&w55fa93wdt_driver);
}

module_init(w55fa93_wdt_init);
module_exit(w55fa93_wdt_exit);

MODULE_AUTHOR("Wan ZongShun <mcuos.com@gmail.com>");
MODULE_DESCRIPTION("Watchdog driver for W55FA93");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS("platform:w55fa93-wdt");
