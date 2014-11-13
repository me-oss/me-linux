/* linux/driver/vedio/w55fa93fb.c
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
 *   2008/12/18     Jhe add this file for nuvoton W55FA93 LCD Controller.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <asm/cacheflush.h>
#include <asm/mach/map.h>
#include <asm/arch/regs-clock.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/fb.h>

#include <asm/param.h> 
#include <linux/timer.h>
#include <asm/arch/w55fa93_fb.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
//Do not Supprot osd 
#if 0
/* OSD IMAGE SIZE */
unsigned int OSD_IMAGE_WIDTH  = OSD_WIDTH;
unsigned int OSD_IMAGE_HEIGHT = OSD_HEIGHT;
/* OSD Window Starting Coordinates */
unsigned int OSD_X_WINDOW = 0;
unsigned int OSD_Y_WINDOW = 0;
#endif

#define EDMA_USE_IRQ  
#define VDMA_CH       0
#define DUAL_BUF      1             // 1-> Use VDMA for VPOST dual buffer mode, 0-> Use Signle buffer for VPOST
/*
	Allocate Video & OSD memory size, 
	just equal to screen size (LCDWIDTH*LCDHEIGHT*LCDBPP/8)
*/	
unsigned long video_alloc_len;


/* video global mapping variables */
unsigned long video_buf_mmap;
unsigned long video_dma_mmap;
unsigned long video_cpu_mmap;

static DECLARE_WAIT_QUEUE_HEAD(wq);

/* AHB & APB system clock information */
extern unsigned int w55fa93_ahb_clock;
extern unsigned int w55fa93_apb_clock;

/* FSC */
#ifdef CONFIG_W55FA93_VIDEOIN
extern unsigned int bIsVideoInEnable;
extern unsigned int w55fa93_VIN_PAC_BUFFER;
#endif
/* PWM duty cycle */
unsigned int g_PWMOutputDuty=0;
unsigned int w55fa93_pwm_channel;

static u32 _auto_disable = 0;

#if DUAL_BUF
static u32 _bg_mem_v = 0;
static u32 _bg_mem_p = 0;
#endif
static u32 _dma_len = 0;
static u32 _dma_dst = 0;

#if defined(CONFIG_TVOUT_D1_720x480) || defined(CONFIG_TVOUT_VGA_640x480) || defined (CONFIG_TVOUT_QVGA_320x240)
#define     TV_OUTPUT_DEVICE        1       // Use TVFILED_INT for IRQ (TV Filed Interrupt)
#else
#define     TV_OUTPUT_DEVICE        0       // Use VINT for IRQ (LCD VSYNC End interrupt)
#endif

#ifdef	CONFIG_TVOUT_QVGA_320x240
	#include "w55fa93_TV.c"
#elif 	CONFIG_TVOUT_VGA_640x480
	#include "w55fa93_TV.c"
#elif 	CONFIG_TVOUT_D1_720x480
	#include "w55fa93_TV.c"
#endif

#ifdef	CONFIG_HANNSTARR_HSD043I9W1_480x272
#include "w55fa93_HannStar_HSD043I9W1.c"
#endif

#ifdef	CONFIG_SHARP_LQ035Q1DH02_320X240
#include "w55fa93_Sharp_LQ035Q1DH02.c"
#endif

#ifdef	CONFIG_WINTEK_WMF3324_320X240
#include "w55fa93_Wintek_WMF3324.c"
#endif

#ifdef	CONFIG_AMPIRE_800x480
#include "w55fa93_Ampire_800x480.c"
#endif

#ifdef	CONFIG_AMPIRE_800x600
#include "w55fa93_Ampire_800x600.c"
#endif

#ifdef	CONFIG_GOWORLD_GWMTF9360A_320x240
#include "w55fa93_GOWORLD_GWMTF9360A.c"
#endif

#ifdef	CONFIG_GOWORLD_GW8973_480x272
#include "w55fa93_GOWORLD_GW8973.c"
#endif

#ifdef	CONFIG_VG680_640x480
#include "w55fa93_VG680.c"
#endif


#ifdef EDMA_USE_IRQ
void fb_edma_irq_handler(unsigned int arg)
{
//	printk("fb_edma_irq_handler\n");
	w55fa93_edma_free(VDMA_CH);  
}
#endif

