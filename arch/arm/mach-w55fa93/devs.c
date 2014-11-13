/* linux/arch/arm/mach-w55fa93/devs.c
 *
 * This file based on linux/arch/arm/plat-s3c24xx/devs.c by Ben Dooks
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 * Author:wan zongshun <zswan@nuvoton.com>
 *
 * Base w55fa93 platform device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dm9000.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/arch/fb.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include "devs.h"

/* Serial port registrations */
struct platform_device *w55fa93_uart_devs[2];

/*Init the platform_device
* platform_device named: w55fa93_*device
*/

static struct resource w55fa93_wdt_resource[] = {
	[0] = {
		.start = W55FA93_PA_TIMER,
		.end   = W55FA93_PA_TIMER + W55FA93_SZ_TIMER - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_WDT,
		.end   = IRQ_WDT,
		.flags = IORESOURCE_IRQ,
	}
};

struct platform_device w55fa93_device_wdt = {
	.name		  = "w55fa93-wdt",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(w55fa93_wdt_resource),
	.resource	  = w55fa93_wdt_resource,
};

EXPORT_SYMBOL(w55fa93_device_wdt);

/* USB OHCI Host Controller */

static struct resource w55fa93_usb_ohci_resource[] = {
	[0] = {
		.start = W55FA93_PA_USBH,
		.end   = W55FA93_PA_USBH + W55FA93_SZ_USBH - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_USBH,
		.end   = IRQ_USBH,
		.flags = IORESOURCE_IRQ,
	}
};

static u64 w55fa93_device_usb_ohci_dmamask = 0xffffffffUL;

struct platform_device w55fa93_usbhdevice = {
	.name		  = "w55fa93-ohci",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(w55fa93_usb_ohci_resource),
	.resource	  = w55fa93_usb_ohci_resource,
	.dev              = {
		.dma_mask = &w55fa93_device_usb_ohci_dmamask,
		.coherent_dma_mask = 0xffffffffUL
	}
};

EXPORT_SYMBOL(w55fa93_usbhdevice);

/* I2C */
static struct resource w55fa93_i2c_resource[] = {
	[0] = {
		.start = W55FA93_PA_I2C,
		.end   = W55FA93_PA_I2C + W55FA93_SZ_I2C - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_I2C,
		.end   = IRQ_I2C,
		.flags = IORESOURCE_IRQ,
	}

};
struct platform_device w55fa93_device_i2c = {
	.name		  = "w55fa93-i2c",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(w55fa93_i2c_resource),
	.resource	  = w55fa93_i2c_resource,
};

EXPORT_SYMBOL(w55fa93_device_i2c);

static struct resource w55fa93_rtc_resource[] = {
	[0] = {
		.start = W55FA93_PA_RTC,
		.end   = W55FA93_PA_RTC + 0xff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_RTC,
		.end   = IRQ_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device w55fa93_device_rtc = {
	.name		  = "w55fa93-rtc",
	.id		  = -1,
	.num_resources	  = ARRAY_SIZE(w55fa93_rtc_resource),
	.resource	  = w55fa93_rtc_resource,
};

EXPORT_SYMBOL(w55fa93_device_rtc);


/* DM9000 Ethernet */
/*static struct resource w55fa93_dm9k_resource[] = {
	[0] = {
		.start = 0x90000000,
		.end   = 0x90000000 + 0x4 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 0x90000000 + 0x4,
		.end   = 0x90000000 + 0x204 - 1,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = IRQ_IRQ1,
		.end   = IRQ_IRQ1,
		.flags = IORESOURCE_IRQ,
	}
};

static struct dm9000_plat_data w55fa93_device_dm9k_platdata = {
	.flags		  = DM9000_PLATF_16BITONLY,
};

struct platform_device w55fa93_device_dm9k = {
	.name		  = "w55fa93-dm9k",
	.id		  = 0,
	.num_resources	  = ARRAY_SIZE(w55fa93_dm9k_resource),
	.resource	  = w55fa93_dm9k_resource,
	.dev		  = {
		.platform_data = &w55fa93_device_dm9k_platdata
	}
};
EXPORT_SYMBOL(w55fa93_device_dm9k);*/

/* ZSWAN  for define*/


W55FA93_RECS(USBD);
W55FA93_DEVICE(usbdevice,USBD,0,"w55fa93-usbd");

W55FA93_RECS(ADC);
W55FA93_DEVICE(adcdevice,ADC,0,"w55fa93-ts");

W55FA93_RECS(VPOST);
W55FA93_DEVICE(lcddevice,VPOST,0,"w55fa93-lcd");


EXPORT_SYMBOL(w55fa93_usbdevice);
EXPORT_SYMBOL(w55fa93_adcdevice);
EXPORT_SYMBOL(w55fa93_lcddevice);


/*w55fa93 lcd resoure define here,but don't finish*/

void w55fa93_fb_set_platdata(struct w55fa93fb_mach_info *pd)
{
	struct w55fa93fb_mach_info *npd;

	npd = kmalloc(sizeof(*npd), GFP_KERNEL);
	if (npd) {
		memcpy(npd, pd, sizeof(*npd));
		w55fa93_lcddevice.dev.platform_data = npd;
	} else {
		printk(KERN_ERR "no memory for W55FA93 LCD platform data\n");
	}
}


