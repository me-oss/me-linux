/* linux/arch/arm/mach-w55fa93/cpu.c
 *
 * Based on linux/arch/arm/plat-s3c24xx/cpu.c
 *
 * Copyright (c)2008 Nuvoton technology corporation
 *	http://www.nuvoton.com
 *	@Author:wan zongshun <zswan@nuvoton.com>
 *
 * W55FA93 CPU Support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/rtc.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/rtc.h>
#include <asm/tlbflush.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/w55fa93_sysmgr.h>
#include "cpu.h"

/* table of supported CPUs */

static struct cpu_table cpu_ids[] __initdata = {

	{
		.idcode		= 0x00FA5C30,
		.idmask		= 0xFFFFFFFF,
		.map_io		= map_io_w55fa93,
		.init_clocks	= init_clocks_w55fa93,
		.init_uarts	= init_uarts_w55fa93,
		.init		= init_w55fa93,
		.name		= "W55FA93"
	},
};

/* minimal IO mapping */

static struct map_desc w55fa93_iodesc[] __initdata = {
	IODESC_ENT(IRQ),
	IODESC_ENT(UART)
};


static struct cpu_table *
w55fa93_lookup_cpu(unsigned long idcode)
{
	struct cpu_table *tab;
	int count;

	tab = cpu_ids;
	for (count = 0; count < ARRAY_SIZE(cpu_ids); count++, tab++) {
		if ((idcode & tab->idmask) == tab->idcode)
			return tab;
	}

	return NULL;
}

/* cpu information */

static struct cpu_table *cpu;

void __init w55fa93_init_io(struct map_desc *mach_desc, int size)
{
	unsigned long idcode = 0x00FA5C30;

	/* initialise the io descriptors we need for initialisation */
	iotable_init(w55fa93_iodesc, ARRAY_SIZE(w55fa93_iodesc));

	cpu = w55fa93_lookup_cpu(idcode);

	if (cpu == NULL) {
		printk(KERN_ERR "Unknown CPU type 0x%08lx\n", idcode);
		panic("Unknown w55fa93 CPU");
	}

	if (cpu->map_io == NULL || cpu->init == NULL) {
		printk(KERN_ERR "CPU %s support not enabled\n", cpu->name);
		panic("Unsupported w55fa93  CPU");
	}

	printk("CPU %s (id 0x%08lx)\n", cpu->name, idcode);

	(cpu->map_io)(mach_desc, size);
}

/* w55fa93_init_clocks
 *
 * Initialise the clock subsystem and associated information from the
 * given master crystal value.
 *
 * xtal  = 0 -> use default PLL crystal value (normally 15MHz)
 *      != 0 -> PLL crystal value in Hz
*/

void __init w55fa93_init_clocks(xtal)
{
	if (xtal == 0)
		xtal = CLOCK_TICK_RATE;

	if (cpu == NULL)
		panic("w55fa93_init_clocks: no cpu setup?\n");

	if (cpu->init_clocks == NULL)
		panic("w55fa93_init_clocks: cpu has no clock init\n");
	else
		(cpu->init_clocks)(xtal);
}

void __init w55fa93_init_uarts(struct w55fa93_uartcfg *cfg, int no)
{
	if (cpu == NULL)
		return;

	if (cpu->init_uarts == NULL) {
		printk(KERN_ERR "w55fa93_init_uarts: cpu has no uart init\n");
	} else
		(cpu->init_uarts)(cfg, no);
}

static int __init w55fa93_arch_init(void)
{
	int ret;

	// do the correct init for cpu

	if (cpu == NULL)
		panic("w55fa93_arch_init: NULL cpu\n");

	ret = (cpu->init)();
	if (ret != 0)
		return ret;

	if (&w55fa93_board != NULL) {
		struct platform_device **ptr = w55fa93_board.devices;
		int i;

		for (i = 0; i < w55fa93_board.devices_count; i++, ptr++) {
			ret = platform_device_register(*ptr);

			if (ret) {
				printk(KERN_ERR "w55fa93: failed to add board device %s (%d) @%p\n", (*ptr)->name, ret, *ptr);
			}
		}

		/* mask any error, we may not need all these board
		 * devices */
		ret = 0;
	}

	return ret;
}


