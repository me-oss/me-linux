/* arch/arm/mach-w55fa93/include/mach/uncompress.h
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 * wan zongshun <zswan@nuvoton.com>
 * Based on arch/arm/mach-s3c2410/include/mach/uncompress.h
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#ifndef __ASM_ARCH_UNCOMPRESS_H
#define __ASM_ARCH_UNCOMPRESS_H

/* defines for UART registers */
#include "asm/arch/w55fa93_reg.h"

#include <asm/arch/map.h>

/* working in physical space... */
#undef W55FA93_GPIOREG
#undef W55FA93_WDOGREG


/* how many bytes we allow into the FIFO at a time in FIFO mode */
#define FIFO_MAX	 (14)

#define uart_base W55FA93_PA_UART

static __inline__ void
uart_wr(unsigned int reg, unsigned int val)
{
	volatile unsigned int *ptr;

	ptr = (volatile unsigned int *)(reg + uart_base);
	*ptr = val;
}

static __inline__ unsigned int
uart_rd(unsigned int reg)
{
	volatile unsigned int *ptr;

	ptr = (volatile unsigned int *)(reg + uart_base);
	return *ptr;
}


/* we can deal with the case the UARTs are being run
 * in FIFO mode, so that we don't hold up our execution
 * waiting for tx to happen...
*/

static void putc(int ch)
{
		/* not using fifos */
	
	while (!(uart_rd(W55FA93_COM_FSR) & UART_FSR_TEMT))
			barrier();
	

	/* write byte to transmission register */
	uart_wr(W55FA93_COM_TX, ch);
}

static inline void flush(void)
{
}

#define __raw_writel(d,ad) do { *((volatile unsigned int *)(ad)) = (d); } while(0)

/* CONFIG_W55FA93_BOOT_WATCHDOG
 *
 * Simple boot-time watchdog setup, to reboot the system if there is
 * any problem with the boot process
*/

#ifdef CONFIG_W55FA93_BOOT_WATCHDOG

#define WDOG_COUNT (0xff00)

static inline void arch_decomp_wdog(void)
{
  return;
}

static void arch_decomp_wdog_start(void)
{
  return;
}

#else
#define arch_decomp_wdog_start()
#define arch_decomp_wdog()
#endif

#ifdef CONFIG_W55FA93_BOOT_ERROR_RESET

static void arch_decomp_error(const char *x)
{
	putstr("\n\n");
	putstr(x);
	putstr("\n\n -- System Halt\n");

	while(1);
}

#define arch_error arch_decomp_error
#endif

static void error(char *err);

static void
arch_decomp_setup(void)
{
	/* we may need to setup the uart(s) here if we are not running
	 * on an BAST... the BAST will have left the uarts configured
	 * after calling linux.
	 */

	arch_decomp_wdog_start();
}


#endif /* __ASM_ARCH_UNCOMPRESS_H */
