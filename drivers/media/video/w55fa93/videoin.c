/* videoin.c
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


#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/videodev.h>
#include <linux/jiffies.h>

#include <asm/io.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/fb.h>
#include <asm/arch/w55fa93_fb.h>
#include <asm/arch/videoin.h>
#include <asm/arch/DrvVideoin.h>

#include "videoinpriv.h"

//#define LCDWIDTH	480
//#define LCDHEIGHT	272
//#define LCDBPP		16
//IMPORT_SYMBOL(w55fa93_FB_BG_PHY_ADDR);

typedef struct reg_struct_t
{
    char address;
    char val;
}reg_struct_t;



extern unsigned int w55fa93_FB_BG_PHY_ADDR;
extern unsigned int w55fa93_upll_clock, w55fa93_apll_clock, w55fa93_ahb_clock;

unsigned int w55fa93_JPG_Y_ADDR = 0;
unsigned int w55fa93_JPG_U_ADDR = 0;
unsigned int w55fa93_JPG_V_ADDR = 0;

unsigned int w55fa93_JPG_Y0_ADDR = 0;
unsigned int w55fa93_JPG_U0_ADDR = 0;
unsigned int w55fa93_JPG_V0_ADDR = 0;

unsigned int w55fa93_JPG_Y1_ADDR = 0;
unsigned int w55fa93_JPG_U1_ADDR = 0;
unsigned int w55fa93_JPG_V1_ADDR = 0;
unsigned int bDumpframeBuffer;

spinlock_t spin_vin_opc = SPIN_LOCK_UNLOCKED;

EXPORT_SYMBOL(w55fa93_JPG_Y_ADDR);
EXPORT_SYMBOL(w55fa93_JPG_U_ADDR);
EXPORT_SYMBOL(w55fa93_JPG_V_ADDR);

#ifndef CONFIG_FSC
unsigned int bIsVideoInEnable;
unsigned int w55fa93_VIN_PAC_BUFFER;
EXPORT_SYMBOL(bIsVideoInEnable);
EXPORT_SYMBOL(w55fa93_VIN_PAC_BUFFER);
#endif

#define VIDEOIN_ENCODE_BUFFER_SIZE	(640*480*2)	//Max support 640*480*2
//#define VIDEOIN_ENCODE_BUFFER_SIZE	(320*240*2)	//Max support 640*480*2

#define VIDEOIN_BUFFER_SIZE	(CONFIG_VIDEOIN_PREVIEW_BUFFER_SIZE * CONFIG_VIDEOIN_BUFFER_COUNT)

static DECLARE_WAIT_QUEUE_HEAD(videoin_wq);
static int videoin_nr = -1;
module_param(videoin_nr, int, 1);

/* Global variable */
videoin_priv_t 		videoin_priv;
videoin_window_t 	tViewwindow;
videoin_encode_t		tEncode;

unsigned int u32PhysPacketBuf;
unsigned int u32VirtPacketBuf;

unsigned int g_u32FbPhyAddr;

#ifdef CONFIG_SENSOR_OV9660
	__u32 u32Sensor = OV_9660;
#elif defined  CONFIG_SENSOR_OV7670
	__u32 u32Sensor = OV_7670;
#else
# error "please select one sensor"
#endif

//#define DBG_PRINTF	printk
#define DBG_PRINTF(...)

#define ERR_PRINTF		printk

#define outp32(addr, value)	outl(value, addr)
#define inp32(addr)			inl(addr)

/* Packet Frame Buffer Pointer x3, Planar Frame Byffer Pointer x2 */						   	
videoIn_buf_t videoIn_buf[5]={	{0,0},    	//Packet Buffer 0
						   	{0,0},   	//Packet Buffer 1	
							{0,0}, 	//Packet Buffer 2
							{0,0}, 	//Planar Buffer 0
							{0,0}};	//Planar Buffer 1	