static struct platform_device *sys_clk;
extern unsigned int w55fa93_upll_clock;
extern unsigned int w55fa93_system_clock;
extern unsigned int w55fa93_cpu_clock;
extern unsigned int w55fa93_ahb_clock;
extern unsigned int w55fa93_apb_clock;
extern u32 w55fa93_key_pressing;
extern u32 w55fa93_ts_pressing;
static u32 sram_vaddr;

void enter_clock_setting(u8, u8, u8) __attribute__ ((section ("enter_cs")));
void enter_clock_setting(u8 sys_div, u8 cpu_div, u8 apb_div)
{
	unsigned int volatile i;

	// push register pages to TLB cache entry
	inl(REG_CLKDIV0);
	inl(REG_SDEMR);

	if (sys_div > 0) {
		outl((inl(REG_CLKDIV0) & ~SYSTEM_N1) | (sys_div<<8), REG_CLKDIV0);
		outl((inl(REG_CLKDIV4) & ~(APB_N|CPU_N)) | ((apb_div<<8)|cpu_div), REG_CLKDIV4);
		// disable DLL of SDRAM device
		outl(inl(REG_SDEMR) | DLLEN, REG_SDEMR);
		// disable Low Frequency mode
		outl(inl(REG_SDOPM) | LOWFREQ, REG_SDOPM);
	} else {
		// disable DLL of SDRAM device
		outl(inl(REG_SDEMR) & ~DLLEN, REG_SDEMR);
		// disable Low Frequency mode
		outl(inl(REG_SDOPM) & ~LOWFREQ, REG_SDOPM);
		outl((inl(REG_CLKDIV0) & ~SYSTEM_N1) | (sys_div<<8), REG_CLKDIV0);
		outl((inl(REG_CLKDIV4) & ~(APB_N|CPU_N)) | ((apb_div<<8)|cpu_div), REG_CLKDIV4);
	}
	for (i = 0; i < 0x1000; i++) ;
}

int set_system_clocks(u32 sys_clock, u32 cpu_clock, u32 apb_clock)
{
	void (*cs_func)(u8, u8, u8);
	unsigned int volatile flags;
	u32 int_mask, tmp_system_clock, tmp_cpu_clock, tmp_hclk1_clock;
	u8 sys_div, cpu_div, apb_div;

	sys_div = w55fa93_upll_clock / sys_clock - 1 + (w55fa93_upll_clock%sys_clock ? 1:0);
	tmp_system_clock = w55fa93_upll_clock / (sys_div+1);
	cpu_div = tmp_system_clock / cpu_clock - 1 + (tmp_system_clock%cpu_clock ? 1:0);
	tmp_cpu_clock = tmp_system_clock / (cpu_div+1);
	tmp_hclk1_clock = (tmp_system_clock < tmp_cpu_clock*2) ? tmp_cpu_clock/2:tmp_cpu_clock;
	apb_div = tmp_hclk1_clock / apb_clock - 1 + (tmp_hclk1_clock%apb_clock ? 1:0);
	//printk("sys_div=%d, cpu_div=%d, apb_div=%d\n", sys_div, cpu_div, apb_div);

	if (cpu_div > 1) {
		printk("CPU divider must be 0 or 1 !!\n");
		return -1;
	}

	w55fa93_system_clock = tmp_system_clock;
	w55fa93_cpu_clock = tmp_cpu_clock;
	w55fa93_ahb_clock = w55fa93_system_clock / 2;
	w55fa93_apb_clock = (w55fa93_cpu_clock/2) / (apb_div+1);
#if 0
	printk("SYS clock = %d\n", w55fa93_system_clock);
	printk("CPU clock = %d\n", w55fa93_cpu_clock);
	printk("AHB clock = %d\n", w55fa93_ahb_clock);
	printk("APB clock = %d\n", w55fa93_apb_clock);
	printk("REG_CLKDIV0 = 0x%x\n", inl(REG_CLKDIV0));
	printk("REG_CLKDIV4 = 0x%x\n", inl(REG_CLKDIV4));
#endif

	save_flags(flags);
	cli();
	int_mask = inl(REG_AIC_IMR);
	// diable all interrupts
	outl(0xFFFFFFFF, REG_AIC_MDCR);
	//restore_flags(flags);

	// put enter_clock_setting into SRAM
	memcpy(sram_vaddr, enter_clock_setting, 512);
	cs_func = (void(*)(u8, u8, u8)) (sram_vaddr);

	// flush all TLB cache entries
	local_flush_tlb_all();
	// change the system clocks
	cs_func(sys_div, cpu_div, apb_div);

	//save_flags(flags);
	//cli();
	outl(0xFFFFFFFF, REG_AIC_MDCR);
	// restore interrupt mask
	outl(int_mask, REG_AIC_MECR);
	restore_flags(flags);

	return 0;
}

