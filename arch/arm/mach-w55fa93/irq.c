/* linux/arch/arm/mach-w55fa93/irq.c
 *
 * based on linux/arch/arm/plat-s3c24xx/irq.c by Ben Dooks
 * 
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 * Author:wanzongsun ,zswan@nuvoton.com
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
#include <linux/ptrace.h>
#include <linux/sysdev.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/irq.h>

//#include <asm/arch/regs-irq.h>
//#include <asm/arch/regs-timer.h>
#include <asm/arch/w55fa93_reg.h>

#include "cpu.h"

static void
w55fa93_irq_mask(unsigned int irqno)
{
	unsigned long mask;

	mask = 1UL << irqno;

	outl(mask, REG_AIC_MDCR);
}

static inline void
w55fa93_irq_ack(unsigned int irqno)
{	

	outl(0x01, REG_AIC_EOSCR);
	
}

static inline void
w55fa93_irq_maskack(unsigned int irqno)
{
	outl(1 << irqno, REG_AIC_SCCR);	 
}


static void
w55fa93_irq_unmask(unsigned int irqno)
{
	unsigned long mask;
#if 0
	if (irqno == IRQ_TIMER0)
	{
	  outl(7<<16 | inl(REG_AIC_GEN), REG_AIC_GEN);
		outl (1<<IRQ_T_INT_GROUP, REG_AIC_MECR);
	}
#endif
	mask = (1UL << irqno);
	outl(mask, REG_AIC_MECR);
}

struct irqchip w55fa93_irq_edge_chip = {
	.ack	   = w55fa93_irq_maskack,
	.mask	   = w55fa93_irq_mask,
	.unmask	   = w55fa93_irq_unmask,
	.set_wake	   = NULL
};

static struct irqchip w55fa93_irq_chip = {
	.ack	   = w55fa93_irq_ack,
	.mask	   = w55fa93_irq_mask,
	.unmask	   = w55fa93_irq_unmask,
	.set_wake	   = NULL
};



/* w55fa93_init_irq
 *
 * Initialise w55fa93 IRQ system
*/

void __init w55fa93_init_irq(void)
{
	int irqno;

	outl(0xFFFFFFFF, REG_AIC_MDCR); /* disable all interrupts */

	for (irqno = IRQ_WDT; irqno < NR_IRQS; irqno++) {	
		set_irq_chip(irqno, &w55fa93_irq_chip);
		set_irq_handler(irqno, do_level_IRQ);
		set_irq_flags(irqno, IRQF_VALID);
	}
}