/* Packet Frame Buffer with Offset. The value Packet Buffer Address plus offset if VPOST and VindeIn using Different Buffer Address */
unsigned int vinPackAddr0,  vinPackAddr1, vinPackAddr2; 		
 
unsigned long videoin_dmamalloc_phy(unsigned int u32Buf, unsigned long size)
{
	videoin_priv_t *priv = (videoin_priv_t *)&videoin_priv;
	void *mem;
	unsigned long adr;

	DBG_PRINTF("%s\n",__FUNCTION__);	
	size = PAGE_ALIGN(size);	
	priv->vaddr = dma_alloc_writecombine(NULL/*dev*/, 
									size,&priv->paddr, 
									GFP_KERNEL);	
	printk("videoin priv->paddr=%x,priv->vaddr=%x\n", priv->paddr, priv->vaddr);
	if (!priv->vaddr)
		return NULL;

	adr = (unsigned long) priv->vaddr;
	videoIn_buf[u32Buf].u32PhysAddr = priv->paddr;
	if(u32Buf<3)
		videoIn_buf[u32Buf+5].u32PhysAddr;
	videoIn_buf[u32Buf].u32VirtAddr = adr;
	while (size > 0) {
		SetPageReserved(vmalloc_to_page((void *)adr));		
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;		
	}
	DBG_PRINTF("SetPageReserved = 0x%x\n", adr);
	return priv->paddr;
}
void videoin_free_dmamalloc_phy(void)
{
	UINT32 u32Idx;
	for(u32Idx=0; u32Idx<3; u32Idx=u32Idx+1)
	{
		if(videoIn_buf[u32Idx].u32PhysAddr!=0)
			dma_free_writecombine(NULL/*dev*/,
								 	LCDWIDTH*LCDHEIGHT*LCDBPP/8, 
									videoIn_buf[u32Idx].u32VirtAddr, 
									videoIn_buf[u32Idx].u32PhysAddr);
	}
	for(u32Idx=3; u32Idx<5; u32Idx=u32Idx+1)
	{
		if(videoIn_buf[u32Idx].u32PhysAddr!=0)
			dma_free_writecombine(NULL/*dev*/, 
									VIDEOIN_ENCODE_BUFFER_SIZE, 
									videoIn_buf[u32Idx].u32VirtAddr, 
									videoIn_buf[u32Idx].u32PhysAddr);
	}
}
/*---------------------------------------------------------------------------------------------------------
	Memory management functions 
	Allocate the packet buffer for previewing for Display or JPEG encoding source
	Size depends on the (CONFIG_VIDEOIN_BUFFER_SIZE * CONFIG_VIDEOIN_BUFFER_COUNT)
	CONFIG_VIDEOIN_BUFFER_SIZE was set in menuconfig
	CONFIG_VIDEOIN_BUFFER_COUNT was defined in videoin.h
---------------------------------------------------------------------------------------------------------*/
static void *videoin_dmamalloc(unsigned long size)
{
	videoin_priv_t *priv = (videoin_priv_t *)&videoin_priv;
	void *mem;
	unsigned long adr;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	size = PAGE_ALIGN(size);
	
	priv->vaddr = dma_alloc_writecombine(NULL/*dev*/, 
									size,&priv->paddr, 
									GFP_KERNEL);	
	DBG_PRINTF("videoin priv->paddr=%x,priv->vaddr=%x\n", priv->paddr, priv->vaddr);
	if (!priv->vaddr)
		return NULL;

	memset(priv->vaddr, 0, size); /* Clear the ram out, no junk to the user */
	adr = (unsigned long) priv->vaddr;
	while (size > 0) {
		SetPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	return priv->vaddr;
}

static int i32OpenCount = 0;
static int videoin_open(struct inode *inode, struct file *file)
{
	struct video_device *dev = video_devdata(file);
	videoin_priv_t *priv = (videoin_priv_t *)dev->priv;	
	volatile int  loop;
	int ret = 0, buf_num;
	
	DBG_PRINTF("%s\n",__FUNCTION__);	
	
	for(buf_num=0; buf_num<=2; buf_num=buf_num+1)
	{//Clear packet buffer, Black in YUV422 is Y=0x0, U=V=0x80 
		unsigned int* pu32Addr =  videoIn_buf[buf_num].u32VirtAddr;							
		unsigned int i;
		for(i=0; i<LCDWIDTH*LCDHEIGHT*LCDBPP/8;i=i+4)
		{
			*pu32Addr++=0x80008000; //2 Pixel
		}					
	}
	w55fa93_VIN_PAC_BUFFER = videoIn_buf[0].u32PhysAddr;	
	bIsVideoInEnable = 1;	/* Important !!! to enable VPOST show video buffer */	
	

//Critical section 
	spin_lock(&spin_vin_opc);
	if(i32OpenCount==0)
	{
		i32OpenCount = 1;
		spin_unlock(&spin_vin_opc);
		DrvVideoIn_EnableInt(eDRVVIDEOIN_VINT);		
		return 0;
	}
	else
	{
		spin_unlock(&spin_vin_opc);
		return -1;
	}
}
static int videoin_close(struct inode *inode, struct file *file)
{
	struct video_device *dev = video_devdata(file);
	videoin_priv_t *priv = (videoin_priv_t *)dev->priv;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	if((inp32(REG_VPECTL)&(PKEN|VPEEN))==(PKEN|VPEEN))
	{//
		DrvVideoIn_SetOperationMode(TRUE);			//One shutter mode
		while(DrvVideoIn_GetOperationMode()==TRUE);		
	}
	DrvVideoIn_SetPipeEnable(TRUE, eDRVVIDEOIN_BOTH_PIPE_DISABLE);
	DrvVideoIn_DisableInt(eDRVVIDEOIN_VINT);
	DrvVideoIn_SetPipeEnable(FALSE, eDRVVIDEOIN_BOTH_PIPE_DISABLE);
	DrvVideoIn_DisableInt(eDRVVIDEOIN_VINT);

	spin_lock(&spin_vin_opc);
	if(i32OpenCount==1)
		i32OpenCount = 0;
	spin_unlock(&spin_vin_opc);
#ifndef CONFIG_FSC	
	bIsVideoInEnable = 0;
	outp32(REG_LCM_FSADDR, g_u32FbPhyAddr);
#endif
}
/*
* read data after encode/decode finished
*/
static ssize_t videoin_read(struct file *file, char __user *buf,
		      size_t count, loff_t *ppos)
{
	struct video_device *dev = video_devdata(file);
	videoin_priv_t *priv = (videoin_priv_t *)dev->priv;

	int nonblock = file->f_flags & O_NONBLOCK;
	int ret;
	int size, index;
	videoin_info_t videoininfo;

	DBG_PRINTF("%s\n",__FUNCTION__);	
 
	if(bDumpframeBuffer==0)
	{//Preview pipe
		size = priv->preview_height * priv->preview_width * LCDBPP/8;
		DBG_PRINTF("Packet W*H = %d * %d \n", priv->preview_width , priv->preview_height);
		priv->vaddr = videoIn_buf[0].u32VirtAddr;	
	}
	else
	{//Encode pipe	
		priv->vaddr = videoIn_buf[3].u32VirtAddr;
		if(tEncode.u32Format & 1)
			size = tEncode.u32Width * tEncode.u32Height  * 3/2;
		else
			size = tEncode.u32Width * tEncode.u32Height * 2;		
		DBG_PRINTF("Planar format %s,  W*H = %d * %d \n", (tEncode.u32Format&1) ? "YUV420":"YUV422",  
					tEncode.u32Width , tEncode.u32Height);
	}

	down(&priv->lock);
	if (size >= count)
		size = count;
		
	if (priv->videoin_bufferend == 0)
		index = CONFIG_VIDEOIN_BUFFER_COUNT - 1;
	else
		index = priv->videoin_bufferend - 1;

	DBG_PRINTF("index = 0x%x\n", index);
	DBG_PRINTF("Dst buf addr = 0x%x\n", (UINT32)buf);
	DBG_PRINTF("Dst buf addr = 0x%x\n", (UINT32)(priv->vaddr + index * CONFIG_VIDEOIN_PREVIEW_BUFFER_SIZE));
	if (copy_to_user(buf, priv->vaddr + index * CONFIG_VIDEOIN_PREVIEW_BUFFER_SIZE, size)) 
	{
		ERR_PRINTF("videoin_read copy_to_user error\n");
		ret = -EFAULT;
		goto out;
	}
	*ppos += size;
	
	DBG_PRINTF("*ppos = %d\n", *ppos);

	up(&priv->lock);
	
	ret = size;
out:
	return ret;
}

static int videoin_mmap (struct file *file, struct vm_area_struct *vma)
{	
	struct video_device *dev = video_devdata(file);
	videoin_priv_t *priv = (videoin_priv_t *)dev->priv;
	unsigned long start = vma->vm_start;
	unsigned long size  = vma->vm_end-vma->vm_start;
	unsigned long page, pos;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	DBG_PRINTF("start = 0x%x\n",start);
	DBG_PRINTF("size = 0x%x\n",size);
	
	if(bDumpframeBuffer==0)	
		pos = videoIn_buf[0].u32VirtAddr;	
	else
		pos = videoIn_buf[3].u32VirtAddr;
	priv->mmap_bufsize = size;
	while (size > 0) 
	{
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED))
		{
			ERR_PRINTF("remap error\n");
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}
	return 0;
	
}