static ssize_t
write_clk(struct device *dev, struct device_attribute *attr,
	  const char *buffer, size_t count)
{
	unsigned int volatile flags;

	if (w55fa93_upll_clock == 192000) {
		// SYS:CPU:AHB:APB = 192:192:96:48
		if (buffer[0] == '1' && buffer[1] == '9' && buffer[2] == '2') {
			set_system_clocks(192000, 192000, 48000);
		}

		// SYS:CPU:AHB:APB = 96:96:48:48
		else if (buffer[0] == '9' && buffer[1] == '6') {
			set_system_clocks(96000, 96000, 48000);
		}
	}
	else if (w55fa93_upll_clock == 240000) {
		// SYS:CPU:AHB:APB = 240:240:120:60
		if (buffer[0] == '2' && buffer[1] == '4' && buffer[2] == '0') {
			set_system_clocks(240000, 240000, 60000);
		}

		// SYS:CPU:AHB:APB = 120:120:60:60
		else if (buffer[0] == '1' && buffer[1] == '2' && buffer[2] == '0') {
			set_system_clocks(120000, 120000, 60000);
		}
	}

	// RTC power off mode
	if (buffer[0] == 'r' && buffer[1] == 'p' && buffer[2] == 'o') {
		// disable LVR
		outl(inl(REG_MISCR) & ~(LVR_RDY | LVR_EN), REG_MISCR);

		// turn off speaker
#if defined(CONFIG_HANNSTARR_HSD043I9W1_480x272)
		outl(inl(REG_GPIOB_OMD) | (1 << 3), REG_GPIOB_OMD);
		outl(inl(REG_GPIOB_DOUT) & ~(1 << 3), REG_GPIOB_DOUT);
#elif defined(CONFIG_GOWORLD_GWMTF9360A_320x240)
		outl(inl(REG_GPIOE_OMD) | (1 << 1), REG_GPIOE_OMD);
		outl(inl(REG_GPIOE_DOUT) & ~(1 << 1), REG_GPIOE_DOUT);
#endif

		// turn off video out
		outl((inl(REG_LCM_TVCtl) & ~TVCtl_LCDSrc) | 0x800, REG_LCM_TVCtl);

		// diable system interrupts
		save_flags(flags);
		cli();

		while ((inl(REG_RTC_AER) & 0x10000) != 0x0) ;
		// set RTC register access enable password
		outl(0xA965, REG_RTC_AER);
		// make sure RTC register read/write enable
		while ((inl(REG_RTC_AER) & 0x10000) == 0x0) {
			mdelay(10);
			outl(0xA965, REG_RTC_AER);
		}

		// RTC will power off
		outl((inl(REG_RTC_PWRON) & ~0x5) | 0x2, REG_RTC_PWRON);

		// enable system interrupts
		restore_flags(flags);

		// wait system enter power off
		while (1) ;
	}

	return count;
}

/* Attach the sysfs write method */
DEVICE_ATTR(clock, 0644, NULL, write_clk);

/* Attribute Descriptor */
static struct attribute *clk_attrs[] = {
	&dev_attr_clock.attr,
	NULL
};

/* Attribute group */
static struct attribute_group clk_attr_group = {
	.attrs = clk_attrs,
};

static int __init w55fa93_system_clock_init(void)
{
	/* Register a platform device */
	printk("register clock device\n");

	sys_clk = platform_device_register_simple("w55fa93-clk", -1, NULL, 0);
	if (sys_clk == NULL)
		printk("register failed\n");
	sysfs_create_group(&sys_clk->dev.kobj, &clk_attr_group);
	sram_vaddr = ioremap(0xFF000000, 4*1024);
}

arch_initcall(w55fa93_arch_init);
module_init(w55fa93_system_clock_init);