static void w55fa93fb_set_lcdaddr(struct w55fa93fb_info *fbi)
{
	/*set frambuffer start phy addr*/
	outl(video_dma_mmap, REG_LCM_FSADDR);
}


static int w55fa93fb_check_var(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	struct w55fa93fb_info *fbi = info->par;
	
	printk("check_var(var=%p, info=%p)\n", var, info);
	/* validate x/y resolution */
	if (var->yres > fbi->mach_info->yres.max)
		var->yres = fbi->mach_info->yres.max;
	else if (var->yres < fbi->mach_info->yres.min)
		var->yres = fbi->mach_info->yres.min;

	if (var->xres > fbi->mach_info->xres.max)
		var->xres = fbi->mach_info->xres.max;
	else if (var->xres < fbi->mach_info->xres.min)
		var->xres = fbi->mach_info->xres.min;

	/* validate bpp */
	if (var->bits_per_pixel > fbi->mach_info->bpp.max)
		var->bits_per_pixel = fbi->mach_info->bpp.max;
	else if (var->bits_per_pixel < fbi->mach_info->bpp.min)
		var->bits_per_pixel = fbi->mach_info->bpp.min;

	/* set r/g/b positions */
	if (var->bits_per_pixel == 16) {
		var->red.offset			= 11;
		var->green.offset		= 5;
		var->blue.offset		= 0;
		var->red.length			= 5;
		var->green.length		= 6;
		var->blue.length		= 5;
		var->transp.length	= 0;
	} else {
		var->red.length			= var->bits_per_pixel;
		var->red.offset			= 0;
		var->green.length		= var->bits_per_pixel;
		var->green.offset		= 0;
		var->blue.length		= var->bits_per_pixel;
		var->blue.offset		= 0;
		var->transp.length	= 0;
	}
	return 0;
}

static void w55fa93fb_activate_var(struct w55fa93fb_info *fbi,
				   struct fb_var_screeninfo *var)
{
}

static int w55fa93fb_set_par(struct fb_info *info)
{
	struct w55fa93fb_info *fbi = info->par;
	struct fb_var_screeninfo *var = &info->var;
	if (var->bits_per_pixel == 16)
		fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;
	else
		fbi->fb->fix.visual = FB_VISUAL_PSEUDOCOLOR;

	fbi->fb->fix.line_length     = (var->width*var->bits_per_pixel)/8;

	/* activate this new configuration */
	w55fa93fb_activate_var(fbi, var);
	return 0;
}	

/* from pxafb.c */
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

static int w55fa93fb_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	struct w55fa93fb_info *fbi = info->par;
	unsigned int val;
	switch (fbi->fb->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseuo-palette */
		if (regno < 16) {
			u32 *pal = fbi->fb->pseudo_palette;

			val  = chan_to_field(red,   &fbi->fb->var.red);
			val |= chan_to_field(green, &fbi->fb->var.green);
			val |= chan_to_field(blue,  &fbi->fb->var.blue);

			pal[regno] = val;
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		if (regno < 256) {
			/* currently assume RGB 5-6-5 mode */
			val  = ((red   >>  0) & 0xf800);
			val |= ((green >>  5) & 0x07e0);
			val |= ((blue  >> 11) & 0x001f);
		}
		break;

	default:
		return 1;   /* unknown type */
	}
	return 0;
}

static int w55fa93fb_blank(int blank_mode, struct fb_info *info)
{
    return w55fa93fb_blank_device(blank_mode, info);
}


static int w55fa93fb_open(struct fb_info *info, int init)
{
        //printk("w55fa93fb_open\n");
//	outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) | 0x20000, REG_LCM_LCDCInt); // enable VIN
//        _auto_disable = 0;
#if TV_OUTPUT_DEVICE
		outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) | 0x40000, REG_LCM_LCDCInt); // enable VIN	//### Chris
#else
		outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) | 0x20000, REG_LCM_LCDCInt); // enable VIN	//### Chris
#endif		
        _auto_disable = 0;	//## Chris  1;  set as _auto_disable=0 could let VIN continue enable, may degrade 004_bitmap.swf fps 10% on screen but not affect the console output fps.
        return 0;
}

static int w55fa93fb_close(struct fb_info *info, int init)
{
	unsigned int volatile flags;

	//printk("w55fa93fb_close\n");
	_auto_disable = 0;
	
	return 0;
}