//mls@dev03 define ioctl to set contrast and brightness for OV76xx
#define VIDEOIN_SENSOROV76xx_SETCONTRAST _IOW('v',141, __u32)
#define VIDEOIN_SENSOROV76xx_SETBRIGHTNESS _IOW('v',142, __u32)
#define VIDEOIN_SENSOROV76xx_CHECKPID _IOR('v',143, __u32)
//mlsdev008 define ioctl to implement flip up
#define VIDEOIN_SENSOROV76xx_FLIPUP	_IOW('v',144,__u32)

#define VIDEOIN_SENSOROV76xx_IRLED	_IOW('v',145,__u32)

#define VIDEOIN_SENSOROV76xx_SETREG	_IOW('v',146,__u32)
#define VIDEOIN_SENSOROV76xx_GETREG	_IOW('v',147,__u32)
#define VIDEOIN_SENSOROV76xx_CHECKFLIPUP	_IOW('v',148,__u32)

static int videoin_ioctl(struct inode *inode, struct file *file,
				 unsigned int cmd, void *arg)
{
	char value,address;
	reg_struct_t reg_struct;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	DBG_PRINTF("cmd = %d\n",cmd);	
	DBG_PRINTF("videoin ioctl, cmd = %d\n", cmd);
	
	switch (cmd) {	
		case VIDEOIN_PREVIEW_PIPE_CTL:						
			vin_ioctl_preview_ctl(inode, 
							file,
				 			cmd, 
							arg);	
			break;
		case VIDEOIN_ENCODE_PIPE_CTL:
			DBG_PRINTF("Preview start\n");	
			vin_ioctl_encode_ctl(inode, 
							file,
				 			cmd, 
							arg);
			break;	
		case VIDEOIN_S_PARAM:
			vin_ioctl_s_para(inode, 
							file,
				 			cmd, 
							arg);	
			break;
		case VIDEOIN_S_VIEW_WINDOW:
			vin_ioctl_s_view_window(inode, 
							file,
				 			cmd, 
							arg);				
			
		case VIDEOIN_S_JPG_PARAM:
			 vin_ioctl_s_jpg_para(inode, 
							file,
				 			cmd, 
							arg);	
			break;
		case VIDEOIN_SELECT_FRAME_BUFFER:
			vin_ioctl_select_frame_buffer(inode, 
							file,
				 			cmd, 
							arg);
			break;

		//mls@dev03 set contrast and brightness for sensor OV76xx
		case VIDEOIN_SENSOROV76xx_SETCONTRAST:
			mlsSensorOV76xxSetContrast(arg);
			break;
		case VIDEOIN_SENSOROV76xx_SETBRIGHTNESS:
			mlsSensorOV76xxSetBrightness(arg);
			break;
		//mls@dev03 20111021 check PID to check camera and i2c communication
		case VIDEOIN_SENSOROV76xx_CHECKPID:
			mlsSensorOV76xxCheckPID(&value);
			put_user(value,(char*)arg);
			break;
		case VIDEOIN_SENSOROV76xx_FLIPUP:
			printk("execute VIDEOIN_SENSOROV76xx_FLIPUP in videoin.c\n");
			mlsSensorOV76xxFlipup();
			break;
		
		case VIDEOIN_SENSOROV76xx_CHECKFLIPUP:
			value = mlsSensorOV76xxCheckFlipup();
			put_user(value, (char*) arg);
			break;
		
		case VIDEOIN_SENSOROV76xx_IRLED:
			
			mlsSensorOV76XXSetIR(arg);
			break;	
		
		case VIDEOIN_SENSOROV76xx_SETREG:
			copy_from_user(&reg_struct,(reg_struct_t*) arg, sizeof(reg_struct_t));
			mlsSensorOV76XXSetReg(reg_struct.address,reg_struct.val);
			printk("Sensor SetReg addr = %02X, value = %02X\n",reg_struct.address,reg_struct.val);
			break;	
		
		case VIDEOIN_SENSOROV76xx_GETREG:
			copy_from_user(&reg_struct,(reg_struct_t*) arg, sizeof(reg_struct_t));
			mlsSensorOV76XXGetReg(reg_struct.address,&reg_struct.val);
			printk("Sensor GetReg addr = %02X, value = %02X\n",reg_struct.address,reg_struct.val);
			copy_to_user((reg_struct_t*) arg, &reg_struct,sizeof(reg_struct_t));
			break;	
			
			
		default:
			return -ENOIOCTLCMD;
	}
	return 0;
}

