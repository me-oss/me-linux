/* jpegpriv.h
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 * <clyu2@nuvoton.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef __ASM_ARM_W55FA93_JPEG_PRIV_H
#define __ASM_ARM_W55FA93_JPEG_PRIV_H

#include <asm/semaphore.h>

#define		ISCAPTURED(priv)	(priv->videoin_bufferend_bak != priv->videoin_bufferend)

enum{
	OV_7648,
	PP_PO3030K,	
	PP_PO2010D2,
	OV_9660,
	OV_9653,
	OV_7670,
	OV_2640,
	OV_6880
};

#define SPI0_ON		1
#define SENSOR_ON	2

typedef struct videoin_priv {
	struct video_device jdev;				/* Must be the first field! */
	struct semaphore           lock;
	__u32	vaddr;						//if vaddr_src/vaddr_dst == 0, they will point to vaddr
	__u32	paddr;						//if paddr_src/paddr_dst == 0, they will point to paddr
	__u32	mmap_bufsize;				/*mmap buffer size, user can set buffer address or using mmap*/
	__u32	input_format;					/*for captured*/
	__u32	output_format;				/*for captured*/
	__u32	preview_width;				/*for captured*/
    	__u32	preview_height;				/*for captured*/
    	__u32	engine;						//output to which ip engine: LCD or jpeg
    	__u32	shotmode;					//jpeg shot mode
    	__u32	shotnumber;					//jpeg shot number for CON_NUMBER shot mode
    	__u32	preview_resolution_changed;			/*for captured*/
    	__u32 	byte2pixel;					//is 2 times of actual byte
	__u32	videoin_buffersize;				/*each video in buffer size*/
	__u32	videoin_bufferend;
	__u32	videoin_bufferend_bak;
	__u8	**videoin_buffer;				/*A pointer store those allocated buffer address  */
	void	*dev_id;
	vout_ops_t *callback; 
	
}videoin_priv_t;

typedef struct 
{
	UINT32 u32PhysAddr;
	UINT32 u32VirtAddr;
}videoIn_buf_t;

#endif//__ASM_ARM_W55FA93_JPEG_PRIV_H