static int w55fa93fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
    return w55fa93fb_ioctl_device(info, cmd, arg);
}

static struct fb_ops w55fa93fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= w55fa93fb_check_var,
	.fb_set_par		= w55fa93fb_set_par,
	.fb_blank			= w55fa93fb_blank,
	.fb_setcolreg	= w55fa93fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
	.fb_open	= w55fa93fb_open,
	.fb_release	= w55fa93fb_close,			//### Chris	
	.fb_ioctl			= w55fa93fb_ioctl, 
	/*.fb_mmap 			= w55fa93fb_mmap,*/
};

void LCDDelay(unsigned int nCount)
{
		unsigned volatile int i;
		
		for(;nCount!=0;nCount--)
			for(i=0;i<100;i++);
}



#ifdef CONFIG_ASSIGN_FB_ADDR
extern unsigned int w55fa93_fb_v;
#endif
static int __init w55fa93fb_map_video_memory(struct w55fa93fb_info *fbi)
{
        fbi->map_size = PAGE_ALIGN(fbi->fb->fix.smem_len + PAGE_SIZE);
        /* Allocate the whole buffer size for both video */
             
#if DUAL_BUF
        _bg_mem_v  = dma_alloc_writecombine(fbi->dev, fbi->map_size,
                                            &_bg_mem_p, GFP_KERNEL);                              
	// off-screen buffer for dual buffer
        //printk("1*** _bg_mem_v 0x%x\n", _bg_mem_v);  
	//printk("1*** _bg_mem_p 0x%x\n", _bg_mem_p);
        if (_bg_mem_v == 0)
                return -ENOMEM;
#endif

#ifndef CONFIG_ASSIGN_FB_ADDR
        fbi->map_cpu  = dma_alloc_writecombine(fbi->dev, fbi->map_size, &fbi->map_dma, GFP_KERNEL);
	// on-screen buffer for singal/dual buffer buffer
        //printk("2*** fbi->map_cpu 0x%x\n", fbi->map_cpu);  
	//printk("2*** fbi->map_dma 0x%x\n", fbi->map_dma);
#else
	fbi->map_cpu = w55fa93_fb_v;
        fbi->map_dma = CONFIG_FRAME_BUFFER_ADDR;

	/*set frambuffer start phy addr*/
//	outl(CONFIG_FRAME_BUFFER_ADDR, REG_LCM_FSADDR);
	
	outl((inl(REG_LCM_LCDCInt)& ~0x0003), REG_LCM_LCDCInt);		// clear Vsync/Hsync sync Flags				
	while(1)
	{
		printk("REG_LCM_LCDCInt = 0x%x !!!\n", inl(REG_LCM_LCDCInt));				
		if (inl(REG_LCM_LCDCInt) & 0x02)	// wait VSync 
		{
			outl(CONFIG_FRAME_BUFFER_ADDR, REG_LCM_FSADDR);					
			outl((inl(REG_LCM_LCDCCtl)& 0xFFFEFFF0)|(0x10003), REG_LCM_LCDCCtl);	// change source flormat to RGB565	
			printk("Vsync flag is encountered !!!\n");
		//	while(1);
			break;
		}					
	}

	
#endif
        fbi->map_size = fbi->fb->fix.smem_len;

#if DUAL_BUF
        memcpy(_bg_mem_v, fbi->map_cpu, fbi->map_size); // make two buffer consistent

    	if( (inl(REG_AHBCLK) & EDMA0_CKE) ==0)
    	{
    		printk("EDMA Ch0 clock is off, turn it\n\r");	
    		outl(inl(REG_AHBCLK) | EDMA0_CKE, REG_AHBCLK);			      // enable EDMA Ch0 clock		
    	}	
	//w55fa93_edma_request(VDMA_CH,"w55fa93-VPOST");
        //w55fa93_edma_enable(VDMA_CH);
        _dma_len = fbi->map_size;                   // Transfer Size
        _dma_dst = fbi->map_dma;                    // Physical Destination Addr
#endif

        if (fbi->map_cpu) {
                /* prevent initial garbage on screen */
                //memset(fbi->map_cpu, 0xff, fbi->map_size);
#if DUAL_BUF
		// Dual Buffer
	        fbi->screen_dma		= _bg_mem_p;
	        fbi->fb->screen_base	= _bg_mem_v;
#else
    		// Single Buffer
    		fbi->screen_dma		= fbi->map_dma;
    		fbi->fb->screen_base	= fbi->map_cpu;
#endif
                fbi->fb->fix.smem_start  = fbi->screen_dma;
        } else {
#if DUAL_BUF
                dma_free_writecombine(fbi->dev, fbi->map_size, _bg_mem_v, _bg_mem_p);
#endif
	}


        /* video_buf_mmap is the LCD physical starting address, cpu is the virtual */
        video_cpu_mmap=(unsigned long)fbi->map_cpu;
        video_dma_mmap=(unsigned long)fbi->map_dma;
        video_buf_mmap=(unsigned long)fbi->map_size;
        //memset(fbi->map_cpu, 0x33, g_LCDWholeBuffer);

        return fbi->map_cpu ? 0 : -ENOMEM;
}