static struct file_operations videoin_fops = {
	.owner =  THIS_MODULE,
	.open =   videoin_open,
	.release =videoin_close,
	.read =   videoin_read,
	.mmap =   videoin_mmap,
	.ioctl =  videoin_ioctl,
	.llseek = no_llseek,
};

#ifdef CONFIG_PM

/* suspend and resume support for the lcd controller */

static int videoin_suspend(struct platform_device *dev, pm_message_t state)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct w90x900fb_info *info = fbinfo->par;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	w90x900fb_stop_lcd();
	msleep(1);
	clk_disable(info->clk);
	return 0;
}

static int videoin_resume(struct platform_device *dev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct w90x900fb_info *info = fbinfo->par;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	clk_enable(info->clk);
	msleep(1);

	w90x900fb_init_registers(info);
	return 0;
}

#else
#define videoinc_suspend NULL
#define videoin_resume  NULL
#endif
static int vincap, first;
volatile u64 irqbegin;
	//curtime = jiffies;

unsigned int u32FrameNumber=0; 
unsigned int videoinFrameCount(void)
{
	return u32FrameNumber;
}
static irqreturn_t irq_handler(int irq, void *dev_id, struct pt_regs *r)
{
	UINT32 u32IntStatus;
	u32FrameNumber = u32FrameNumber+1;
/* Useless to fix FSC bug
	if(videoIn_buf[0].u32PhysAddr != videoIn_buf[5].u32PhysAddr)
	{//Fix FSC bug
		if( inp32(REG_FSC0_WBUF) == videoIn_buf[0].u32PhysAddr)
		{	
			DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PACKET, 0, videoIn_buf[5].u32PhysAddr);
		}
		else if ( inp32(REG_FSC0_WBUF) == videoIn_buf[1].u32PhysAddr)
		{
			DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PACKET, 1, videoIn_buf[6].u32PhysAddr);
		}
		else if (  inp32(REG_FSC0_WBUF) == videoIn_buf[2].u32PhysAddr)
		{
			DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PACKET, 2, videoIn_buf[7].u32PhysAddr);
		}
	}
*/ 	
#ifndef CONFIG_FSC
	if(inp32( REG_VPECTL ) & PKEN)
	{//Packet pipe enable 
		if( inp32(REG_PACBA0) == vinPackAddr0)
		{	
			DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PACKET, 0, vinPackAddr1);
			w55fa93_VIN_PAC_BUFFER = videoIn_buf[0].u32PhysAddr;
		}
		else if ( inp32(REG_PACBA0) == vinPackAddr1)
		{
			DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PACKET, 0, vinPackAddr2);
			w55fa93_VIN_PAC_BUFFER = videoIn_buf[1].u32PhysAddr;
		}
		else if (  inp32(REG_PACBA0) == vinPackAddr2)
		{
			DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PACKET, 0, vinPackAddr0);
			w55fa93_VIN_PAC_BUFFER = videoIn_buf[2].u32PhysAddr;
		}
	}	
