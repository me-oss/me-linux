/* linux/arch/arm/mach-w55fa93/time.c
 *
 * Based on linux/arch/arm/plat-s3c24xx/time.c by Ben Dooks
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 * wanzongshun,zswan@nuvoton.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/clk.h>

#include <asm/system.h>
#include <asm/leds.h>
#include <asm/mach-types.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/map.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/mach/time.h>

#include "cpu.h"

extern unsigned int w55fa93_external_clock;

static unsigned long w55fa93_gettimeoffset (void)
{
	return 0;
}

/*
 * IRQ handler for the timer
 */
static irqreturn_t
w55fa93_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	write_seqlock(&xtime_lock);
	timer_tick(regs);
	write_sequnlock(&xtime_lock);
	outl(0x01, REG_TISR); /* clear TIF0 */ 

	return IRQ_HANDLED;
}

static struct irqaction w55fa93_timer_irq = {
	.name		= "w55fa93 Timer Tick",
	.flags		= SA_INTERRUPT | SA_TIMER,
	.handler	= w55fa93_timer_interrupt,
};

/*
 * Set up timer reg.
 */

static void w55fa93_timer_setup (void)
{
	outl(0, REG_TCSR0);
	outl(0, REG_TCSR1);
	outl(0x03, REG_TISR);  /* clear for safty */ 
	outl(w55fa93_external_clock/HZ, REG_TICR0);
	outl(0x68000000, REG_TCSR0);
}

static void __init w55fa93_timer_init (void)
{
	w55fa93_timer_setup();
	setup_irq(IRQ_TIMER0, &w55fa93_timer_irq);
}

struct sys_timer w55fa93_timer = {
	.init		= w55fa93_timer_init,
	.offset		= w55fa93_gettimeoffset,
	.resume		= w55fa93_timer_setup
};
