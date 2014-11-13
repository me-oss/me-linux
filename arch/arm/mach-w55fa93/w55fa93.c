/* linux/arch/arm/mach-w55fa93/w55fa93.c
 *
 * Based on linux/arch/arm/plat-s3c24xx/s3c244x.c by Ben Dooks
 *
 *Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
* wan zongshun,zswan@nuvoton.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/serial.h>

#include "cpu.h"
#include "devs.h"
/* Initial IO mappings */

static struct map_desc w55fa93_iodesc[] __initdata = {
	IODESC_ENT(TIMER),
	IODESC_ENT(USBD),
	IODESC_ENT(GCR),

	IODESC_ENT(SDIC),
	IODESC_ENT(SIC),

	IODESC_ENT(EDMA),

	IODESC_ENT(VPOST),
	IODESC_ENT(BLT),

	IODESC_ENT(VIDEOIN),
	IODESC_ENT(I2SM),
	IODESC_ENT(FSC),

	IODESC_ENT(SPU),

	IODESC_ENT(JPEG),
	IODESC_ENT(USBH),

	IODESC_ENT(PWM),
	IODESC_ENT(GPIO),
	IODESC_ENT(ADC),

	IODESC_ENT(SPI0),	

	IODESC_ENT(RTC),
	IODESC_ENT(I2C),
	IODESC_ENT(KPI),
};

struct resource w55fa93_HUART_resource[]= {
	[0] = {
		.start = W55FA93_PA_UART,
		.end   = W55FA93_PA_UART + 0x0ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_HUART,
		.end   = IRQ_HUART,
		.flags = IORESOURCE_IRQ,
	}
};

struct resource w55fa93_UART_resource[]= {
	[0] = {
		.start = W55FA93_PA_UART + 0x100,
		.end   = W55FA93_PA_UART + 0x1ff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_UART,
		.end   = IRQ_UART,
		.flags = IORESOURCE_IRQ,
	}
};

/*Init the dev resource*/
//W55FA93_RECS(HUART);
//W55FA93_RECS(UART);

/*Init the plat dev*/
W55FA93_DEVICE(uart0,HUART,0,"w55fa93-uart");
W55FA93_DEVICE(uart1,UART,1,"w55fa93-uart");

static struct platform_device *uart_devices[] __initdata = {
	&w55fa93_uart0,
	&w55fa93_uart1
};

static int w55fa93_uart_count = 0;

/* uart registration process */

void __init init_uarts_w55fa93(struct w55fa93_uartcfg *cfg, int no)
{
	struct platform_device *platdev;
	int uart;

	// enable UART clock
	//outl(inl(REG_CLKMAN) | 1 << 11, REG_CLKMAN);

	for (uart = 0; uart < no; uart++, cfg++) {
		platdev = uart_devices[cfg->hwport];

		w55fa93_uart_devs[uart] = platdev;
		platdev->dev.platform_data = cfg;
	}

	w55fa93_uart_count = uart;
}

/* w55fa93_map_io
 *
 * register the standard cpu IO areas, and any passed in from the
 * machine specific initialisation.
*/

void __init map_io_w55fa93(struct map_desc *mach_desc, int mach_size)
{
	/* register our io-tables */

	iotable_init(w55fa93_iodesc, ARRAY_SIZE(w55fa93_iodesc));
}

unsigned int w55fa93_external_clock;
unsigned int w55fa93_apll_clock;
unsigned int w55fa93_upll_clock;
unsigned int w55fa93_system_clock;
unsigned int w55fa93_cpu_clock;
unsigned int w55fa93_ahb_clock;
unsigned int w55fa93_apb_clock;
EXPORT_SYMBOL(w55fa93_external_clock);
EXPORT_SYMBOL(w55fa93_apll_clock);
EXPORT_SYMBOL(w55fa93_upll_clock);
EXPORT_SYMBOL(w55fa93_system_clock);
EXPORT_SYMBOL(w55fa93_cpu_clock);
EXPORT_SYMBOL(w55fa93_ahb_clock);
EXPORT_SYMBOL(w55fa93_apb_clock);

