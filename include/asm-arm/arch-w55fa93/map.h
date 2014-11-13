/* arch/arm/mach-w55fa93/include/mach/map.h
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 * wan zongshun <zswan@nuvoton.com>
 * Based on arch/arm/mach-s3c2410/include/mach/map.h
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ASM_ARCH_MAP_H
#define __ASM_ARCH_MAP_H


#ifndef __ASSEMBLY__
#define W55FA93_ADDR(x)		((void __iomem *)0xF0000000 + (x))
#else
#define W55FA93_ADDR(x)		(0xF0000000 + (x))
#endif


/* interrupt controller is the first thing we put in, to make
 * the assembly code for the irq detection easier
 */

/* GCR_BA */
#define W55FA93_VA_GCR		W55FA93_ADDR(0x08000000)
#define W55FA93_PA_GCR		(0xB0000000)
#define W55FA93_SZ_GCR		SZ_4K

/* AIC_BA */
#define W55FA93_VA_IRQ		W55FA93_ADDR(0x00000000)
#define W55FA93_PA_IRQ		(0xB8000000)
#define W55FA93_SZ_IRQ		SZ_4K

/* SDIC_BA */
#define W55FA93_VA_SDIC		W55FA93_ADDR(0x00003000)
#define W55FA93_PA_SDIC		0xB0003000
#define W55FA93_SZ_SDIC		SZ_4K

/* EDMA_BA */
#define W55FA93_VA_EDMA		W55FA93_ADDR(0x00008000)
#define W55FA93_PA_EDMA		0xB0008000
#define W55FA93_SZ_EDMA		SZ_4K

/* SPU_BA */
#define W55FA93_VA_SPU		W55FA93_ADDR(0x01000000)
#define W55FA93_PA_SPU		0xB1000000
#define W55FA93_SZ_SPU		SZ_4K

/* I2S_BA */
#define W55FA93_VA_I2SM		W55FA93_ADDR(0x01001000)
#define W55FA93_PA_I2SM		0xB1001000
#define W55FA93_SZ_I2SM		SZ_4K

/* LCD_BA */
#define W55FA93_VA_VPOST	W55FA93_ADDR(0x01002000)
#define W55FA93_PA_VPOST	0xB1002000
#define W55FA93_SZ_VPOST	SZ_4K

/* CAP_BA */
#define W55FA93_VA_VIDEOIN	W55FA93_ADDR(0x01003000)
#define W55FA93_PA_VIDEOIN	0xB1003000
#define W55FA93_SZ_VIDEOIN	SZ_4K

/* FSC_BA */
#define W55FA93_VA_FSC		W55FA93_ADDR(0x01005000)
#define W55FA93_PA_FSC		0xB1005000
#define W55FA93_SZ_FSC		SZ_4K

/* SIC_BA */
#define W55FA93_VA_SIC		W55FA93_ADDR(0x01006000)
#define W55FA93_PA_SIC		0xB1006000
#define W55FA93_SZ_SIC		SZ_4K

/* UDC_BA */
#define W55FA93_VA_USBD		W55FA93_ADDR(0x01008000)
#define W55FA93_PA_USBD		0xB1008000
#define W55FA93_SZ_USBD		SZ_4K

/* UHC_BA */
#define W55FA93_VA_USBH		W55FA93_ADDR(0x01009000)
#define W55FA93_PA_USBH		0xB1009000
#define W55FA93_SZ_USBH		SZ_4K

/* JPG_BA */
#define W55FA93_VA_JPEG		W55FA93_ADDR(0x0100A000)
#define W55FA93_PA_JPEG		0xB100A000
#define W55FA93_SZ_JPEG		SZ_4K

/* BLT_BA */
#define W55FA93_VA_BLT		W55FA93_ADDR(0x0100D000)
#define W55FA93_PA_BLT		0xB100D000
#define W55FA93_SZ_BLT		SZ_4K

/* GP_BA */
#define W55FA93_VA_GPIO		W55FA93_ADDR(0x08001000)
#define W55FA93_PA_GPIO		0xB8001000
#define W55FA93_SZ_GPIO		SZ_4K

/* TMR_BA */
#define W55FA93_VA_TIMER	W55FA93_ADDR(0x08002000)
#define W55FA93_PA_TIMER	0xB8002000
#define W55FA93_SZ_TIMER	SZ_4K

/* RTC_BA */
#define W55FA93_VA_RTC		W55FA93_ADDR(0x08003000)
#define W55FA93_PA_RTC		0xB8003000
#define W55FA93_SZ_RTC		SZ_4K

/* I2C_BA */
#define W55FA93_VA_I2C		W55FA93_ADDR(0x08004000)
#define W55FA93_PA_I2C		0xB8004000
#define W55FA93_SZ_I2C		SZ_4K

/* KPI_BA */
#define W55FA93_VA_KPI		W55FA93_ADDR(0x08005000)
#define W55FA93_PA_KPI		0xB8005000
#define W55FA93_SZ_KPI		SZ_4K

/* PWM_BA */
#define W55FA93_VA_PWM		W55FA93_ADDR(0x08007000)
#define W55FA93_PA_PWM		0xB8007000
#define W55FA93_SZ_PWM		SZ_4K

/* UA_BA */
#define W55FA93_VA_UART		W55FA93_ADDR(0x08008000)
#define W55FA93_PA_UART		0xB8008000
#define W55FA93_SZ_UART		SZ_4K

/* SPIMS0_BA */
#define W55FA93_VA_SPI0		W55FA93_ADDR(0x0800C000)
#define W55FA93_PA_SPI0		0xB800C000
#define W55FA93_SZ_SPI0		SZ_4K

/* ADC_BA */
#define W55FA93_VA_ADC		W55FA93_ADDR(0x0800E000)
#define W55FA93_PA_ADC		0xB800E000
#define W55FA93_SZ_ADC		SZ_4K


#endif /* __ASM_ARCH_MAP_H */
