/* arch/arm/mach-w55fa93/include/mach/entry-macro.S
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 * wan zongshun <zswan@nuvoton.com>
 * Based on arch/arm/mach-s3c2410/include/mach/entry-macro.S
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <asm/arch/hardware.h>
#include <asm/arch/w55fa93_reg.h>

	.macro  get_irqnr_preamble, base, tmp
	.endm

	.macro  arch_ret_to_user, tmp1, tmp2
	.endm

	.macro	get_irqnr_and_base, irqnr, irqstat, base, tmp

		mov	\base, #AIC_BA

		ldr	\irqnr, [ \base, #AIC_IPER]
		ldr	\irqnr, [ \base, #AIC_ISNR]
		cmp	\irqnr, #0		
		streq   \tmp, [\base, #AIC_EOSCR]	@ fix the fake interrupt issue
		
	.endm

		/* currently don't need an disable_fiq macro */

		.macro	disable_fiq
		.endm