static inline unsigned int
w55fa93_get_pll(char pll, unsigned int xtal)
{
	unsigned char NOMap[4] = {1, 2, 2, 4};
	unsigned int pllcon;
	unsigned int NR, NF, NO;
	uint64_t fvco = 0;

	if (pll == 0)
		pllcon = inl(REG_APLLCON);
	else if (pll == 1)
		pllcon = inl(REG_UPLLCON);
	else
		return 0;

	NF = (pllcon & FB_DV) + 2;
	NR = ((pllcon & IN_DV) >> 9) + 2;
	NO = NOMap[((pllcon & OUT_DV) >> 14)];

	fvco = (uint64_t)xtal * NF;
	do_div(fvco, (NR * NO));

	return (unsigned int)fvco;
}

int w55fa93_set_apll_clock(unsigned int clock)
{
	int ret = 0;

	if (w55fa93_external_clock == 12000000) {
		if (clock == 208896) {		// for SPU/I2S 48/32KHz * 128, TD = 0.0063%
			outl(0x937D, REG_APLLCON);
		}
		else if (clock == 184320) {	// for ADC 16KHz * 16 * 80, TD = 0.0237%
			outl(0x12A7, REG_APLLCON);
		}
		else if (clock == 169344) {	// for SPU/I2S 44.1KHz * 384 and ADC 11.025KHz * 16 * 80, TD = 0.0063%
			outl(0x4EFC, REG_APLLCON);
		}
		else if (clock == 153600) {	// for ADC 24/20KHz * 16 * 80, TD = 0%
			outl(0x063E, REG_APLLCON);
		}
		else if (clock == 147456) {	// for SPU/I2S 64/96KHz * 256, TD = 0.0186%
			outl(0x0A54, REG_APLLCON);
		}
		else if (clock == 135475) {	// for SPU/I2S 88.2KHz * 256, TD = 0.0183%
			outl(0x550D, REG_APLLCON);
		}
		else if (clock == 135000) {	// for TV 27MHz, TD = 0%, 8KHz TD = 0.1243%, 11.025KHz TD = 0.3508%
			outl(0x4458, REG_APLLCON);
		}
		else if (clock == 106496) {	// for SPU/I2S 32KHz * 256, TD = 0.0038%
			outl(0x4445, REG_APLLCON);
		}
		else {
			printk("%s does not support %dKHz APLL clock!\n", __FUNCTION__, clock);
			ret = -1;
		}
	}
	else if (w55fa93_external_clock == 27000000) {
		if (clock == 208896) {
			outl(0x6324, REG_APLLCON);
		}
		else if (clock == 184320) {
			outl(0x1E72, REG_APLLCON);
		}
		else if (clock == 169344) {
			outl(0x1243, REG_APLLCON);
		}
		else if (clock == 153600) {
			outl(0xAD0F, REG_APLLCON);
		}
		else if (clock == 147456) {
			outl(0x1645, REG_APLLCON);
		}
		else if (clock == 135475) {
			outl(0x730D, REG_APLLCON);
		}
		else if (clock == 135000) {
			outl(0x4426, REG_APLLCON);
		}
		else if (clock == 106496) {
			outl(0x4E45, REG_APLLCON);
		}
		else {
			printk("%s does not support %dKHz APLL clock!\n", __FUNCTION__, clock);
			ret = -1;
		}
	}
	else {
		printk("%s does not support %dMHz xtal clock!\n", __FUNCTION__, print_mhz(w55fa93_external_clock));
		ret = -1;
	}

	if (ret == 0)
		w55fa93_apll_clock = clock;

	return ret;
}
EXPORT_SYMBOL(w55fa93_set_apll_clock);