#endif	
	if(inp32(REG_YBA0) == w55fa93_JPG_Y0_ADDR )
	{//Current use YUV 0. Switch to another one buffer
		DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 0, w55fa93_JPG_Y1_ADDR);		
		DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 1, w55fa93_JPG_U1_ADDR);	
		DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 2, w55fa93_JPG_V1_ADDR);	
		//Buffer 0 is valid for JEPG
		w55fa93_JPG_Y_ADDR = w55fa93_JPG_Y0_ADDR;	
		w55fa93_JPG_U_ADDR = w55fa93_JPG_U0_ADDR;
		w55fa93_JPG_V_ADDR = w55fa93_JPG_V0_ADDR;
	}
	else
	{
		DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 0, w55fa93_JPG_Y0_ADDR);		
		DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 1, w55fa93_JPG_U0_ADDR);	
		DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 2, w55fa93_JPG_V0_ADDR);	
		//Buffer 1 is valid for JEPG
		w55fa93_JPG_Y_ADDR = w55fa93_JPG_Y1_ADDR;
		w55fa93_JPG_U_ADDR = w55fa93_JPG_U1_ADDR;
		w55fa93_JPG_V_ADDR = w55fa93_JPG_V1_ADDR;
	}
	DrvVideoIn_SetShadowRegister();

	u32IntStatus = inp32(REG_VPEINT);
	if( (u32IntStatus & (VINTEN | VINT)) == (VINTEN | VINT))
		outp32(REG_VPEINT, (u32IntStatus & ~(MDINT | ADDRMINT | MEINT)));			/* Clear Frame end interrupt */
	else if((u32IntStatus & (ADDRMEN|ADDRMINT)) == (ADDRMEN|ADDRMINT))
		outp32(REG_VPEINT, (u32IntStatus & ~(MDINT | VINT | MEINT)));				/* Clear Address match interrupt */
	else if ((u32IntStatus & (MEINTEN|MEINT)) == (MEINTEN|MEINT))
		outp32(REG_VPEINT, (u32IntStatus & ~(MDINT | VINT|ADDRMINT)));			/* Clear Memory error interrupt */	
	else if ((u32IntStatus & (MDINTEN|MDINT)) == (MDINTEN|MDINT))
		outp32(REG_VPEINT, (u32IntStatus & ~( VINT | MEINT | ADDRMINT)));			/* Clear Memory error interrupt */	

	wake_up_interruptible(&videoin_wq);    
	return IRQ_HANDLED;
}