static inline void w55fa93fb_unmap_video_memory(struct w55fa93fb_info *fbi)
{
#ifndef CONFIG_ASSIGN_FB_ADDR 
	dma_free_writecombine(fbi->dev,/*g_LCDWholeBuffer*/fbi->map_size,fbi->map_cpu, fbi->map_dma);
#endif
}

/*
 * w55fa93fb_init_registers - Initialise all LCD-related registers
 */
static int w55fa93fb_init_registers(struct w55fa93fb_info *fbi)
{

	return w55fa93fb_init_device(fbi);

}	


void w55fa93fb_init_pwm(void)
{
#ifdef CONFIG_W55FA93_PWM_INIT
	/* Enable PWM clock */
	outl(inl(REG_APBCLK) | PWM_CKE, REG_APBCLK);

	/* Set divider to 1 */
	outl(0x4444, REG_PWM_CSR);
	
	/* Set all to Toggle mode  */
	outl(0x0C0C0C0C, REG_PCR);	

	// set PWM clock to 1MHz
	outl(w55fa93_apb_clock/1000 - 1, REG_PPR);
#endif
	w55fa93fb_init_pwm_device();
}

#define PWM_OUTPUT_FREQUENCY 500	
#define PWM_DEFAULT_OUTPUT_FREQUENCY 500

