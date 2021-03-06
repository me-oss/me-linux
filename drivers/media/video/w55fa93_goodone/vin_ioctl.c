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

#include "DrvFsc.h"


//#define DBG_PRINTF	printk
#define DBG_PRINTF(...)
#define ERR_PRINTF		printk

/* Global variable */
extern videoin_priv_t 		videoin_priv;
extern videoin_window_t 	tViewwindow;
extern videoin_encode_t		tEncode;

extern unsigned int w55fa93_FB_BG_PHY_ADDR;
extern unsigned int w55fa93_JPG_Y0_ADDR, w55fa93_JPG_U0_ADDR, w55fa93_JPG_V0_ADDR;
extern unsigned int w55fa93_JPG_Y1_ADDR, w55fa93_JPG_U1_ADDR, w55fa93_JPG_V1_ADDR;
extern unsigned int w55fa93_JPG_Y_ADDR;
extern unsigned int bDumpframeBuffer;
extern videoIn_buf_t videoIn_buf[];

#ifndef CONFIG_FSC
extern unsigned int bIsVideoInEnable;
extern unsigned int vinPackAddr0,  vinPackAddr1, vinPackAddr2; 
#endif
#define outp32(addr, value)		outl(value, addr)
#define inp32(addr)			inl(addr)


void vin_ioctl_preview_start(void)
{	
	unsigned int u32frameCount;
	BOOL bEngEnable;  	
	E_DRVVIDEOIN_PIPE ePipeEnable;

	DrvVideoIn_GetPipeEnable(&bEngEnable,
								&ePipeEnable);
#ifdef CONFIG_FSC	
	outp32(REG_LCM_LCDCCtl, inp32(REG_LCM_LCDCCtl) | BIT31);			//Enable Buffer address by FSC 	
	DrvVideoIn_SetPacketFrameBufferControl(TRUE, 
										TRUE);	
#endif
	switch(ePipeEnable) 
	{
		case eDRVVIDEOIN_BOTH_PIPE_DISABLE:
									
		case eDRVVIDEOIN_PACKET:								
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_PACKET);				break;
		case eDRVVIDEOIN_PLANAR:
		case eDRVVIDEOIN_BOTH_PIPE_ENABLE:					
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_BOTH_PIPE_ENABLE);	break;							
		default:												break;
	}		
	//bIsVideoInEnable = 1;	/* Important !!! to enable VPOST show video buffer */			
	DrvVideoIn_EnableInt(eDRVVIDEOIN_VINT);						
	u32frameCount = videoinFrameCount();

	if((inp32(REG_VPECTL) & (PKEN | PNEN)) !=0)
	{//Note! the interrupt will always keep sclient if disable both pipes
		DBG_PRINTF("Wait a frame\n");
		do
		{
		}while(u32frameCount == videoinFrameCount());	
	}
}
void vin_ioctl_preview_stop(void)
{
	unsigned int u32frameCount;
	BOOL bEngEnable;  	
	E_DRVVIDEOIN_PIPE ePipeEnable;
	DrvVideoIn_GetPipeEnable(&bEngEnable,
								&ePipeEnable);
	//if(bEngEnable)
	//{
	//DrvVideoIn_SetOperationMode(TRUE);		//TRUE:One shutter mode
	//while(DrvVideoIn_GetOperationMode()==TRUE);
	//}			
	switch(ePipeEnable) 
	{
		case eDRVVIDEOIN_BOTH_PIPE_DISABLE:					break;
		case eDRVVIDEOIN_PACKET:								
					DrvVideoIn_SetPipeEnable(FALSE,
							eDRVVIDEOIN_BOTH_PIPE_DISABLE);	break;
		case eDRVVIDEOIN_PLANAR:							break;
		case eDRVVIDEOIN_BOTH_PIPE_ENABLE:					
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_PLANAR);				break;							
		default:												break;
	}	
	u32frameCount = videoinFrameCount();
	if((inp32(REG_VPECTL) & (PKEN | PNEN)) !=0)
	{//Note! the interrupt will always keep sclient if disable both pipes
		DBG_PRINTF("Wait a frame\n");
		do
		{
		}while(u32frameCount == videoinFrameCount());		
	}
}
void vin_ioctl_encode_start(void)
{	
	unsigned int u32frameCount;
	BOOL bEngEnable;  	
	E_DRVVIDEOIN_PIPE ePipeEnable;

	DrvVideoIn_GetPipeEnable(&bEngEnable,
								&ePipeEnable);
#ifdef CONFIG_FSC
	g_u32FbPhyAddr = inp32(REG_LCM_FSADDR);
	outp32(REG_LCM_LCDCCtl, inp32(REG_LCM_LCDCCtl) | BIT31);			//Enable Buffer address by FSC 	
	DrvVideoIn_SetPacketFrameBufferControl(TRUE, 
										TRUE);	
#endif 
	switch(ePipeEnable) 
	{
		case eDRVVIDEOIN_BOTH_PIPE_DISABLE:
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_PLANAR);				break;				
		case eDRVVIDEOIN_PACKET:								
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_BOTH_PIPE_ENABLE);	break;
		case eDRVVIDEOIN_PLANAR:							break;
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_PLANAR);				break;
		case eDRVVIDEOIN_BOTH_PIPE_ENABLE:					
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_BOTH_PIPE_ENABLE);	break;							
		default:												break;
	}		
	DrvVideoIn_EnableInt(eDRVVIDEOIN_VINT);
	u32frameCount = videoinFrameCount();
	if((inp32(REG_VPECTL) & (PKEN | PNEN)) !=0)
	{//Note! the interrupt will always keep sclient if disable both pipes
		DBG_PRINTF("Wait a frame\n");
		do
		{
		}while(u32frameCount == videoinFrameCount());	
	}
}
void vin_ioctl_encode_stop(void)
{
	unsigned int u32frameCount;
	BOOL bEngEnable;  	
	E_DRVVIDEOIN_PIPE ePipeEnable;
	DrvVideoIn_GetPipeEnable(&bEngEnable,
								&ePipeEnable);
	switch(ePipeEnable) 
	{
		case eDRVVIDEOIN_BOTH_PIPE_DISABLE:					break;
		case eDRVVIDEOIN_PACKET:								
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_PACKET);				break;
		case eDRVVIDEOIN_PLANAR:							break;
					DrvVideoIn_SetPipeEnable(FALSE,
							eDRVVIDEOIN_BOTH_PIPE_DISABLE);	break;	
		case eDRVVIDEOIN_BOTH_PIPE_ENABLE:					
					DrvVideoIn_SetPipeEnable(TRUE,
							eDRVVIDEOIN_PACKET);				break;							
		default:												break;
	}	
	u32frameCount = videoinFrameCount();
	if((inp32(REG_VPECTL) & (PKEN | PNEN)) !=0)
	{//Note! the interrupt will always keep sclient if disable both pipes
		DBG_PRINTF("Wait a frame\n");
		do
		{
		}while(u32frameCount == videoinFrameCount());
	}		
}
/*
void vin_ioctl_exit(void)
{
	
	BOOL bEngEnable;  	
	E_DRVVIDEOIN_PIPE ePipeEnable;
	DrvVideoIn_GetPipeEnable(&bEngEnable,
								&ePipeEnable);
	if(bEngEnable)
	{
		DrvVideoIn_SetOperationMode(TRUE);		//TRUE:One shutter mode
		while(DrvVideoIn_GetOperationMode()==TRUE);
	}		
	DrvVideoIn_SetPipeEnable(FALSE,
							eDRVVIDEOIN_BOTH_PIPE_DISABLE);
	DrvVideoIn_DisableInt(eDRVVIDEOIN_VINT);
	outp32(REG_LCM_FSADDR, g_u32FbPhyAddr);
	outp32(REG_LCM_LCDCCtl, inp32(REG_LCM_LCDCCtl) & ~BIT31);			//Disable Buffer address by FSC 		
}
*/
unsigned int vin_ioctl_preview_ctl(struct inode *inode, 
					struct file *file,
				 	unsigned int cmd, 
					void *arg)
{
	DBG_PRINTF("preview arg %x\n", arg);
	if(arg == 0)
		vin_ioctl_preview_stop();
	else
		vin_ioctl_preview_start();
}
unsigned int vin_ioctl_encode_ctl(struct inode *inode, 
					struct file *file,
				 	unsigned int cmd, 
					void *arg)
{
	DBG_PRINTF("encode arg %x\n", arg);
	if(arg == 0)
		vin_ioctl_encode_stop();
	else
		vin_ioctl_encode_start();
}
unsigned int vin_ioctl_select_frame_buffer(struct inode *inode, 
					struct file *file,
				 	unsigned int cmd, 
					void *arg)
{
	DBG_PRINTF("encode arg %x\n", arg);
	if(arg == 0)		//packet buffer.
		bDumpframeBuffer = 0;
	else			//planar buffer.
		bDumpframeBuffer = 1;
}

