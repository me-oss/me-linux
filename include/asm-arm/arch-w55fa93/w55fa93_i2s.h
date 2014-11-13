/* linux/include/asm-arm/arch-nuc930/nuc930_i2s.h
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Changelog:
 *
 *   2006/08/26     vincen.zswan add this file for nuvoton w55fa93 evb.
 */
 
#ifndef _W55FA93_I2S_H_
#define _W55FA93_I2S_H_

/*----- bit definition of REG_ACTL_IISCON register -----*/
#define IIS					0x0
#define MSB_Justified		0x0008
#define SCALE_1				0x0
#define SCALE_2				0x10000
#define SCALE_3				0x20000
#define SCALE_4				0x30000
#define SCALE_5				0x40000
#define SCALE_6				0x50000
#define SCALE_7				0x60000
#define SCALE_8				0x70000
#define SCALE_10			0x90000
#define SCALE_12			0xB0000
#define SCALE_14			0xD0000
#define SCALE_16			0xF0000
#define BIT16_256FS			0x0
#define BIT16_384FS			0x40
#define BIT24_384FS			0x20
/*#define FS_384				0x20
#define FS_256				0x0
#define BCLK_32				0x00
#define BCLK_48				0x40
*/

/* bit definition of L3DATA register */
#define EX_256FS 		0x20		/*-- system clock --*/
#define EX_384FS 		0x10		
#define EX_IIS			0x00		/*-- data input format  --*/
#define EX_MSB			0x08
#define EX_1345ADDR 	0x14		//The address of the UDA1345TS
#define EX_STATUS		0x02		//data transfer type (STATUS)
#define EX_DATA			0x00		//data transfer type (DATA)
#define EX_ADC_On		0xC2		//turn on the ADC
#define EX_DAC_On		0xC1		//turn on the DAC

/*----- GPIO NUM -----*/

#define L3MODE_GPIO_NUM		(1<<0)
#define L3CLOCK_GPIO_NUM 	(1<<1)
#define L3DATA_GPIO_NUM 	(1<<2)



#define MSB_FORMAT	1
#define IIS_FORMAT  2

#define	I2S_ACTIVE				0x1
#define	I2S_PLAY_ACTIVE			0x2
#define I2S_REC_ACTIVE			0x4

#endif	/* _NUC900_I2S_H_ */