void videoin_register_outputdev(void *dev_id, vout_ops_t *fn)
{
	DBG_PRINTF("%s\n",__FUNCTION__);		
	videoin_priv.dev_id = dev_id;
	videoin_priv.callback = fn;
}

void videoin_release(struct video_device *vfd)
{
	DBG_PRINTF("%s\n",__FUNCTION__);		
	//kfree(vfd);
}
unsigned int g_u32FbPhyAddr;
int __devinit videoin_init(void)
{
	int ret = 0;
	int i;
	unsigned int u32PacketBuf0, u32PacketBuf1, u32PacketBuf2;
	unsigned int u32PlanarBuf0, u32PlanarBuf1;
	UINT32 u32PhyAddr;
	videoin_priv_t *priv = (videoin_priv_t *)&videoin_priv;
	
	g_u32FbPhyAddr = inp32(REG_LCM_FSADDR);					
	
	DBG_PRINTF("%s\n",__FUNCTION__);				
	/* initialize locks */
	init_MUTEX(&videoin_priv.lock);
	
	priv->jdev.owner = THIS_MODULE;
	priv->jdev.type = VID_TYPE_CAPTURE | VID_TYPE_SCALES;
	priv->jdev.hardware = VID_HARDWARE_W55FA93;
	priv->jdev.release = videoin_release;
	priv->jdev.fops = &videoin_fops;
	priv->jdev.priv = &videoin_priv;
	priv->preview_width     = LCDWIDTH; //CONFIG_VIDEOIN_PREVIEW_RESOLUTION_X;
	priv->preview_height    = LCDHEIGHT; //CONFIG_VIDEOIN_PREVIEW_RESOLUTION_Y;
	
	priv->videoin_buffersize = CONFIG_VIDEOIN_PREVIEW_BUFFER_SIZE;
	
	priv->videoin_buffer = kmalloc(sizeof(__u8*) * CONFIG_VIDEOIN_BUFFER_COUNT, GFP_KERNEL);
#ifdef CONFIG_VIDEOIN_VPOST_OVERLAY_BUFFER
	DBG_PRINTF("\nUsing Overlay\n");
	/*For 1 * Packet pipe*/
	u32PhysPacketBuf = w55fa93_FB_BG_PHY_ADDR;
	u32VirtPacketBuf = phys_to_virt(u32PhysPacketBuf);
	
	/*For Planar pipe*/
	priv->vaddr = videoin_dmamalloc(VIDEOIN_ENCODE_BUFFER_SIZE);
	if (!priv->vaddr)
		return -ENOMEM;
#else
	DBG_PRINTF("\nUsing FSC\n");
	//Allocate 3 buffer for preview 
	u32PacketBuf0 = videoin_dmamalloc_phy(0, LCDWIDTH*LCDHEIGHT*LCDBPP/8);		//Packet buffer 0
	if (!u32PacketBuf0)
	{
		printk("VideoIn allocated buffer fail\n");
		return -ENOMEM;	
	}
	u32PacketBuf1 = videoin_dmamalloc_phy(1, LCDWIDTH*LCDHEIGHT*LCDBPP/8);		//Packet buffer 1
	if (!u32PacketBuf1)
	{
		printk("VideoIn allocated buffer fail\n");
		return -ENOMEM;	
	}
	u32PacketBuf2 = videoin_dmamalloc_phy(2, LCDWIDTH*LCDHEIGHT*LCDBPP/8);		//Packet buffer 2
	if (!u32PacketBuf2)
	{
		printk("VideoIn allocated buffer fail\n");
		return -ENOMEM;		
	}
	//Allocate 2 buffer for JEPG encode  
	u32PlanarBuf0 = videoin_dmamalloc_phy(3, VIDEOIN_ENCODE_BUFFER_SIZE);			//Planar buffer 0	
	w55fa93_JPG_Y0_ADDR = u32PlanarBuf0;
	if (!u32PlanarBuf0)
	{
		printk("VideoIn allocated buffer fail\n");
		return -ENOMEM;
	}
	u32PlanarBuf1 = videoin_dmamalloc_phy(4, VIDEOIN_ENCODE_BUFFER_SIZE);			//Planar buffer 1
	w55fa93_JPG_Y1_ADDR = u32PlanarBuf1;
	if (!u32PlanarBuf1)
	{
		printk("VideoIn allocated buffer fail\n");
		return -ENOMEM;
	}
#endif
	
	for(i = 0; i < CONFIG_VIDEOIN_BUFFER_COUNT; i++)
	{
		priv->videoin_buffer[i] = priv->paddr + i * CONFIG_VIDEOIN_PREVIEW_BUFFER_SIZE;
		DBG_PRINTF("bufer[%d]:%x\n", i, priv->videoin_buffer[i]);
	}

	if (video_register_device(&priv->jdev, VFL_TYPE_GRABBER, videoin_nr) == -1) {
		printk("%s: video_register_device failed\n", __FUNCTION__);
		dma_free_writecombine(NULL/*dev*/, VIDEOIN_ENCODE_BUFFER_SIZE, priv->vaddr, priv->paddr);
		kfree(priv->videoin_buffer);//2010-07-27
		return -EPIPE;
	}

	if (!request_mem_region((unsigned long)W55FA93_VA_VIDEOIN, W55FA93_SZ_VIDEOIN, "w55fa93-videoin"))
	{
		printk("%s: request_mem_region failed\n", __FUNCTION__);
		video_unregister_device(&videoin_priv.jdev);
		dma_free_writecombine(NULL/*dev*/, VIDEOIN_ENCODE_BUFFER_SIZE, priv->vaddr, priv->paddr);
		kfree(priv->videoin_buffer); //2010-07-27
		return -EBUSY;
	}

	ret = request_irq(IRQ_CAP, irq_handler, SA_INTERRUPT, "w55fa93-videoin", priv);
	if (ret) {
		printk("cannot get irq %d - err %d\n", IRQ_CAP, ret);
		ret = -EBUSY;
		goto release_mem;
	}
	DrvVideoIn_Init(TRUE, 						// BOOL bIsEnableSnrClock,
					0, 						// E_DRVVIDEOIN_SNR_SRC eSnrSrc,	
					24000, 					// UINT32 u32SensorFreq,
					eDrvVideoIn_2nd_SNR_CCIR601);	// E_DRVVIDEOIN_DEV_TYPE eDevType
		
	if (InitSensor(u32Sensor, priv, u32PacketBuf0, u32PacketBuf1, u32PacketBuf2) == 0)
	{				
		printk("Init Sensor fail\n");	
		ret = -EBUSY;		
		goto release_mem;
	}	
	return ret;

release_mem:
	video_unregister_device(&priv->jdev);
	release_mem_region((unsigned long)W55FA93_VA_VIDEOIN, W55FA93_SZ_VIDEOIN);	
	free_irq(IRQ_CAP,priv);
	videoin_free_dmamalloc_phy();
	kfree(priv->videoin_buffer); //2010-07-27
	return ret;
}

static void __exit videoin_cleanup(void)
{
	videoin_priv_t *priv = (videoin_priv_t *)&videoin_priv;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	video_unregister_device(&priv->jdev);
	release_mem_region((unsigned long)W55FA93_VA_VIDEOIN, W55FA93_SZ_VIDEOIN);
	free_irq(IRQ_CAP,priv);
	dma_free_writecombine(NULL/*dev*/, VIDEOIN_BUFFER_SIZE, priv->vaddr, priv->paddr);
	//platform_driver_unregister(&ov9660codec_driver);
}

module_init(videoin_init);
module_exit(videoin_cleanup);

EXPORT_SYMBOL(videoin_register_outputdev);

MODULE_DESCRIPTION("video in driver for the W55FA93");
MODULE_LICENSE("GPL");