void w55fa93fb_set_CMR(unsigned int arg)
{
	unsigned int value;
#ifdef CONFIG_REVERSE_PWM
	value = g_PWMOutputDuty - arg + 1;
#else
	value = arg;
#endif
printk("PWM value = %d\n", value);
	if(value > g_PWMOutputDuty)
		value = g_PWMOutputDuty;
	else if(value < 1)
		value = 1;
	
	switch(w55fa93_pwm_channel)
	{
		case PWM0:
			outl(value, REG_CMR0);
			outl((inl(REG_PCR) | 0xD), REG_PCR);
			break;
		case PWM1:
			outl(value, REG_CMR1);
			outl((inl(REG_PCR) | 0xD00), REG_PCR);
			break;
		case PWM2:
			outl(value, REG_CMR2);
			outl((inl(REG_PCR) | 0xD0000), REG_PCR);
			break;
		case PWM3:
			outl(value, REG_CMR3);
			outl((inl(REG_PCR) | 0xD000000), REG_PCR);
			break;	
	}
}
void w55fa93fb_set_pwm_channel(unsigned int arg)
{
	w55fa93_pwm_channel = arg;
#ifdef CONFIG_W55FA93_PWM_INIT
	outl(arg, REG_POE);
	outl(arg, REG_PIER);

	switch(w55fa93_pwm_channel)
	{
		case PWM0:
			/* Enable Channel 0 */	
			outl(0x0808080D, REG_PCR);	
			/* Set Channel 0 Pin function */
			outl(((inl(REG_GPDFUN) & ~MF_GPD0) | 0x02), REG_GPDFUN);	
			/* PWM frequency should be between 100Hz and 1KHz */
			/* Set PWM Output frequency 500Hz */
			outl((1000000)/PWM_OUTPUT_FREQUENCY, REG_CNR0);  // duty 2000 ~ 1
			/* default PWM duty is full backlight */
#ifdef CONFIG_REVERSE_PWM
			outl(1, REG_CMR0);	
#else
			outl((1000000)/PWM_DEFAULT_OUTPUT_FREQUENCY, REG_CMR0);	
#endif
			/* Enable Channel 0 */	
			outl(0x0808080D, REG_PCR);				
			break;
		case PWM1:
			/* Enable Channel 1 */	
			outl(0x08080D08, REG_PCR);	
			/* Set Channel 1 Pin function */	
			outl(((inl(REG_GPDFUN) & ~MF_GPD1) | 0x08), REG_GPDFUN);
			/* PWM frequency should be between 100Hz and 1KHz */
			/* Set PWM Output frequency 500Hz */
			outl((1000000)/PWM_OUTPUT_FREQUENCY, REG_CNR1);  // duty 2000 ~ 1
			/* default PWM duty is full backlight */
#ifdef CONFIG_REVERSE_PWM
			outl(1, REG_CMR0);	
#else
			outl((1000000)/PWM_DEFAULT_OUTPUT_FREQUENCY, REG_CMR1);	
#endif
			/* Enable Channel 1 */	
			outl(0x08080D08, REG_PCR);
			break;
		case PWM2:
			/* Enable Channel 2 */	
			outl(0x080D0808, REG_PCR);	
			/* Set Channel Pin function */
			outl(((inl(REG_GPDFUN) & ~MF_GPD2) | 0x20), REG_GPDFUN);
			/* PWM frequency should be between 100Hz and 1KHz */
			/* Set PWM Output frequency 500Hz */
#ifdef CONFIG_REVERSE_PWM
			outl(1, REG_CMR0);	
#else
			outl((1000000)/PWM_OUTPUT_FREQUENCY, REG_CNR2);  // duty 2000 ~ 1
#endif
			/* default PWM duty is full backlight */
			outl((1000000)/PWM_DEFAULT_OUTPUT_FREQUENCY, REG_CMR2);	
			break;
		case PWM3:
			/* Enable Channel 3 */	
			outl(0x0D080808, REG_PCR);	
			/* Set Channel 3 Pin function */
			outl(((inl(REG_GPDFUN) & ~MF_GPD3) | 0x80), REG_GPDFUN);	
			/* PWM frequency should be between 100Hz and 1KHz */
			/* Set PWM Output frequency 500Hz */
			outl((1000000)/PWM_OUTPUT_FREQUENCY, REG_CNR3);  // duty 2000 ~ 1
			/* default PWM duty is full backlight */
#ifdef CONFIG_REVERSE_PWM
			outl(1, REG_CMR0);	
#else
			outl((1000000)/PWM_DEFAULT_OUTPUT_FREQUENCY, REG_CMR3);	
#endif
			/* Enable Channel 3 */	
			outl(0x0D080808, REG_PCR);	
			break;
	}
#endif
	g_PWMOutputDuty = (1000000)/PWM_OUTPUT_FREQUENCY;
	//printk("LCD PWM Output Duty = 0x%x\r\n", g_PWMOutputDuty);
}

static irqreturn_t w55fa93fb_irq(int irq, void *dev_id, struct pt_regs *regs)
{
        int ret;
        unsigned long lcdirq = inl(REG_LCM_LCDCInt);
#ifdef CONFIG_W55FA93_VIDEOIN	
	if (bIsVideoInEnable==1)		
	{
		 outl(w55fa93_VIN_PAC_BUFFER, REG_LCM_FSADDR);
		if (! _auto_disable)
               		//outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00), REG_LCM_LCDCInt);
                	outl((inl(REG_LCM_LCDCInt) & 0xFFFEFF00), REG_LCM_LCDCInt);
        	else
              		outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) & ~0x20000, REG_LCM_LCDCInt); // disable VIN
        		return IRQ_HANDLED;
	}
#endif
	
//	printk("Enter LCD Int Status = 0x%x\n", lcdirq);