unsigned int vin_ioctl_s_para(struct inode *inode, 
					struct file *file,
				 	unsigned int cmd, 
					void *arg)
{
	struct video_device *dev = video_devdata(file);
	videoin_priv_t *priv = (videoin_priv_t *)dev->priv;
	int ret = 0;
	videoin_param_t param;

	DBG_PRINTF("videoin_ioctl VIDEOIN_S_PARAM\n");
	if (copy_from_user((void*)&param, (void *)arg, sizeof(param))) {
		ERR_PRINTF("copy_from_user error\n");
		ret = -EFAULT;
		return ret;
	}
	DrvVideoIn_SetDataFormatAndOrder(eDRVVIDEOIN_IN_UYVY, 
										param.format.input_format,
										param.format.output_format);		

	{
		UINT32 u32GCD;
		u32GCD = vinGCD(param.resolution.y,
								 	480); 							 
		DrvVideoIn_SetVerticalScaleFactor(eDRVVIDEOIN_PACKET,		
										param.resolution.y/u32GCD,
										480/u32GCD);	
		DBG_PRINTF("V DDA: %d/%d\n", param.resolution.y/u32GCD, 480/u32GCD);									
		u32GCD = vinGCD(param.resolution.x, 
									640);													
		DrvVideoIn_SetHorizontalScaleFactor(eDRVVIDEOIN_PACKET,		
										param.resolution.x/u32GCD,
										640/u32GCD);
		DBG_PRINTF("H DDA: %d/%d\n", param.resolution.x/u32GCD, 640/u32GCD);
		
	}
	DrvVideoIn_SetSensorPolarity(param.polarity.bVsync, 
									param.polarity.bHsync, 
									param.polarity.bPixelClk);			

	DrvVideoIn_SetShadowRegister();
	return ret;
}
unsigned int vin_ioctl_s_view_window(struct inode *inode, 
					struct file *file,
				 	unsigned int cmd, 
					void *arg)
{				
	int ret = 0, i;	
	UINT32 u32PacStride, u32PlaStride, u32Addr;
	BOOL bEngEnable;  	
	E_DRVVIDEOIN_PIPE ePipeEnable;
	DrvVideoIn_GetPipeEnable(&bEngEnable,
								&ePipeEnable);	
	//DrvVideoIn_SetOperationMode(TRUE);			//TRUE:One shutter mode
	//while(DrvVideoIn_GetOperationMode()==TRUE);

	/* Clear the ram out, no junk to the user */
#if 0		
	memset(videoIn_buf[0].u32VirtAddr, 0, LCDWIDTH*LCDHEIGHT*LCDBPP/8);
	memset(videoIn_buf[1].u32VirtAddr, 0, LCDWIDTH*LCDHEIGHT*LCDBPP/8);
	memset(videoIn_buf[2].u32VirtAddr, 0, LCDWIDTH*LCDHEIGHT*LCDBPP/8);
#endif
	DBG_PRINTF("videoin_ioctl VIDEOIN_S_VIEW_WINDOW\n");
	if (copy_from_user((void*)&tViewwindow, (void *)arg, sizeof(tViewwindow))) {
		ERR_PRINTF("copy_from_user error\n");
		ret = -EFAULT;
		return ret;
	}
	DBG_PRINTF("ViewWindow Width =%d\n", tViewwindow.ViewWindow.u32ViewWidth);
	DBG_PRINTF("ViewWindow Width =%d\n", tViewwindow.ViewWindow.u32ViewHeight);	
	DBG_PRINTF("ViewWindow PosX =%d\n", tViewwindow.ViewStartPos.u32PosX);
	DBG_PRINTF("ViewWindow PosY =%d\n",tViewwindow.ViewStartPos.u32PosY);
	DBG_PRINTF("Packet buffer 0 physical address =0x%x\n", videoIn_buf[0].u32PhysAddr);				
	DBG_PRINTF("Packet buffer 1 physical address =0x%x\n", videoIn_buf[1].u32PhysAddr);
	DBG_PRINTF("Packet buffer 2 physical address =0x%x\n", videoIn_buf[2].u32PhysAddr);

	vinPackAddr0 = videoIn_buf[0].u32PhysAddr + \
				LCDWIDTH*tViewwindow.ViewStartPos.u32PosY*LCDBPP/8+ \
				tViewwindow.ViewStartPos.u32PosX*LCDBPP/8;
	DBG_PRINTF("New Packet buffer 0 physical address =0x%x\n", vinPackAddr0);	
	/*
	DrvFSC_SetChannelBaseAddr(eDRVFSC_CHANNEL_0,
								0,
								u32Addr
								);				
	*/
	vinPackAddr1 = videoIn_buf[1].u32PhysAddr + \
				LCDWIDTH*tViewwindow.ViewStartPos.u32PosY*LCDBPP/8+ \
				tViewwindow.ViewStartPos.u32PosX*LCDBPP/8;	
	
	DBG_PRINTF("New Packet buffer 1 physical address =0x%x\n", vinPackAddr1);				
	/*
	DrvFSC_SetChannelBaseAddr(eDRVFSC_CHANNEL_0,
								1,	
								u32Addr
								);
	*/
	vinPackAddr2 = videoIn_buf[2].u32PhysAddr + \
				LCDWIDTH*tViewwindow.ViewStartPos.u32PosY*LCDBPP/8+ \
				tViewwindow.ViewStartPos.u32PosX*LCDBPP/8;
	DBG_PRINTF("New Packet buffer 2 physical address =0x%x\n", u32Addr);
	/*
	DrvFSC_SetChannelBaseAddr(eDRVFSC_CHANNEL_0,
								2,
								u32Addr
								);			
	*/		
	DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PACKET, 0, vinPackAddr0);
	DrvVideoIn_SetShadowRegister();
	DrvVideoIn_GetStride(&u32PacStride, &u32PlaStride);
	DrvVideoIn_SetStride(LCDWIDTH, u32PlaStride);
	return ret;							
}
unsigned int vin_ioctl_s_jpg_para(struct inode *inode, 
					struct file *file,
				 	unsigned int cmd, 
					void *arg)
{				
	int ret = 0;	
	UINT32 u32PacStride, u32PlaStride;
	UINT32 u32PhyYAddr,   u32PhyUAddr,  u32PhyVAddr;
	UINT32 u32GCD;
	BOOL bEngEnable;
	E_DRVVIDEOIN_PIPE ePipeEnable;

	DBG_PRINTF("videoin_ioctl VIDEOIN_S_JPG_WINDOW\n");
	if (copy_from_user((void*)&tEncode, (void *)arg, sizeof(tEncode))) {
		ERR_PRINTF("copy_from_user error\n");
		ret = -EFAULT;
		return ret;
	}
	DBG_PRINTF("JPG Width =%d\n", tEncode.u32Width);
	DBG_PRINTF("JPG Height =%d\n",tEncode.u32Height);	
	DBG_PRINTF("JPG Format =%d\n", tEncode.u32Format);
	if( tEncode.u32Width> 640)
	{
		ret = -EFAULT;
		return ret;
	}	
	if(tEncode.u32Height> 480)
	{
		ret = -EFAULT;
		return ret;
	}	
	if((tEncode.u32Width%4)!=0)
	{
		ret = -EFAULT;
		return ret;
	}

	u32GCD = vinGCD(tEncode.u32Height,
							 	480); 							 
	DrvVideoIn_SetVerticalScaleFactor(eDRVVIDEOIN_PLANAR,		
									tEncode.u32Height/u32GCD,
									480/u32GCD);											
	u32GCD = vinGCD(tEncode.u32Width, 
								640);													
	DrvVideoIn_SetHorizontalScaleFactor(eDRVVIDEOIN_PLANAR,		
									tEncode.u32Width/u32GCD,
									640/u32GCD);
	
	
	if(tEncode.u32Format&1)
	{//YUV420 
		DrvVideoIn_SetPlanarFormat(TRUE);
		w55fa93_JPG_U0_ADDR = w55fa93_JPG_Y0_ADDR  + tEncode.u32Width*tEncode.u32Height;
		w55fa93_JPG_V0_ADDR = w55fa93_JPG_U0_ADDR + tEncode.u32Width*tEncode.u32Height/4;

		w55fa93_JPG_U1_ADDR = w55fa93_JPG_Y1_ADDR  + tEncode.u32Width*tEncode.u32Height;
		w55fa93_JPG_V1_ADDR = w55fa93_JPG_U1_ADDR + tEncode.u32Width*tEncode.u32Height/4;
	}
	else
	{//YUV422
		DrvVideoIn_SetPlanarFormat(FALSE);
		w55fa93_JPG_U0_ADDR = w55fa93_JPG_Y0_ADDR  + tEncode.u32Width*tEncode.u32Height;
		w55fa93_JPG_V0_ADDR = w55fa93_JPG_U0_ADDR + tEncode.u32Width*tEncode.u32Height/2;
		w55fa93_JPG_U1_ADDR = w55fa93_JPG_Y1_ADDR  + tEncode.u32Width*tEncode.u32Height;
		w55fa93_JPG_V1_ADDR = w55fa93_JPG_U1_ADDR + tEncode.u32Width*tEncode.u32Height/2;
	}
	DBG_PRINTF("Y0 Addr =0x%x\n",  w55fa93_JPG_Y0_ADDR);
	DBG_PRINTF("U0 Addr =0x%x\n",  w55fa93_JPG_U0_ADDR);
	DBG_PRINTF("V0 Addr =0x%x\n",  w55fa93_JPG_V0_ADDR);	
	DBG_PRINTF("Y1 Addr =0x%x\n",  w55fa93_JPG_Y1_ADDR);
	DBG_PRINTF("U1 Addr =0x%x\n",  w55fa93_JPG_U1_ADDR);
	DBG_PRINTF("V1 Addr =0x%x\n",  w55fa93_JPG_V1_ADDR);			
	DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 
									eDRVVIDEOIN_BUF0, 	//Y										
									w55fa93_JPG_Y0_ADDR);
	DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 
									eDRVVIDEOIN_BUF1, 	//U										
									w55fa93_JPG_U0_ADDR);
	DrvVideoIn_SetBaseStartAddress(eDRVVIDEOIN_PLANAR, 
									eDRVVIDEOIN_BUF2, 											
									w55fa93_JPG_V0_ADDR);	//V
	w55fa93_JPG_Y_ADDR 	= w55fa93_JPG_Y0_ADDR;
	DrvVideoIn_GetPipeEnable(&bEngEnable, &ePipeEnable);		
	if(tEncode.u32Enable == 1)
	{
		DBG_PRINTF("Planar Pipe enable\n");
		DrvVideoIn_SetPipeEnable(TRUE, ePipeEnable | eDRVVIDEOIN_PLANAR );
	}
	else
	{
		DBG_PRINTF("Planar Pipe disable\n");
		DrvVideoIn_SetPipeEnable(bEngEnable, ePipeEnable & (~eDRVVIDEOIN_PLANAR));
	}
	
	DrvVideoIn_GetStride(&u32PacStride, &u32PlaStride);
	DrvVideoIn_SetStride(LCDWIDTH, tEncode.u32Width);
	DrvVideoIn_SetShadowRegister();	
	return ret;					
}
/*
unsigned int vin_ioctl_g_info(struct inode *inode, 
					struct file *file,
				 	unsigned int cmd, 
					void *arg)
{
	int ret = 0;
	struct video_device *dev = video_devdata(file);
	videoin_priv_t *priv = (videoin_priv_t *)dev->priv;	

	videoin_info_t videoinfo;
	videoinfo.bufferend = priv->videoin_bufferend;

	DBG_PRINTF("videoin_ioctl VIDEOIN_G_INFO\n");
	if (copy_to_user((void*)arg, (void *)&videoinfo, sizeof(videoin_info_t))) {
		ERR_PRINTF("copy_to_user error\n");
		ret = -EFAULT;
		return ret;
	}
	return ret;
}
*/
unsigned int vin_ioctl_encode_pipe_start(void)
{
	BOOL   	bIsEngEnable; 		
	UINT32 	u32PipeEnable;
	
	DrvVideoIn_GetPipeEnable(&bIsEngEnable, &u32PipeEnable);

	switch(u32PipeEnable) 
	{
		case eDRVVIDEOIN_BOTH_PIPE_DISABLE:
			DrvVideoIn_SetPipeEnable(bIsEngEnable,
							eDRVVIDEOIN_PLANAR);				break;
		case eDRVVIDEOIN_PACKET:
				DrvVideoIn_SetPipeEnable(bIsEngEnable,
							eDRVVIDEOIN_BOTH_PIPE_ENABLE);	break;
		case eDRVVIDEOIN_PLANAR:	
		case eDRVVIDEOIN_BOTH_PIPE_ENABLE:					break;				
		default:												break;
	}	
}
