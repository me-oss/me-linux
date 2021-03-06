/* linux/include/asm-arm/arch-w55fa93/w55fa93_sysmgr.h
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
 */


#ifndef W55FA93_SYSMGR_H
#define W55FA93_SYSMGR_H

#define SYSMGR_MAJOR	199
#define SYSMGR_MINOR0	0
#define SYSMGR_MINOR1	1
#define SYSMGR_MINOR2	2
#define SYSMGR_MINOR3	3

#define SYSMGR_STATUS_SD_INSERT		0x00000010
#define SYSMGR_STATUS_SD_REMOVE		0x00000020
#define SYSMGR_STATUS_USBD_PLUG		0x00000040
#define SYSMGR_STATUS_USBD_UNPLUG	0x00000080
#define SYSMGR_STATUS_AUDIO_OPEN	0x00000100
#define SYSMGR_STATUS_AUDIO_CLOSE	0x00000200
#define SYSMGR_STATUS_USBH_PLUG		0x00000400
#define SYSMGR_STATUS_USBH_UNPLUG	0x00000800
#define SYSMGR_STATUS_USBD_CONNECT_PC	0x00001000
#define SYSMGR_STATUS_NORMAL		0x01000000
#define SYSMGR_STATUS_DISPLAY_OFF	0x02000000
#define SYSMGR_STATUS_IDLE		0x04000000
#define SYSMGR_STATUS_MEMORY_IDLE	0x08000000
#define SYSMGR_STATUS_POWER_DOWN	0x10000000
#define SYSMGR_STATUS_RTC_POWER_OFF	0x20000000
#define SYSMGR_STATUS_POWER_OFF		0x40000000

#define SYSMGR_CMD_NORMAL		0x01000000
#define SYSMGR_CMD_DISPLAY_OFF		0x02000000
#define SYSMGR_CMD_IDLE			0x04000000
#define SYSMGR_CMD_MEMORY_IDLE		0x08000000
#define SYSMGR_CMD_POWER_DOWN		0x10000000
#define SYSMGR_CMD_RTC_POWER_OFF	0x20000000
#define SYSMGR_CMD_POWER_OFF		0x40000000

#define SYSMGR_IOC_MAXNR		10
#define SYSMGR_IOC_MAGIC		'S'
#define SYSMGR_IOC_SET_POWER_STATE	_IOW(SYSMGR_IOC_MAGIC, 0, unsigned int)
#define SYSMGR_IOC_GET_USBD_STATE	_IOR(SYSMGR_IOC_MAGIC, 1, unsigned int)
#define SYSMGR_IOC_GET_AUDIO_STATE	_IOR(SYSMGR_IOC_MAGIC, 2, unsigned int)
#define SYSMGR_IOC_GET_POWER_KEY	_IOR(SYSMGR_IOC_MAGIC, 3, unsigned int)
#define SYSMGR_IOC_GET_USBH_STATE	_IOR(SYSMGR_IOC_MAGIC, 4, unsigned int)

#endif