#if TV_OUTPUT_DEVICE
        //if ((lcdirq & 0x4) && (inl(REG_LCM_TVCtl) & 0x80000000)){
        if (lcdirq & 0x4){
#else
        if (lcdirq & 0x2) {
#endif            
#if DUAL_BUF
                // trigger VDMA to copy off-screen frame buffer to on-screen frame buffer
	        ret = w55fa93_edma_request(VDMA_CH,"w55fa93-VPOST");
       	    	if (ret < 0) {
        			//printk("Request VDMA fail.\n");
        			goto VDMA_EXIT;
	        }
		else
		{
    	            	ret = w55fa93_edma_setup_single(VDMA_CH, _bg_mem_p, _dma_dst, _dma_len);
    	        	if (ret < 0) {
    	        		printk("w55fa93_edma_setup_single failed and returns %d\n", ret);
    	        		goto VDMA_FREE;
    	        	}
#ifdef EDMA_USE_IRQ
			ret = w55fa93_edma_setup_handlers(0, 2, fb_edma_irq_handler, NULL);
    	        	if (ret < 0) {
    	        		printk("w55fa93_edma_setup_handlers failed and returns %d\n", ret);
    	        		goto VDMA_FREE;
    	        	}
#endif
			flush_cache_all();
    			//w55fa93_edma_enable(VDMA_CH);
    			w55fa93_edma_trigger(VDMA_CH);
#ifdef EDMA_USE_IRQ
        		goto VDMA_EXIT;
#else
	                while (DrvEDMA_IsCHBusy(0))
	                        ;
    			//w55fa93_edma_disable(VDMA_CH);
    			w55fa93_edma_trigger_done(VDMA_CH);
#endif
    		}
VDMA_FREE:
   	   w55fa93_edma_free(VDMA_CH);  
	} 
#else 
	}
VDMA_FREE:
#endif
 
VDMA_EXIT:

	//debug VPOST register
	//ret1 = inl(REG_LCM_LCDCInt);
	//if (ret1 == 0xFFFFFFFF)
	//  printk("Read Status error = 0x%x\n",ret1);


//        if (! _auto_disable)
                //outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00), REG_LCM_LCDCInt);
//                outl((inl(REG_LCM_LCDCInt) & 0xFFFEFF00), REG_LCM_LCDCInt);
//        else
//                outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) & ~0x20000, REG_LCM_LCDCInt); // disable VIN

        if (! _auto_disable)	//### Chris
        {	
//        		printk("### Enable VIN: lcdirq=0x%x, disable=%d###\n",lcdirq,_auto_disable);	//## Chris        	
#if TV_OUTPUT_DEVICE        	
                outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) | 0x40000, REG_LCM_LCDCInt);
#else
               outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) | 0x20000, REG_LCM_LCDCInt);
#endif                
        }else{
//        		printk("### Disable VIN: lcdirq=0x%x, disable=%d###\n",lcdirq,_auto_disable);	//## Chris
#if TV_OUTPUT_DEVICE
                outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) & ~0x40000, REG_LCM_LCDCInt); // disable VIN
#else
                outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) & ~0x20000, REG_LCM_LCDCInt); // disable VIN
#endif                
		}


        return IRQ_HANDLED;
}
static char driver_name[] = "w55fa93fb";

