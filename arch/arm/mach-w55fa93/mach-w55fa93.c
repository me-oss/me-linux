/* linux/arch/arm/mach-w55fa93/mach-w55fa93.c
 *
 * Based on mach-s3c2410/mach-smdk2410.c by Jonas Dietsche
 *
 * Copyright (C) 2008 Nuvoton technology corporation
 * All rights reserved.
 *
 * @Author: wan zongshun ,zswan@nuvoton.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/serial.h>
#include "devs.h"
#include "cpu.h"

static struct map_desc w55fa93_iodesc[] __initdata = {  /* nothing here yet */
};

static struct w55fa93_uartcfg w55fa93_uartcfgs[] = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0,
		.ulcon	     = 0,
		.ufcon	     = 0,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0,
		.ulcon	     = 0,
		.ufcon	     = 0,
	}
};
/*here should be your evb resourse,such as LCD*/
static struct platform_device *w55fa93_devices[] __initdata = {
	&w55fa93_usbdevice,
	&w55fa93_adcdevice,
	&w55fa93_lcddevice,
	&w55fa93_usbhdevice,
	&w55fa93_device_i2c,	
	&w55fa93_device_wdt,
	&w55fa93_device_rtc
};

static void __init mach_w55fa93_map_io(void)
	
{
	w55fa93_init_io(w55fa93_iodesc, ARRAY_SIZE(w55fa93_iodesc));
	w55fa93_init_clocks(0);
	w55fa93_init_uarts(w55fa93_uartcfgs, ARRAY_SIZE(w55fa93_uartcfgs));
}

/*cpu.c call this to register device*/
struct w55fa93_board w55fa93_board __initdata = {
	.devices       = w55fa93_devices,
	.devices_count = ARRAY_SIZE(w55fa93_devices)
};
MACHINE_START(W55FA93, "W55FA93")

	/* Maintainer: wanzongshun */
	.phys_io	= W55FA93_PA_UART,
	.io_pg_offst	= (((u32)W55FA93_VA_UART) >> 18) & 0xfffc,
	.boot_params	= 0x100,
	.map_io		= mach_w55fa93_map_io,
	.init_irq	= w55fa93_init_irq,
	.init_machine	= NULL,
	.timer		= &w55fa93_timer,
MACHINE_END