void __init init_clocks_w55fa93(int xtal)
{
	if ((inl(REG_CHIPCFG) & 0xC) == 0x8)
		xtal = 12000000;
	else 
		xtal = 27000000;	
	w55fa93_external_clock = xtal;

#if 0
#ifndef CONFIG_SET_CPU_CLOCK_BY_BOOT_CODE
	outl(0x1ab, REG_EBIDS);
	outl(0x00AAAA00, REG_CKSKEW);
	outl(inl(REG_PWRON) & ~IPLLS, REG_PWRON);
	outl(0x60000000, REG_IBTBAS);
	mdelay(10);
	outl(inl(REG_PWRON) | IPLLS, REG_PWRON);
  #if defined CONFIG_SYSCLK_240_120_60
	outl(0x4f47, REG_PLLCON);
	outl(0x50, REG_SYSDIV);
	outl((inl(REG_EBICON) & ~(0xfff8)) | 0x3840, REG_EBICON);
  #elif defined CONFIG_SYSCLK_192_96_48
	outl(0x3f47, REG_PLLCON);
	outl(0x50, REG_SYSDIV);
	outl((inl(REG_EBICON) & ~(0xfff8)) | 0x3000, REG_EBICON);
  #elif defined CONFIG_SYSCLK_168_84_42
	outl(0x3707, REG_PLLCON);
	outl(0x51, REG_SYSDIV);
	outl((inl(REG_EBICON) & ~(0xfff8)) | 0x2760, REG_EBICON);
  #elif defined CONFIG_SYSCLK_144_72_36
	outl(0x1F24, REG_PLLCON);
	outl(0x50, REG_SYSDIV);
	outl((inl(REG_EBICON) & ~(0xfff8)) | 0x21C0, REG_EBICON);
  #elif defined CONFIG_SYSCLK_96_96_48
	outl(0x3f47, REG_PLLCON);
	outl(0x41, REG_SYSDIV);
	outl((inl(REG_EBICON) & ~(0xfff8)) | 0x3000, REG_EBICON);
  #else
    #error Unimplemented system frequency.
  #endif
#endif // CONFIG_SET_CPU_CLOCK_BY_BOOT_CODE
#endif

	w55fa93_apll_clock = w55fa93_get_pll(0, xtal) / 1000;
	w55fa93_upll_clock = w55fa93_get_pll(1, xtal) / 1000;
	if ((inl(REG_CLKDIV0) & 0x18) == 0x10)
		w55fa93_system_clock = w55fa93_apll_clock/((inl(REG_CLKDIV0)&0x7) + 1); 
	else if ((inl(REG_CLKDIV0) & 0x18) == 0x18)
		w55fa93_system_clock = w55fa93_upll_clock/((inl(REG_CLKDIV0)&0x7) + 1); 
	else
		printk("w55fa93_system_clock is unsupported!\n");
	w55fa93_cpu_clock = w55fa93_system_clock/((inl(REG_CLKDIV4)&0xF) + 1);
	w55fa93_ahb_clock = w55fa93_system_clock/((((inl(REG_CLKDIV4)>>4)&0xF) + 1) * 2);
	if (w55fa93_system_clock < (w55fa93_cpu_clock * 2))
		w55fa93_apb_clock = w55fa93_system_clock/((((inl(REG_CLKDIV4)>>8)&0xF) + 1) * 2);
	else
		w55fa93_apb_clock = w55fa93_cpu_clock/(((inl(REG_CLKDIV4)>>8)&0xF) + 1);
#if 1
	printk("w55fa93_external_clock	= %d MHz\n", print_mhz(w55fa93_external_clock));
	printk("w55fa93_apll_clock	= %d KHz\n", w55fa93_apll_clock);
	printk("w55fa93_upll_clock	= %d KHz\n", w55fa93_upll_clock);
	printk("w55fa93_system_clock	= %d KHz\n", w55fa93_system_clock);
	printk("w55fa93_cpu_clock	= %d KHz\n", w55fa93_cpu_clock);
	printk("w55fa93_ahb_clock	= %d KHz\n", w55fa93_ahb_clock);
	printk("w55fa93_apb_clock	= %d KHz\n", w55fa93_apb_clock);
#endif

}

int __init init_w55fa93(void)
{
	return platform_add_devices(w55fa93_uart_devs, w55fa93_uart_count);
}