static int __init w55fa93fb_probe(struct platform_device *pdev)
{
	struct w55fa93fb_info *info;
	struct fb_info	   *fbinfo;
	static struct w55fa93fb_mach_info *mach_info;
	struct w55fa93fb_hw *mregs;	
	unsigned long page; /* For LCD page reserved */
	int ret;
	int irq;
	int i;
	
	printk("###########w55fa93 frame buffer probe############\n");


#ifdef CONFIG_W55FA93_FB_INIT
  	// 2.Enable IP!|s clock
	outl(inl(REG_AHBCLK) | 0x08000000, REG_AHBCLK);
  	// 3.Reset IP
	outl(inl(REG_AHBIPRST) | 0x00000400, REG_AHBIPRST);
	outl(inl(REG_AHBIPRST) & (~0x00000400), REG_AHBIPRST);
	outl(inl(REG_AHBCLK) | 0x08000000, REG_AHBCLK);
	
	
	outl((inl(REG_LCM_LCDCInt) & 0xFFFFFF00) | 0x20000, REG_LCM_LCDCInt); // enable VIN /* SW add for bg/fp */
  	// 4.Configure IP according to inputted arguments.
	//outl((inl(REG_CLKSEL) & ~(0x10000000)) | ((0x1 & 0x1) << 28), REG_CLKSEL);   // 27 MHz source
	//outl((inl(REG_CLKSEL) & ~(BIT(20,16))) | ((0x5 & 0x1F) << 16), REG_CLKSEL);  // 27 / 6 = 4.5 Mhz 
	//outl((inl(REG_CLKSEL) & ~(0x10000000)) | ((0x1 & 0x1) << 30), REG_CLKSEL);   // PLL 240 Mhz => AHB = 120 Mhz
	//outl((inl(REG_CLKSEL) & ~(BIT(20,16))) | ((0xB & 0x1F) << 16), REG_CLKSEL);   // 60 / 12 = 5 Mhz  ??
  	// 5.Enable IP I/O pins
	//outl((inl(REG_PINFUN) | 0x00000060 ), REG_PINFUN);
	/*Open end */	
#endif

  	mach_info = pdev->dev.platform_data;
	if (mach_info == NULL) {
		dev_err(&pdev->dev,"no platform data for lcd, cannot attach\n");
		return -EINVAL;
	}
	
	mregs = &mach_info->regs;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0){
		dev_err(&pdev->dev, "no irq for device\n");
		return -ENOENT;
	}

	fbinfo = framebuffer_alloc(sizeof(struct w55fa93fb_info), &pdev->dev);
	if (!fbinfo){
		return -ENOMEM;
	}
	
	info = fbinfo->par;
	info->fb = fbinfo;
	platform_set_drvdata(pdev, fbinfo);


	strcpy(fbinfo->fix.id, driver_name);

	memcpy(&info->regs, &mach_info->regs, sizeof(info->regs));
	
	info->mach_info		  		  	= pdev->dev.platform_data;

	fbinfo->fix.type	   				= FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.type_aux	    	= 0;
	fbinfo->fix.xpanstep	    	= 0;
	fbinfo->fix.ypanstep	    	= 0;
	fbinfo->fix.ywrapstep	    	= 0;
	fbinfo->fix.accel	    			= FB_ACCEL_NONE;

	fbinfo->var.nonstd	   			= 0;
	fbinfo->var.activate	    	= FB_ACTIVATE_NOW;
	fbinfo->var.height	    		= mach_info->height;
	fbinfo->var.width	    			= mach_info->width;
	fbinfo->var.accel_flags   	= 0;
	fbinfo->var.vmode	   		 		= FB_VMODE_NONINTERLACED;

	fbinfo->fbops		    				= &w55fa93fb_ops;
	fbinfo->flags		    				= FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette  	  = &info->pseudo_pal;

	fbinfo->var.xres	    			= mach_info->xres.defval;
	fbinfo->var.xres_virtual  	= mach_info->xres.defval;
	fbinfo->var.yres	    			= mach_info->yres.defval;
	fbinfo->var.yres_virtual  	= mach_info->yres.defval;
	fbinfo->var.bits_per_pixel	= mach_info->bpp.defval;

	fbinfo->var.upper_margin    = mach_info->upper_margin;
	fbinfo->var.lower_margin    = mach_info->lower_margin;
	fbinfo->var.vsync_len	    	= mach_info->vsync_len;

	fbinfo->var.left_margin	    = mach_info->left_margin;
	fbinfo->var.right_margin    = mach_info->right_margin;
	fbinfo->var.hsync_len	    	= mach_info->hsync_len;

	fbinfo->var.red.offset      = 11;
	fbinfo->var.green.offset    = 5;
	fbinfo->var.blue.offset     = 0;
	fbinfo->var.transp.offset   = 0;
	fbinfo->var.red.length      = 5;
	fbinfo->var.green.length    = 6;
	fbinfo->var.blue.length     = 5;
	fbinfo->var.transp.length   = 0;
	fbinfo->fix.smem_len        =	mach_info->xres.max *
				      mach_info->yres.max *
				      mach_info->bpp.max / 8;
					
	video_alloc_len = fbinfo->fix.smem_len;
	
	for (i = 0; i < 256; i++)
		info->palette_buffer[i] = PALETTE_BUFF_CLEAR;

	if (!request_mem_region((unsigned long)W55FA93_VA_VPOST, SZ_4K, "w55fa93-lcd")){
		ret = -EBUSY;
		goto dealloc_fb;
	}
	
#if DUAL_BUF  
        outl(inl(REG_AHBCLK) | EDMA0_CKE, REG_AHBCLK);                  // enable EDMA clock
