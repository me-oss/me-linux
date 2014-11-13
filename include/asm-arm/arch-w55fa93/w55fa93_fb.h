/*
 * linux/drivers/video/w55fa93fb.h
 *	Copyright (c) 2004 Arnaud Patard
 *
 *  W55FA93 LCD Framebuffer Driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
*/

#ifndef __W55FA93FB_H
#define __W55FA93FB_H

struct w55fa93fb_info {
	struct fb_info		*fb;
	struct device		*dev;
	struct clk		*clk;

	struct w55fa93fb_mach_info *mach_info;

	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char *		map_cpu;	/* virtual */
	u_int			map_size;

	struct w55fa93fb_hw	regs;

	/* addresses of pieces placed in raw buffer */
	u_char *		screen_cpu;	/* virtual address of buffer */
	dma_addr_t		screen_dma;	/* physical address of buffer */
	unsigned int		palette_ready;

	/* keep these registers in case we need to re-write palette */
	u32			palette_buffer[256];
	u32			pseudo_pal[16];
};

#define PALETTE_BUFF_CLEAR (0x80000000)	/* entry is clear/invalid */

int w55fa93fb_init(void);

/*
#ifdef CONFIG_TOPPOLY_TD028THEA1_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_AUO_A035QN02_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_SOLOMON_SSD1297_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_HIMAX_HX8224_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_ILITEK_ILI9325_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_GIANTPLUS_GPM905A0_SRGBDUMMY_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_GIANTPLUS_GPM905A0_CCIR656_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_GIANTPLUS_GPM1040A0_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#if defined(CONFIG_NOVATEK_GPG48273QS5_480X272) || defined(CONFIG_GOWORLD_GWMTF7456B_480X272)
#define LCDWIDTH 640		// reserve bigger space 
#define LCDHEIGHT 272
#define LCDBPP 16
#endif

#ifdef CONFIG_CHRONTEL_TVOUT_VGA_640X480
#define LCDWIDTH 640
#define LCDHEIGHT 480
#define LCDBPP 16
#endif
*/

#ifdef CONFIG_TVOUT_QVGA_320x240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_TVOUT_VGA_640x480
#define LCDWIDTH 640
#define LCDHEIGHT 480
#define LCDBPP 16
#endif

#ifdef CONFIG_TVOUT_D1_720x480
#define LCDWIDTH 720
#define LCDHEIGHT 480
#define LCDBPP 16
#endif

#ifdef CONFIG_HANNSTARR_HSD043I9W1_480x272
#define LCDWIDTH 480
#define LCDHEIGHT 272
#define LCDBPP 16
#endif

#ifdef CONFIG_SHARP_LQ035Q1DH02_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_WINTEK_WMF3324_320X240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef	CONFIG_AMPIRE_800x480
#define LCDWIDTH 800
#define LCDHEIGHT 480
#define LCDBPP 16
#endif

#ifdef	CONFIG_AMPIRE_800x600
#define LCDWIDTH 800
#define LCDHEIGHT 600
#define LCDBPP 16
#endif

#ifdef CONFIG_GOWORLD_GWMTF9360A_320x240
#define LCDWIDTH 320
#define LCDHEIGHT 240
#define LCDBPP 16
#endif

#ifdef CONFIG_GOWORLD_GW8973_480x272
#define LCDWIDTH 480
#define LCDHEIGHT 272
#define LCDBPP 16
#endif

#ifdef CONFIG_VG680_640x480
#define LCDWIDTH 640
#define LCDHEIGHT 480
#define LCDBPP 16
#endif


#define DISPLAY_MODE_RGB565	0
#define DISPLAY_MODE_RGB555	1
#define DISPLAY_MODE_YCBYCR	2

#define VIDEO_ACTIVE_WINDOW_COORDINATES	_IOW('v', 22, unsigned int)	//set display-start line in display buffer
#define VIDEO_DISPLAY_ON								_IOW('v', 24, unsigned int)	//display on
#define VIDEO_DISPLAY_OFF								_IOW('v', 25, unsigned int)	//display off
#define IOCTLCLEARSCREEN		_IOW('v', 26, unsigned int)	//clear screen
#define IOCTL_LCD_BRIGHTNESS					_IOW('v', 27, unsigned int)  //brightness control		
#define IOCTLBLACKSCREEN		_IOW('v', 23, unsigned int)	//clear screen ****** changed from 28 > 23

#define IOCTL_LCD_ENABLE_INT		_IO('v', 28)
#define IOCTL_LCD_DISABLE_INT		_IO('v', 29)

#define IOCTL_LCD_RGB565_2_RGB555	_IO('v', 30)
#define IOCTL_LCD_RGB555_2_RGB565	_IO('v', 31)
#define IOCTL_LCD_GET_DMA_BASE          _IOR('v', 32, unsigned int *)
#define DUMP_LCD_REG			_IOR('v', 33, unsigned int *)
#define VIDEO_DISPLAY_LCD		_IOW('v', 38, unsigned int)	//display LCD only
#define VIDEO_DISPLAY_TV		_IOW('v', 39, unsigned int)	//display TV only 

#endif