#endif        
	
	//ret = request_irq(irq, w55fa93fb_irq, 0, pdev->name, info);
	 ret = request_irq(irq, w55fa93fb_irq, SA_INTERRUPT, pdev->name, info);
	if (ret) {
		dev_err(&pdev->dev, "cannot get irq %d - err %d\n", irq, ret);
		ret = -EBUSY;
		goto release_mem;
	}

	msleep(1);

	/* Initialize video memory */
	ret = w55fa93fb_map_video_memory(info);
	if (ret) {
		ret = -ENOMEM;

	}
	/* video & osd buffer together */	
	for (page = (unsigned long)video_cpu_mmap; 
		       page < PAGE_ALIGN((unsigned long)ret + video_buf_mmap/*g_LCDWholeBuffer*/);
		       page += PAGE_SIZE){
           SetPageReserved(virt_to_page(page));
	 }	
	
#ifdef CONFIG_W55FA93_FB_INIT
	ret = w55fa93fb_init_registers(info);
#else
	//w55fa93fb_set_lcdaddr(info);
#endif
#ifdef 	CONFIG_W55FA93_SETPWM
	w55fa93fb_init_pwm();
#endif

	ret = w55fa93fb_check_var(&fbinfo->var, fbinfo);
	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register framebuffer device: %d\n", ret);
		goto free_video_memory;
	}

    w55fa93fb_probe_device();

	/* create device files */
	//device_create_file(&pdev->dev, &dev_attr_debug);

	w55fa93fb_set_lcdaddr(info);
	printk("w55fa93 LCD driver has been installed successfully\n");

	return 0;

free_video_memory:
	w55fa93fb_unmap_video_memory(info);
release_mem:
 	release_mem_region((unsigned long)W55FA93_VA_VPOST, W55FA93_SZ_VPOST);
dealloc_fb:
	framebuffer_release(fbinfo);
	return ret;

	
} 

/* 
 *			w55fa93fb_stop_lcd
 *
 * 			shutdown the lcd controller
 */
static void w55fa93fb_stop_lcd(void)
{
	unsigned long flags;

	local_irq_save(flags);

	w55fa93fb_stop_device();

	local_irq_restore(flags);
}

/*
 *  Cleanup
 */
static int w55fa93fb_remove(struct platform_device *pdev)
{
	struct fb_info *fbinfo = platform_get_drvdata(pdev);
	struct w55fa93fb_info *info = fbinfo->par;
	int irq;

	w55fa93fb_stop_lcd();
	msleep(1);

	w55fa93fb_unmap_video_memory(info);

	irq = platform_get_irq(pdev, 0);
	free_irq(irq,info);
	release_mem_region((unsigned long)W55FA93_VA_VPOST, W55FA93_SZ_VPOST);
	unregister_framebuffer(fbinfo);

	return 0;
}

#ifdef CONFIG_PM

/* suspend and resume support for the lcd controller */

static int w55fa93fb_suspend(struct platform_device *dev, pm_message_t state)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct w55fa93fb_info *info = fbinfo->par;

	w55fa93fb_stop_lcd();
	msleep(1);
	clk_disable(info->clk);
	return 0;
}

static int w55fa93fb_resume(struct platform_device *dev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct w55fa93fb_info *info = fbinfo->par;

	clk_enable(info->clk);
	msleep(1);

	w55fa93fb_init_registers(info);

	return 0;
}

#else
#define w55fa93fb_suspend NULL
#define w55fa93fb_resume  NULL
#endif
static struct platform_driver w55fa93fb_driver = {
	.probe		= w55fa93fb_probe,
	.remove		= w55fa93fb_remove,
	.suspend	= w55fa93fb_suspend,
	.resume		= w55fa93fb_resume,
	.driver		= {
		.name	= "w55fa93-lcd",
		.owner	= THIS_MODULE,
	},
};

int __devinit w55fa93fb_init(void)
{
	/*set up w55fa93 register*/
	printk("---w55fa93fb_init ----w55fa93 frame buffer init \n");
	w55fa93_fb_set_platdata(&w55fa93_lcd_platdata);
	return platform_driver_register(&w55fa93fb_driver);
}

static void __exit w55fa93fb_cleanup(void)
{
	platform_driver_unregister(&w55fa93fb_driver);
}

module_init(w55fa93fb_init);
module_exit(w55fa93fb_cleanup);

MODULE_DESCRIPTION("Framebuffer driver for the W55FA93");
MODULE_LICENSE("GPL"); 
