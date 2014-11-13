/* linux/driver/char/nuc900_usb.c
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
 *   2007/01/26     lssh add this file for nuvoton usb device driver.
 */
 

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ioport.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/map.h>
#include <asm/arch/irqs.h>
#include <linux/delay.h>

#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/w55fa93_usbd.h>
#include <asm/arch/w55fa93_gpio.h>

//USB TEST MODES
#define TEST_J					0x01
#define TEST_K					0x02
#define TEST_SE0_NAK			0x03
#define TEST_PACKET				0x04
#define TEST_FORCE_ENABLE		0x05

#ifdef SD_DEBUG
#define DBGOUT(fmt, arg...) printk(fmt, ##arg)
#define PDEBUG(fmt, arg...)		printk(fmt, ##arg)
#else
#define DBGOUT(fmt, arg...)
#define PDEBUG(fmt, arg...)
//#define printk(fmt, arg...)
#endif

#ifdef SD_DEBUG_PRINT_LINE
#define PRN_LINE()				PDEBUG("[%-20s] : %d\n", __FUNCTION__, __LINE__)
#else
#define PRN_LINE()
#endif

#ifdef SD_DEBUG_ENABLE_ENTER_LEAVE
#define ENTER()					PDEBUG("[%-20s] : Enter...\n", __FUNCTION__)
#define LEAVE()					PDEBUG("[%-20s] : Leave...\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef SD_DEBUG_ENABLE_MSG
#define MSG(msg)				PDEBUG("[%-20s] : %s\n", __FUNCTION__, msg)
#else
#define MSG(msg)
#endif

#ifdef SD_DEBUG_ENABLE_MSG2
#define MSG2(fmt, arg...)			PDEBUG("[%-20s] : "fmt, __FUNCTION__, ##arg)
#define PRNBUF(buf, count)		{int i;MSG2("CID Data: ");for(i=0;i<count;i++)\
									PDEBUG("%02x ", buf[i]);PDEBUG("\n");}
#else
#define MSG2(fmt, arg...)
#define PRNBUF(buf, count)
#endif

#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
#include <asm/arch/w55fa93_sysmgr.h>
extern void sysmgr_report(unsigned status);
#endif
u32 volatile enabletestmode,testselector;
u32 volatile usbdMaxPacketSize;
u32 volatile g_maxLun = 0;
u32 volatile usb_buffer_base;
u32 volatile usb_data_buffer_size = 0x10000, usb_cbw_buffer_size = CBW_SIZE,usb_csw_buffer_size = CSW_SIZE;
u32 volatile g_USB_Mode_Check = 0;
u32 volatile g_dwCBW_flag = 0, g_preventflag = 0;
static const char driver_name [] = "Nuvoton W55FA93 USB 2.0 Bulk Driver";
static int bIsPlug=0;
int volatile bIsUSBOnLine = 0;

////////////////////////////////////////////////////////////////////////////
/* MSC Descriptor */
static u8 Mass_DeviceDescriptor[18] __attribute__ ((aligned(4))) =
{
	0x12,
	0x01,
	0x00, 0x02,			/* bcdUSB - USB 2.0 */
	0x00,
	0x00,
	0x00,
	0x40,				/* bMaxPacketSize0 - 64 Bytes  */
	0x16, 0x04,			/* Vendor ID */ 
	0x13, 0x09,			/* Product ID */
	0x00, 0x00,		
	0x01,				/* iManufacture */
	0x02,				/* iProduct */
	0x03,				/* iSerialNumber */
	0x01				/* bNumConfigurations */
} ;


static u16 Mass_QualifierDescriptor[6] = 
{
	0x060a, 0x0200, 0x0000, 0x4000, 0x0001, 0x0000
};

static u16 Mass_ConfigurationBlock[16] =
{
	0x0209, 0x0020, 0x0101, 0x8000, 0x0932, 0x0004, 0x0200, 0x0508, 0x0050, 0x0507,
	0x0281, 0x0200, 0x0701, 0x0205, 0x0002, 0x0102
};

static u16 Mass_ConfigurationBlockFull[16] =
{
	0x0209, 0x0020, 0x0101, 0x8000, 0x0932, 0x0004, 0x0200, 0x0508, 0x0050, 0x0507,
	0x0281, 0x0040, 0x0701, 0x0205, 0x4002, 0x0100
};

static u16 Mass_OSConfigurationBlock[16] =
{
	0x0709, 0x0020, 0x0101, 0x8000, 0x0932, 0x0004, 0x0200, 0x0508, 0x0050, 0x0507,
	0x0281, 0x0040, 0x0701, 0x0205, 0x4002, 0x0100
};

static u16 Mass_StringDescriptor0[2] = 
{
	0x0304, 0x0409
};

u32 volatile g_STR1_DSCPT_LEN,g_STR2_DSCPT_LEN,g_STR3_DSCPT_LEN; 

/* iManufacturer */
u8 Mass_StringDescriptor1[] __attribute__ ((aligned(4))) = 
{
	0x10, 	/* bLength (Dafault Value is 0x10, the value will be set to actual value according to the Descriptor size later) */
	0x03,	/* bDescriptorType */
	'n', 0, 'u', 0,	'v', 0, 'o', 0,	'T', 0, 'o', 0, 'n', 0
} ;

/* iProduct */
u8 Mass_StringDescriptor2[] __attribute__ ((aligned(4))) = 
{
	0x24,	/* bLength (Dafault Value is 0x24, the value will be set to actual value according to the Descriptor size later) */
	0x03,	/* bDescriptorType */
	'A', 0, 'R', 0,	'M', 0, '9', 0,	'2', 0, '6', 0,	'-', 0, 'B', 0,	'a', 0, 's', 0,	'e', 0, 'd', 0,
	' ', 0, 'M', 0,	'C', 0, 'U', 0
};
/* iSerialNumber */
u8 Mass_StringDescriptor3[]  __attribute__ ((aligned(4))) = 
{
	0x10, 	/* bLength (Dafault Value is 0x10, the value will be set to actual value according to the Descriptor size later) */
	0x03,	/* bDescriptorType */
	'n', 0, 'u', 0,	'v', 0, 'o', 0,	'T', 0, 'o', 0, 'n', 0
};

////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
static u32* _DeviceDescriptor;
static u32* _QualifierDescriptor;
static u32* _ConfigurationBlock;
static u32* _ConfigurationBlockFull;
static u32* _OSConfigurationBlock;
static u32* _StringDescriptor0;
static u32* _StringDescriptor1;
static u32* _StringDescriptor2;
static u32* _StringDescriptor3;

static void	USB_Init(wbusb_dev*	dev);
static void	USB_Irq_Init(wbusb_dev*	dev);
static void	SDRAM_USB_Transfer(wbusb_dev *dev,u8	epname,u8* buf ,u32 Tran_Size);

void write_data(wbusb_dev *dev,u8* buf,u32 length);
void read_data(wbusb_dev *dev,u8* buf,u32	length);
int	check_cbw(wbusb_dev	*dev,void* cbw);

static void	start_write(wbusb_dev *dev,u8* buf,u32 length);
static void	start_read(wbusb_dev *dev,u8* buf,u32	length);

static void	A_task_wake_up(wbusb_dev *dev);//write task

static void	B_task_block(wbusb_dev *dev);//idle	and	read task
static void	B_task_wake_up(wbusb_dev *dev);

static void	C_task_block(wbusb_dev *dev);//vcom vendor
static void	C_task_wake_up(wbusb_dev *dev);

void __usleep(int count)
{
	int i=0;
	
	for(i=0;i<count;i++);
	
}

static void start_write(wbusb_dev *dev,u8* buf,u32 length)
{
	u32 volatile reg;
#ifdef FLUSH_CACHE
	u32 volatile dma_addr;
#endif
	u32 offset;
	
	DECLARE_WAITQUEUE(wait,	current);
	add_wait_queue(&dev->wusbd_wait_a, &wait);
	set_current_state(TASK_INTERRUPTIBLE);


	USB_WRITE(REG_USBD_IRQ_ENB, (USB_DMA_REQ | USB_RST_STS | USB_SUS_REQ|USB_VBUS_STS));
	//dump_massbuf(buf,length);
#ifdef FLUSH_CACHE
	flush_cache_all();
#endif
#ifdef FLUSH_CACHE
	dma_addr=(u32)__pa(buf);
	USB_WRITE(REG_USBD_AHB_DMA_ADDR, dma_addr);//Tell DMA the memory physcal address
#else
	if(buf== dev->mass_wbuf)
		USB_WRITE(REG_USBD_AHB_DMA_ADDR, dev->mass_dma_wbuf);//Tell DMA the memory physcal address
	else
		USB_WRITE(REG_USBD_AHB_DMA_ADDR, dev->mass_dma_cswbuf);//Tell DMA the memory physcal address
#endif
	USB_WRITE(REG_USBD_DMA_CNT, length);

	reg = USB_READ(REG_USBD_DMA_CTRL_STS);
	if ((reg & 0x40) != 0x40)
		USB_WRITE(REG_USBD_DMA_CTRL_STS, reg|0x00000020);

	schedule();
	
//quit:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&dev->wusbd_wait_a, &wait);

	return ;
}

static void start_read(wbusb_dev *dev,u8* buf,u32	length)
{
#ifdef FLUSH_CACHE
	unsigned int volatile dma_addr;
#endif
	DECLARE_WAITQUEUE(wait,	current);
	add_wait_queue(&dev->wusbd_wait_b, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	
	USB_WRITE(REG_USBD_IRQ_ENB, (USB_DMA_REQ | USB_RST_STS|USB_VBUS_STS));
#ifdef FLUSH_CACHE
	flush_cache_all();
#endif
#ifdef FLUSH_CACHE
	dma_addr=(u32)__pa(buf);
	USB_WRITE(REG_USBD_AHB_DMA_ADDR,dma_addr);//Tell DMA the memory address
#else	
	if(buf== dev->mass_rbuf)
		USB_WRITE(REG_USBD_AHB_DMA_ADDR, dev->mass_dma_rbuf);//Tell DMA the memory address
	else
		USB_WRITE(REG_USBD_AHB_DMA_ADDR, dev->mass_dma_cbwbuf);//Tell DMA the memory address
#endif
	USB_WRITE(REG_USBD_DMA_CNT, length);
	USB_WRITE(REG_USBD_DMA_CTRL_STS, USB_READ(REG_USBD_DMA_CTRL_STS)|0x00000020);
	
	schedule();
	
	//dump_massbuf(buf,length);

//quit:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&dev->wusbd_wait_b, &wait);
	return ;
}


static void	A_task_wake_up(wbusb_dev *dev)
{

	wake_up_interruptible(&dev->wusbd_wait_a);

	return ;
}


static void B_task_block(wbusb_dev *dev)
{


	DECLARE_WAITQUEUE(wait,	current);
	add_wait_queue(&dev->wusbd_wait_b, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	schedule();

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&dev->wusbd_wait_b, &wait);

	return ;
}

static void B_task_wake_up(wbusb_dev *dev)
{

	wake_up_interruptible(&dev->wusbd_wait_b);

	return ;
}


static void	C_task_block(wbusb_dev *dev)
{

	ENTER();
#if 1
	DECLARE_WAITQUEUE(wait,	current);
	add_wait_queue(&dev->wusbd_wait_c, &wait);
	set_current_state(TASK_INTERRUPTIBLE);

	schedule();

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&dev->wusbd_wait_c, &wait);

#else

	bCBlockFlag = 0;
  wait_event_interruptible(dev->wusbd_wait_c, bCBlockFlag != 0);
#endif

	LEAVE();

	return ;
}
#if 1
static void	C_task_wake_up(wbusb_dev *dev)
{
	ENTER();

#if 1
	//bCBlockFlag = 1;
	wake_up_interruptible(&dev->wusbd_wait_c);
	
	MSG2("bCBlockFlag:%x\n",bCBlockFlag);

#endif
	LEAVE();

	return ;
}
#endif


static void USB_All_Flag_Clear(wbusb_dev *dev)
{
	DBGOUT("%s %d\n",__FUNCTION__,__LINE__);
	dev->usb_enumstatus=0;
	dev->usb_getstatus=0;
	dev->usb_altsel=0;
}

int Disable_USB(wbusb_dev *dev)
{
	ENTER();
	USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) & ~(Phy_suspend | vbus_detect));//D+ low power off usb	
 	USB_WRITE(REG_AHBCLK, USB_READ(REG_AHBCLK) & ~USBD_CKE);
	LEAVE();
  
	return 0;
	
}



int Enable_USB(wbusb_dev *dev)
{
	ENTER();
	USB_WRITE(REG_AHBCLK, USB_READ(REG_AHBCLK) | USBD_CKE);
	USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) | (0x20 | Phy_suspend));		
	/* wait PHY clock ready */
#if defined(CONFIG_W55FA93_USBD_FIX_TO_FULL_SPEED)  	
	USB_WRITE(REG_USBD_OPER, 0x0);
#else
 	USB_WRITE(REG_USBD_EPA_MPS, 0x00000200);		// mps 512
#endif
  	while(1)
   	{     
#if defined(CONFIG_W55FA93_USBD_FIX_TO_FULL_SPEED)  
		if (USB_READ(REG_USBD_OPER) == 0x0)
#else			
    		if (USB_READ(REG_USBD_EPA_MPS) == 0x00000200)
#endif
			break;
   	}
	printk("PHY clock ready\n");
	USB_Init(dev);
	USB_Irq_Init(dev);

	mdelay(300);
      //  USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) | (0x20 | Phy_suspend | vbus_detect));//power on usb D+ high
	LEAVE();
  
  	return 0;	
}

void Reset_USB(wbusb_dev *dev)
{
	ENTER();

	//printk("Disable USB now ...\n");
	Disable_USB(dev);
	//printk("Enable USB now ...\n");
	Enable_USB(dev);
	
	LEAVE();
}



void usbdHighSpeedInit()
{
	usbdMaxPacketSize = 0x200;
	
	DBGOUT("usbdHighSpeedInit\n");
	
	/* bulk in */
	USB_WRITE(REG_USBD_EPA_RSP_SC, 0x00000000);		// auto validation
	USB_WRITE(REG_USBD_EPA_MPS, 0x00000200);		// mps 512
	USB_WRITE(REG_USBD_EPA_CFG, 0x0000001b);		// bulk in ep no 1
	USB_WRITE(REG_USBD_EPA_START_ADDR, 0x00000200);
	USB_WRITE(REG_USBD_EPA_END_ADDR, 0x000003ff);

	/* bulk out */
	USB_WRITE(REG_USBD_EPB_IRQ_ENB, 0x00000010);	// data pkt received  and outtokenb
	USB_WRITE(REG_USBD_EPB_RSP_SC, 0x00000000);		// auto validation
	USB_WRITE(REG_USBD_EPB_MPS, 0x00000200);		// mps 512
	USB_WRITE(REG_USBD_EPB_CFG, 0x00000023);		// bulk out ep no 2
	USB_WRITE(REG_USBD_EPB_START_ADDR, 0x00000400);
	USB_WRITE(REG_USBD_EPB_END_ADDR, 0x000007ff);
	

	DBGOUT("usbdHighSpeedInit end\n");
	
}

void usbdFullSpeedInit()
{
	usbdMaxPacketSize = 0x40;
	/* bulk in */
	USB_WRITE(REG_USBD_EPA_RSP_SC, 0x00000000);		// auto validation
	USB_WRITE(REG_USBD_EPA_MPS, 0x00000040);		// mps 64
	USB_WRITE(REG_USBD_EPA_CFG, 0x0000001b);		// bulk in ep no 1
	USB_WRITE(REG_USBD_EPA_START_ADDR, 0x00000100);
	USB_WRITE(REG_USBD_EPA_END_ADDR, 0x0000017f);

	/* bulk out */
	USB_WRITE(REG_USBD_EPB_IRQ_ENB, 0x00000010);	// data pkt received  and outtokenb
	USB_WRITE(REG_USBD_EPB_RSP_SC, 0x00000000);		// auto validation
	USB_WRITE(REG_USBD_EPB_MPS, 0x00000040);		// mps 64
	USB_WRITE(REG_USBD_EPB_CFG, 0x00000023);		// bulk out ep no 2
	USB_WRITE(REG_USBD_EPB_START_ADDR, 0x00000200);
	USB_WRITE(REG_USBD_EPB_END_ADDR, 0x0000027f);
}

static void USB_Init(wbusb_dev *dev)
{
	int	j;

	ENTER();
	dev->usb_devstate=0;
#if defined(CONFIG_W55FA93_USBD_FIX_TO_FULL_SPEED) 
	dev->usb_speedset = 1;// default at high speed mode //1 --- USB 1.1
#else
	dev->usb_speedset = 2;// default at high speed mode //1 --- USB 1.1
#endif
	dev->usb_address = 0;

	/* 
	 * configure mass storage interface endpoint 
	 */
	/* Device is in High Speed */
	if (dev->usb_speedset == 2)
	{
		usbdHighSpeedInit();
	}
	/* Device is in Full Speed */
	else if (dev->usb_speedset == 1)
	{
		usbdFullSpeedInit();
	}
	DBGOUT("start init usbd registers !\n");
	/*
	 * configure USB controller 
	 */
	USB_WRITE(REG_USBD_IRQ_ENB_L, 0x0f);	/* enable usb, cep, epa, epb interrupt */
	USB_WRITE(REG_USBD_IRQ_ENB, (USB_DMA_REQ | USB_RESUME | USB_RST_STS|USB_VBUS_STS));
#if defined(CONFIG_W55FA93_USBD_FIX_TO_FULL_SPEED) 
	USB_WRITE(REG_USBD_OPER, 0x00);//USB 1.1
#else
	USB_WRITE(REG_USBD_OPER, USB_HS);//USB 2.0
#endif
	USB_WRITE(REG_USBD_ADDR, 0);
	
	/* allocate 0xff bytes mem for cep */
	USB_WRITE(REG_USBD_CEP_START_ADDR, 0);
	USB_WRITE(REG_USBD_CEP_END_ADDR, 0x07f);
	USB_WRITE(REG_USBD_CEP_IRQ_ENB, (CEP_SUPPKT | CEP_STS_END));

	dev->epnum=ENDPOINTS;

	for	(j=0 ; j<dev->epnum	; j++)
	{
		dev->ep[j].EP_Num =	0xff;
		dev->ep[j].EP_Dir =	0xff;
		dev->ep[j].EP_Type = 0xff;

	}
	
	dev->bulk_len=0;//bulkin from PC data length

	LEAVE();

}

static void USB_ISR_RST(wbusb_dev	*dev)
{
	ENTER();
	
	dev->usb_devstate=0;
	dev->usb_speedset = 0;
	dev->usb_address = 0;
	
	dev->usb_remlen=0;
	dev->usb_remlen_flag=0;

	USB_All_Flag_Clear(dev);
	
	memset(dev->mass_cbwbuf,0,CBW_SIZE);
	
	//_usbd_DMA_Flag=0;
	A_task_wake_up(dev);
	B_task_wake_up(dev);
	dev->usb_less_mps=0;

	USB_WRITE(REG_USBD_DMA_CTRL_STS, 0x80);
	USB_WRITE(REG_USBD_DMA_CTRL_STS, 0x00);
	
	
	//memset(mass_wbuf,0,sizeof(mass_wbuf));
	//memset(mass_rbuf,0,sizeof(mass_rbuf));

	dev->usb_devstate = 1;		//default state

#if defined(CONFIG_W55FA93_USBD_FIX_TO_FULL_SPEED) 
	DBGOUT("Full speed after reset\n");
	dev->usb_speedset = USB_FULLSPEED;	//for full speed
	usbdFullSpeedInit();
#else
	if (USB_READ(REG_USBD_OPER) & 0x04)
	{
		DBGOUT("High speed after reset\n");
		dev->usb_speedset = USB_HIGHSPEED;	//for high speed
		usbdHighSpeedInit();
	}
	else
	{
		DBGOUT("Full speed after reset\n");
		dev->usb_speedset = USB_FULLSPEED;	//for full speed
		usbdFullSpeedInit();
	}
#endif

	
	USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x002);		//suppkt int
	USB_WRITE(REG_USBD_EPA_RSP_SC, USB_READ(REG_USBD_EPA_RSP_SC)|0x01);		// flush fifo
	USB_WRITE(REG_USBD_EPB_RSP_SC, USB_READ(REG_USBD_EPB_RSP_SC)|0x01);		// flush fifo
			
	USB_WRITE(REG_USBD_ADDR, 0);
	USB_WRITE(REG_USBD_IRQ_ENB, (USB_RST_STS|USB_SUS_REQ|USB_VBUS_STS));
	//USB_WRITE(REG_USBD_IRQ_STAT, 0x02);
	
	LEAVE();
	
}
void USB_ClassDataOut(wbusb_dev *dev)
{
	if(dev->usb_cmd_pkt.bRequest == /*BULK_ONLY_MASS_STORAGE_RESET*/0xFF)
	{
		if(dev->usb_cmd_pkt.wValue != 0 || dev->usb_cmd_pkt.wIndex != 0  || dev->usb_cmd_pkt.wLength != 0)
		{
			/* Invalid BOT MSC Reset Command */
			printk("Invalid BOT MSC Reset Command\n");
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, (USB_READ(REG_USBD_CEP_IRQ_ENB) | 0x03/*(CEP_SETUP_TK_IE | CEP_SETUP_PK_IE)*/));	
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, 0x02/*CEP_SEND_STALL*/);	
		}		
		else
		{
			/* Valid BOT MSC Reset Command */	
			printk("Valid BOT MSC Reset Command\n");	
			g_preventflag = 0;   
			g_dwCBW_flag = 0;
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, /*ZEROLEN*/0x20);	
		}
	}
	else
	{
		/* INvalid SET Command */
		printk("Invalid Set Command\n");	
		USB_WRITE(REG_USBD_CEP_IRQ_ENB, (USB_READ(REG_USBD_CEP_IRQ_ENB) | 0x03/*(CEP_SETUP_TK_IE | CEP_SETUP_PK_IE)*/));	
		USB_WRITE(REG_USBD_CEP_CTRL_STAT, 0x02/*CEP_SEND_STALL*/);		
	}

}


static void SDRAM_USB_Transfer(wbusb_dev *dev,u8	epname,u8* buf ,u32 Tran_Size)
{

	unsigned int volatile count=0;
	int volatile i=0, loop,len;
	
	
	loop = Tran_Size / USBD_DMA_LEN;
	
	if (epname == EP_A)
	{

		for (i=0; i<loop; i++)
		{
			dev->usb_dma_dir = Ep_In;
			dev->usb_less_mps = 0;
			USB_WRITE(REG_USBD_DMA_CTRL_STS, (USB_READ(REG_USBD_DMA_CTRL_STS)&0xe0) | 0x11);	// bulk in, write
			USB_WRITE(REG_USBD_EPA_IRQ_ENB, 0x08);
			while(!(USB_READ(REG_USBD_EPA_IRQ_STAT) & 0x02));
			start_write(dev,buf+i*USBD_DMA_LEN,USBD_DMA_LEN);
		}
		
		buf = buf + i * USBD_DMA_LEN;
		loop = Tran_Size % USBD_DMA_LEN;
		if (loop != 0)
		{
			count = loop / usbdMaxPacketSize;
			
			if(count!=0)
			{
				dev->usb_dma_dir = Ep_In;
				dev->usb_less_mps = 0;
				USB_WRITE(REG_USBD_DMA_CTRL_STS, (USB_READ(REG_USBD_DMA_CTRL_STS)&0xe0) | 0x11);	// bulk in, write
				USB_WRITE(REG_USBD_EPA_IRQ_ENB, 0x08);
				while(!(USB_READ(REG_USBD_EPA_IRQ_STAT) & 0x02));
				start_write(dev,buf,count*usbdMaxPacketSize);
				
				buf = buf + count*usbdMaxPacketSize;
			}
	
		}
		
		len = loop % usbdMaxPacketSize;
		if (len != 0)
		{
			dev->usb_dma_dir = Ep_In;
			dev->usb_less_mps = 1;
			USB_WRITE(REG_USBD_DMA_CTRL_STS, (USB_READ(REG_USBD_DMA_CTRL_STS)&0xe0) | 0x11);	// bulk in, write
			USB_WRITE(REG_USBD_EPA_IRQ_ENB, 0x08);
			while(!(USB_READ(REG_USBD_EPA_IRQ_STAT) & 0x02));
			start_write(dev,buf,len);
			
		}

	}//if end
	
	else if (epname == EP_B)
	{
		dev->usb_dma_dir = Ep_Out;
		dev->usb_less_mps = 0;
		
		USB_WRITE(REG_USBD_DMA_CTRL_STS, (USB_READ(REG_USBD_DMA_CTRL_STS) & 0xe0)|0x02);	// bulk out,read

		loop = Tran_Size / USBD_DMA_LEN;
		for (i=0; i<loop; i++)
		{
			start_read(dev,buf+i*USBD_DMA_LEN,USBD_DMA_LEN);
		}
		
		buf = buf + i * USBD_DMA_LEN;
		len = Tran_Size % USBD_DMA_LEN;
		if (len != 0)
		{
			start_read(dev,buf,len);
		}

	
	}

	return;

}



void write_data(wbusb_dev *dev,u8* buf,u32 length)
{

	if(!dev->usb_online)
	{
	  //printk("w device unplug !!!\n");
		return;
	}
	SDRAM_USB_Transfer(dev,EP_A,buf,length);

}

void read_data(wbusb_dev *dev,u8* buf,u32	length)
{

	if(!dev->usb_online)
	{
	  //	printk("r device unplug !!!\n");
		return;
	}
	SDRAM_USB_Transfer(dev,EP_B,buf,length);
}

static void USB_ISR_DMA(wbusb_dev *dev)
{
#ifdef FLUSH_CACHE
	flush_cache_all();
#endif
	if (dev->usb_dma_dir == Ep_Out)
	{
		if (dev->bulk_len!=0)
		{
			dev->bulk_len=0;
		}

		B_task_wake_up(dev);
		USB_WRITE(REG_USBD_EPB_IRQ_ENB, 0x10);//data receive interrupt enable
		//printk("\nDMA read irq\n");
	}
			
	if (dev->usb_dma_dir == Ep_In)
	{
		if (dev->usb_less_mps == 1)
		{
			USB_WRITE(REG_USBD_EPA_RSP_SC, (USB_READ(REG_USBD_EPA_RSP_SC)&0xf7)|0x00000040); // packet end
			dev->usb_less_mps = 0;
			
		}
		//printk("\nDMA write irq\n");
		A_task_wake_up(dev);
	}
}


void paser_irq_stat(int irq,wbusb_dev *dev)
{
	u32 volatile reg;
	DBGOUT("paser_irq_stat: 0x%x\n",irq);
	switch(irq)
	{
		case USB_SOF:
		
		break;

		case USB_VBUS_STS:
			reg = USB_READ(REG_USBD_PHY_CTL);	
			if(reg & BIT31)
			{
				dev->usb_online = 1;
				bIsUSBOnLine = 1;
				bIsPlug = 1;
				g_USB_Mode_Check = 1;	
				printk("USB plug!\n");
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
               			sysmgr_report(SYSMGR_STATUS_USBD_PLUG);
#endif
			}
			else
			{
				bIsPlug = 0;
				bIsUSBOnLine = 0;
				dev->usb_online = 0;
				g_USB_Mode_Check = 0;	
				printk("USB un-plug!\n");	
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
               			sysmgr_report(SYSMGR_STATUS_USBD_UNPLUG);
#endif
			}
		break;
		
		case USB_RST_STS:
			if(g_USB_Mode_Check)
			{
				g_USB_Mode_Check = 0;
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
				//printk("PC Connected!\n");
				sysmgr_report(SYSMGR_STATUS_USBD_CONNECT_PC);
#endif
			}
			USB_ISR_RST(dev);
		break;
		
		case USB_RESUME:
			dev->usb_online=1;
			if(g_USB_Mode_Check)
			{
				g_USB_Mode_Check = 0;
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
				//printk("PC Connected!\n");
				sysmgr_report(SYSMGR_STATUS_USBD_CONNECT_PC);
#endif
			}
			USB_WRITE(REG_USBD_IRQ_ENB, (USB_RST_STS|USB_SUS_REQ|USB_VBUS_STS));

		break;
		
		case USB_SUS_REQ:
		  if(dev == NULL) {
		    break;
		    //while(1);
		  }
			if(dev->usb_online)
			{
				if(g_USB_Mode_Check)
				{
					g_USB_Mode_Check = 0;
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
					//printk("PC Connected!\n");
					sysmgr_report(SYSMGR_STATUS_USBD_CONNECT_PC);
#endif
				}
				USB_All_Flag_Clear(dev);
				dev->usb_online=0;
				A_task_wake_up(dev);
				B_task_wake_up(dev);  
			}
			USB_WRITE(REG_USBD_IRQ_ENB, (USB_RST_STS | USB_RESUME|USB_VBUS_STS));
		
		break;
		
		case USB_HS_SETTLE:
			dev->usb_devstate = USB_FULLSPEED;		//default state
			dev->usb_speedset = USB_HIGHSPEED;		//for high speed
			dev->usb_address = 0;		//zero
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x002);
		break;
		
		case USB_DMA_REQ:
			USB_ISR_DMA(dev);
		break;
		
		case USABLE_CLK:
			DBGOUT("Usable Clock Interrupt\n");

		default:
			MSG2("irq: %d not handled !\n",irq);
		break;

	}

	USB_WRITE(REG_USBD_IRQ_STAT,irq);//clear irq bit

	return ;

}

static void Get_SetupPacket(wbusb_dev *dev,u32 temp)
{
	
	dev->usb_cmd_pkt.bmRequestType = (u8)temp & 0xff;
	dev->usb_cmd_pkt.bRequest = (u8)(temp >> 8) & 0xff;
	dev->usb_cmd_pkt.wValue = (u16)USB_READ(REG_USBD_SETUP3_2);
	dev->usb_cmd_pkt.wIndex = (u16)USB_READ(REG_USBD_SETUP5_4);
	dev->usb_cmd_pkt.wLength = (u16)USB_READ(REG_USBD_SETUP7_6);
		
}


static void USB_ISR_ControlPacket(wbusb_dev *dev)
{
	u32	temp;
	u32	ReqErr=0;
	
	temp = USB_READ(REG_USBD_SETUP1_0);
	
	Get_SetupPacket(dev,temp);
	// vendor command
	if (((dev->usb_cmd_pkt.bmRequestType & 0xE0) == 0xa0))
	{
		// clear flags
		USB_All_Flag_Clear(dev);
		dev->usb_enumstatus=CLASS_IN_Flag;

		USB_WRITE(REG_USBD_CEP_IRQ_STAT, /*CEP_STACOM_IS*/0x400);		/* Add by SPCheng */

	//	printk("class CMD !!!\n");
		USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x08);
		USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x08);		//suppkt int ,status and in token
		return;
	}
    	else if ((dev->usb_cmd_pkt.bmRequestType &0xE0) == 0x20)     //0x21 or 0x22 is Class Set Request
    	{
    		
		dev->usb_enumstatus=CLASS_OUT_Flag;  
	  	if (dev->usb_cmd_pkt.wLength == 0)
	   	{
	    		USB_ClassDataOut(dev);
	   	 }    	
	    	else	    
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x44/*CEP_OUT_TK_IE | CEP_DATA_RxED_IE*/);		//OUT_TK_IE        
		return;     
   	 } 	
	if (dev->usb_cmd_pkt.bmRequestType == 0x40)
	{
		// clear flags
		USB_All_Flag_Clear(dev);
		dev->usb_enumstatus=CLASS_IN_Flag;

		dev->bulk_len = 0;
		if (dev->usb_cmd_pkt.bRequest== 0xa0)
		{
			if (dev->usb_cmd_pkt.wValue == 0x12)
			{
				dev->bulk_len = dev->usb_cmd_pkt.wIndex;
				C_task_wake_up(dev);
				//Bulk_First_Flag = 1;
			}
			else if (dev->usb_cmd_pkt.wValue == 0x13)
			{
				// reset DMA
				USB_WRITE(REG_USBD_DMA_CTRL_STS, 0x80);
				USB_WRITE(REG_USBD_DMA_CTRL_STS, 0x00);
				USB_WRITE(REG_USBD_EPA_RSP_SC, USB_READ(REG_USBD_EPA_RSP_SC)|0x01);		// flush fifo
				USB_WRITE(REG_USBD_EPB_RSP_SC, USB_READ(REG_USBD_EPB_RSP_SC)|0x01);		// flush fifo

				USB_WRITE(REG_USBD_EPA_RSP_SC, USB_READ(REG_USBD_EPA_RSP_SC)|0x00000008);	// clear ep toggle
			   USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	// clear nak so that sts stage is complete
				//Bulk_First_Flag = 0;
			}
		}
		USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x400);
		return;
	}
	// standard request
	DBGOUT("USB_ISR_ControlPacket: %x\n",dev->usb_cmd_pkt.bRequest);
	switch (dev->usb_cmd_pkt.bRequest)
	{
		case USBR_GET_DESCRIPTOR:
			
			MSG2("USBR_GET_DESCRIPTOR\n");

			ReqErr = ((dev->usb_cmd_pkt.bmRequestType == 0x80) && ((dev->usb_cmd_pkt.wValue & 0xf000) == 0) 
			&& ((dev->usb_cmd_pkt.wValue & 0x80) == 0)) ? 0 : 1;

			if(ReqErr==1)
			{ 
				DBGOUT("GET_DESCRIPTOR => 0[%x], 2[%x], 4[%x], 6[%x]\n", USB_READ(REG_USBD_SETUP1_0), 
				USB_READ(REG_USBD_SETUP3_2), USB_READ(REG_USBD_SETUP5_4), USB_READ(REG_USBD_SETUP7_6));
				break;	//break this switch loop
			}

			switch ((dev->usb_cmd_pkt.wValue & 0xf00) >> 8) 
			{  
				//high byte contains desc type so need to shift???
				case USB_DT_DEVICE:
					MSG2("USB_DT_DEVICE\n");
					// clear flags
					USB_All_Flag_Clear(dev);
					dev->usb_enumstatus=GET_DEV_Flag;
					
					if(!dev->usb_online){

						dev->usb_online=1;
				
						A_task_wake_up(dev);
						B_task_wake_up(dev);
				
						//printk("USB plugin !\n");
					}

					if (dev->usb_cmd_pkt.wLength > DEVICE_DSCPT_LEN)
						dev->usb_cmd_pkt.wLength = DEVICE_DSCPT_LEN;
					MSG2("dev->usb_cmd_pkt.wLength: %d\n",dev->usb_cmd_pkt.wLength);
					break;

				case USB_DT_CONFIG:
					// clear flags
					MSG2("USB_DT_CONFIG\n");
					USB_All_Flag_Clear(dev);
					dev->usb_enumstatus=GET_CFG_Flag;

					if (dev->usb_cmd_pkt.wLength > CONFIG_DSCPT_LEN)
						dev->usb_cmd_pkt.wLength = CONFIG_DSCPT_LEN;
					MSG2("dev->usb_cmd_pkt.wLength: %d\n",dev->usb_cmd_pkt.wLength);
					break;

				case USB_DT_QUALIFIER:	// high-speed operation
					MSG2("USB_DT_QUALIFIER");
					// clear flags
					USB_All_Flag_Clear(dev);
					dev->usb_enumstatus=GET_QUL_Flag;

					if (dev->usb_cmd_pkt.wLength > QUALIFIER_DSCPT_LEN)
						dev->usb_cmd_pkt.wLength = QUALIFIER_DSCPT_LEN;

					break;

				case USB_DT_OSCONFIG:	// other speed configuration
					MSG2("USB_DT_OSCONFIG");
					// clear flags
					USB_All_Flag_Clear(dev);
					dev->usb_enumstatus=GET_OSCFG_Flag;

					if (dev->usb_cmd_pkt.wLength > OSCONFIG_DSCPT_LEN)
						dev->usb_cmd_pkt.wLength = OSCONFIG_DSCPT_LEN;

					break;

				case USB_DT_STRING:
					MSG2("USB_DT_STRING");
					// clear flags
					USB_All_Flag_Clear(dev);
					dev->usb_enumstatus=GET_STR_Flag;

					if ((dev->usb_cmd_pkt.wValue & 0xff) == 0)
					{
						if (dev->usb_cmd_pkt.wLength > STR0_DSCPT_LEN)
							dev->usb_cmd_pkt.wLength = STR0_DSCPT_LEN;
					}
					else if ((dev->usb_cmd_pkt.wValue & 0xff) == 1)
					{
						if (dev->usb_cmd_pkt.wLength > g_STR1_DSCPT_LEN)
							dev->usb_cmd_pkt.wLength = g_STR1_DSCPT_LEN;
					}
					else if ((dev->usb_cmd_pkt.wValue & 0xff) == 2)
					{
						if (dev->usb_cmd_pkt.wLength > g_STR2_DSCPT_LEN)
							dev->usb_cmd_pkt.wLength = g_STR2_DSCPT_LEN;
					}
					else if ((dev->usb_cmd_pkt.wValue & 0xff) == 3)
					{
						if (dev->usb_cmd_pkt.wLength > g_STR3_DSCPT_LEN)
							dev->usb_cmd_pkt.wLength = g_STR3_DSCPT_LEN;
					}
					break;

				default:
				  //printk("default!!!");
					ReqErr=1;
					break;
			}	//end of switch
			
			
			if (ReqErr == 0)
			{
				USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x408);
				USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x408);		//suppkt int ,status and in token
			}
		
		break;

		case USBR_SET_ADDRESS:
			DBGOUT("USBR_SET_ADDRESS!!!\n");
			ReqErr = ((dev->usb_cmd_pkt.bmRequestType == 0) && ((dev->usb_cmd_pkt.wValue & 0xff00) == 0)
			 && (dev->usb_cmd_pkt.wIndex == 0) && (dev->usb_cmd_pkt.wLength == 0)) ? 0 : 1;

			if ((dev->usb_cmd_pkt.wValue & 0xffff) > 0x7f)	//within 7f
			{
				ReqErr=1;	//Devaddr > 127
				DBGOUT("ERROR -  Request Error - Device address greater than 127\n");
			}

			if (dev->usb_devstate == 3)
			{
				ReqErr=1;	//Dev is configured
				DBGOUT("ERROR - Request Error - Device is already in configure state\n");
			}

			if (ReqErr==1) 
			{
				DBGOUT("ERROR - CepEvHndlr:USBR_SET_ADDRESS <= Request Error\n");
				break;		//break this switch loop
			}

			if(dev->usb_devstate == 2)
			{
				if(dev->usb_cmd_pkt.wValue == 0)
					dev->usb_devstate = 1;		//enter default state
				dev->usb_address = dev->usb_cmd_pkt.wValue;	//if wval !=0,use new address	
			}

			if(dev->usb_devstate == 1)
			{
				if(dev->usb_cmd_pkt.wValue != 0)
				{
					DBGOUT("get address !\n");
					dev->usb_address = dev->usb_cmd_pkt.wValue;
					dev->usb_devstate = 2;
				}
			}
			
			DBGOUT("dev->usb_address:%x\n",dev->usb_address);
			
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x400);
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x400);		// enable status complete interrupt
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	//clear nak so that sts stage is complete
			
			break;

		case USBR_GET_CONFIGURATION:
			
			DBGOUT("USBR_GET_CONFIGURATION!!!");
			ReqErr = ((dev->usb_cmd_pkt.bmRequestType == 0x80) && (dev->usb_cmd_pkt.wValue == 0) &&
			(dev->usb_cmd_pkt.wIndex == 0) && (dev->usb_cmd_pkt.wLength == 0x1) ) ? 0 : 1;

			if (ReqErr==1)
			{
				DBGOUT("ERROR - CepEvHndlr:USBR_GET_CONFIGURATION <= Request Error\n");
				break;	//break this switch loop
			}

			USB_All_Flag_Clear(dev);
			//usbdGetConfig=1;
			dev->usb_getstatus=GET_CONFIG_FLAG;
		
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x408);
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x408);		//status and in token
			
			break;

		case USBR_SET_CONFIGURATION:
			DBGOUT("\nUSBR_SET_CONFIGURATION\n");
			ReqErr = ((dev->usb_cmd_pkt.bmRequestType == 0) && ((dev->usb_cmd_pkt.wValue & 0xff00) == 0) &&
			((dev->usb_cmd_pkt.wValue & 0x80) == 0) && (dev->usb_cmd_pkt.wIndex == 0) && 
			(dev->usb_cmd_pkt.wLength == 0)) ? 0 : 1;

			if (dev->usb_devstate == 1)
			{
				DBGOUT("ERROR - Device is in Default state\n");
				ReqErr=1;
			}

			if ((dev->usb_cmd_pkt.wValue != 1) && (dev->usb_cmd_pkt.wValue != 0) )  //Only configuration one is supported
			{
				DBGOUT("ERROR - Configuration choosed is invalid\n");
				ReqErr=1;
			}
					
			if(ReqErr==1) 
			{
				DBGOUT("ERROR - CepEvHndlr:USBR_SET_CONFIGURATION <= Request Error\n");
				break;	//break this switch loop
			}
			
			dev->usb_confsel =dev->usb_cmd_pkt.wValue;
			
			if (dev->usb_cmd_pkt.wValue == 0)
			{
				dev->usb_devstate = 2;
			}
			else
			{
				dev->usb_devstate = 3;
			}

			USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	//clear nak so that sts stage is complete
			break;

		case USBR_GET_INTERFACE:
			DBGOUT("USBR_GET_INTERFACE!!!\n");
			ReqErr = ((dev->usb_cmd_pkt.bmRequestType == 0x81) && (dev->usb_cmd_pkt.wValue == 0) &&
			(dev->usb_cmd_pkt.wIndex == 0) && (dev->usb_cmd_pkt.wLength == 0x1)) ? 0 : 1;

			if ((dev->usb_devstate == 1) || (dev->usb_devstate == 2))
			{
				DBGOUT("ERROR - Device state is not valid\n");
				ReqErr=1;
			}

			if(ReqErr == 1) 
			{
				DBGOUT("ERROR - CepEvHndlr:USBR_GET_INTERFACE <= Request Error\n");
				break;	//break this switch loop
			}
			USB_All_Flag_Clear(dev);
			dev->usb_getstatus=GET_INTERFACE_FLAG;
			
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x408);
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x408);		//suppkt int ,status and in token
			break;

		case USBR_SET_INTERFACE:
			DBGOUT("\nUSBR_SET_INTERFACE\n\n");
			ReqErr = ((dev->usb_cmd_pkt.bmRequestType == 0x1) && ((dev->usb_cmd_pkt.wValue & 0xff80) == 0) 
			&& ((dev->usb_cmd_pkt.wIndex & 0xfff0) == 0) && (dev->usb_cmd_pkt.wLength == 0)) ? 0 : 1;

			if ((dev->usb_devstate == 0x3) && (dev->usb_cmd_pkt.wIndex == 0x0) && (dev->usb_cmd_pkt.wValue == 0x0))
				DBGOUT("interface choosen is already is use, so stall was not sent\n");
			else
			{
				DBGOUT("ERROR - Choosen interface was not supported\n");
				ReqErr=1;
			}

			if (ReqErr == 1)
			{
				DBGOUT("CepEvHndlr:USBR_SET_INTERFACE <= Request Error\n");
				break;	//break this switch loop
			}
			dev->usb_altsel = dev->usb_cmd_pkt.wValue;
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	//clear nak so that sts stage is complete
			break;

		case USBR_SET_FEATURE:
			DBGOUT("\nUSBR_SET_FEATURE\n\n");
			ReqErr = (((dev->usb_cmd_pkt.bmRequestType & 0xfc) == 0) && ((dev->usb_cmd_pkt.wValue & 0xfffc) == 0) 
			&& ((dev->usb_cmd_pkt.wIndex & 0xff00) == 0) && (dev->usb_cmd_pkt.wLength == 0)) ? 0 : 1;

			if (dev->usb_devstate == 1)
			{
				if((dev->usb_cmd_pkt.bmRequestType & 0x3) == 0x0) // Receipent is Device
				{
					if((dev->usb_cmd_pkt.wValue & 0x3) == TEST_MODE)
					{
						enabletestmode = 1;
						testselector = (dev->usb_cmd_pkt.wIndex >> 8);
					}
				}
				else
				{
					DBGOUT("ERROR - Device is in Default State\n");
					ReqErr=1;
				}
			}

			if (dev->usb_devstate == 2)
			{
				if (((dev->usb_cmd_pkt.bmRequestType & 0x03) == 2) && ((dev->usb_cmd_pkt.wIndex & 0xff) != 0))	//ep but not cep
				{
					DBGOUT("ERROR - Device is in Addressed State, but for noncep\n");
					ReqErr =1;
				}
				else if ((dev->usb_cmd_pkt.bmRequestType & 0x3) == 0x1)
				{
					DBGOUT("ERROR - Device is in Addressed State, but for interfac\n");
					ReqErr=1;
				}
			}

			if (ReqErr == 1) 
			{
				DBGOUT("CepEvHndlr:USBR_SET_FEATURE <= Request Error\n");
				break;	//break this switch loop
			}

			//check if recipient and wvalue are appropriate	
			DBGOUT("dev->usb_cmd_pkt.bmRequestType & 0x3: %x",dev->usb_cmd_pkt.bmRequestType & 0x3);
			switch(dev->usb_cmd_pkt.bmRequestType & 0x3)
			{
				case 0:		//device
					if ((dev->usb_cmd_pkt.wValue & 0x3) == DEVICE_REMOTE_WAKEUP)
						dev->usb_enableremotewakeup = 1;
					else if((dev->usb_cmd_pkt.wValue & 0x3) == TEST_MODE)
					{
						enabletestmode = 1;
						testselector = (dev->usb_cmd_pkt.wIndex >> 8);
					}
					else
					{
						DBGOUT("ERROR - No such feature for device\n");
						ReqErr=1;
					}
					break;

				case 1:		//interface
					break;

				case 2:		//endpoint
					if((dev->usb_cmd_pkt.wValue & 0x3) == ENDPOINT_HALT)
					{
						//dx->chgfea=dx->feature | ENDPOINT_HALT;
						if((dev->usb_cmd_pkt.wIndex & 0x3) == 0) //endPoint zero
						{
							dev->usb_haltep = 0;
						}
						else if((dev->usb_cmd_pkt.wIndex & 0x3) == 1) //endPoint one
						{
							dev->usb_haltep = 1;
						}
						else if((dev->usb_cmd_pkt.wIndex & 0x3) == 2) //endPoint two
						{
							dev->usb_haltep = 2;
						}
						else
						{
							DBGOUT("ERROR - Selected endpoint was not present\n");
							ReqErr=1;
						}
					}
					else
					{
						DBGOUT("ERROR - Neither device,endpoint nor interface was choosen\n");
						ReqErr=1;
					}
					break;

				default:
					break;
			}//device

			if (ReqErr == 0)
			{ 
				//dx->reqstart=(PUSHORT)dx->DeviceMem+0x800;//Zerolen pkt
				//dx->reqlen=0;
			}   

			if(enabletestmode == 1)
			{
				enabletestmode = 0;
				if(testselector == TEST_J)
					USB_WRITE(REG_USBD_MEM_TEST, TEST_J);
				else if(testselector==TEST_K)
					USB_WRITE(REG_USBD_MEM_TEST, TEST_K);
				else if(testselector==TEST_SE0_NAK)
					USB_WRITE(REG_USBD_MEM_TEST, TEST_SE0_NAK);
				else if(testselector==TEST_PACKET)
					USB_WRITE(REG_USBD_MEM_TEST, TEST_PACKET);
				else if(testselector==TEST_FORCE_ENABLE)
					USB_WRITE(REG_USBD_MEM_TEST, TEST_FORCE_ENABLE);
			}
					
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	//clear nak so that sts stage is complete
			break;

		case USBR_CLEAR_FEATURE:
			DBGOUT("\nUSBR_CLEAR_FEATURE\n\n");
			ReqErr = (((dev->usb_cmd_pkt.bmRequestType & 0xfc) == 0) && ((dev->usb_cmd_pkt.wValue & 0xfffc) == 0) 
			&& ((dev->usb_cmd_pkt.wIndex & 0xff00) == 0) && (dev->usb_cmd_pkt.wLength == 0)) ? 0 : 1;

			if (dev->usb_devstate == 1) 
			{
				DBGOUT("ERROR - Device is in default state\n");
				ReqErr =1;
			}

			if(ReqErr == 1) 
			{
				DBGOUT("ERROR - CepEvHndlr:USBR_CLEAR_FEATURE <= Request Error\n");
				break;	//break this switch loop
			}

			switch((dev->usb_cmd_pkt.bmRequestType & 0x3))
			{
				case 0:		//device 
					if((dev->usb_cmd_pkt.wValue & 0x3) == DEVICE_REMOTE_WAKEUP)
						dev->usb_disableremotewakeup = 1;
					else
					{
						DBGOUT("ERROR - No such feature for device\n");
						ReqErr=1;
					}
					break;

				case 1:		//interface
					break;

				case 2:		//endpoint
					if((dev->usb_cmd_pkt.wValue & 0x3) == ENDPOINT_HALT)
					{
						//dx->chgfea=dx->feature |ENDPOINT_HALT;
						if((dev->usb_cmd_pkt.wIndex & 0x3) == 0) //endPoint zero
							dev->usb_unhaltep = 0;
						else if((dev->usb_cmd_pkt.wIndex & 0x3) == 1) //endPoint one
							dev->usb_unhaltep = 1;
						else if((dev->usb_cmd_pkt.wIndex & 0x3) == 2) //endPoint two
							dev->usb_unhaltep = 2;
						else
						{
							DBGOUT("ERROR - endpoint choosen was not supported\n");
							ReqErr=1;
						}
					}
					else
					{
						DBGOUT("ERROR - Neither device,interface nor endpoint was choosen\n");
						ReqErr=1;
					}
					break;

				default:
					break;
			}	//device
			
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x400);
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x400);		//suppkt int ,status and in token
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, USB_READ(REG_USBD_CEP_CTRL_STAT) | 0x04);	// zero len
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	//clear nak so that sts stage is complete
			break;

		case USBR_GET_STATUS:
			DBGOUT("\nUSBR_GET_STATUS\n\n");
			//check if this is valid
			ReqErr = (((dev->usb_cmd_pkt.bmRequestType & 0xfc) == 0x80) && (dev->usb_cmd_pkt.wValue == 0) 
			&& ((dev->usb_cmd_pkt.wIndex & 0xff00) == 0) && (dev->usb_cmd_pkt.wLength == 0x2)) ? 0 : 1;

			if (dev->usb_devstate == 1)
			{
				DBGOUT("ERROR - Device is in default State\n");
				ReqErr =1;
			}
			if (dev->usb_devstate == 2)
			{
				if (((dev->usb_cmd_pkt.bmRequestType & 0x3) == 0x2) && (dev->usb_cmd_pkt.wIndex != 0))
				{
					DBGOUT("ERROR - Device is in Addressed State, but for noncep\n");
					ReqErr =1;
				}
				else if ((dev->usb_cmd_pkt.bmRequestType & 0x3) == 0x1)
				{
					DBGOUT("ERROR - Device is in Addressed State, but for interface\n");
					ReqErr =1;
				}
			}

			if (ReqErr == 1)
			{
				DBGOUT("ERROR - CepEvHndlr:USBR_GET_STATUS <= Request Error\n");
				break;	//break this switch loop
			}
			
			USB_All_Flag_Clear(dev);
			dev->usb_getstatus=GET_STATUS_FLAG;
			
			switch (dev->usb_cmd_pkt.bmRequestType & 0x3)
			{
				case 0:
					DBGOUT("value of dev->usb_remotewakeup is %d\n", dev->usb_remotewakeup);
					if (dev->usb_remotewakeup == 1)
					{
						dev->usbGetStatusData=0x3;
					}
					else 
					{
						dev->usbGetStatusData=0x1;
					}
					break;

				case 1:	//interface
					if (dev->usb_cmd_pkt.wIndex == 0)
					{
						DBGOUT("Status of interface zero\n");
						dev->usbGetStatusData=0x0;
					}
					else
					{
						DBGOUT("Error - Status of interface non zero\n");
						ReqErr=1;
					}
					break;

				case 2:	//endpoint
					if (dev->usb_cmd_pkt.wIndex == 0x0)
					{
						dev->usbGetStatusData=0x0;
						DBGOUT("Status of Endpoint zero\n");
					}
					else if (dev->usb_cmd_pkt.wIndex == 0x81)
					{
						DBGOUT("Status of Endpoint one\n");
						if (dev->usb_haltep == 1)
						{
							dev->usbGetStatusData=0x1;
						}
						else
						{
							dev->usbGetStatusData=0x0;
						}
					}
					else if (dev->usb_cmd_pkt.wIndex == 0x2)
					{
						DBGOUT("Status of Endpoint two\n");
						if (dev->usb_haltep == 2)
						{
							dev->usbGetStatusData=0x1;
						}
						else
						{
							dev->usbGetStatusData=0x0;
						}
					}
					else
					{
						DBGOUT("Error - Status of non-existing Endpoint\n");
						ReqErr=1;
					}
					break;

				default:
					break;
			}
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x408);
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x408);		//suppkt int ,status and in token
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	//clear nak so that sts stage is complete
			break;

		default:
			break;
	}//switch end
	
	if (ReqErr == 1)
	{
		DBGOUT("not supported request, send stall\n");
		USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_SEND_STALL);
	}
}


void USB_ISR_UpdateDevice(wbusb_dev *dev)
{
	//update this device for set requests
	switch(dev->usb_cmd_pkt.bRequest)
	{
		case USBR_SET_ADDRESS:
			MSG2("USB_ISR_UpdateDevice USBR_SET_ADDRESS: %x\n",dev->usb_address);
			USB_WRITE(REG_USBD_ADDR, dev->usb_address);
			break;

		case USBR_SET_CONFIGURATION:

			USB_WRITE(REG_USBD_EPA_RSP_SC, EP_TOGGLE);
			USB_WRITE(REG_USBD_EPB_RSP_SC, EP_TOGGLE);
		break;

		case USBR_SET_INTERFACE:
		break;

		case USBR_SET_FEATURE:
			if(dev->usb_haltep == 0)
				USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_SEND_STALL);
			else if(dev->usb_haltep == 1)
				USB_WRITE(REG_USBD_EPA_RSP_SC, EP_HALT);
			else if(dev->usb_haltep == 2)
				USB_WRITE(REG_USBD_EPB_RSP_SC, EP_HALT);
			else if(dev->usb_enableremotewakeup == 1)
			{
				dev->usb_enableremotewakeup = 0;
				dev->usb_remotewakeup = 1;
			}
			
		break;

		case USBR_CLEAR_FEATURE:
			if(dev->usb_unhaltep == 1 && dev->usb_haltep == 1)
			{
				USB_WRITE(REG_USBD_EPA_RSP_SC, 0x0);
				USB_WRITE(REG_USBD_EPA_RSP_SC, EP_TOGGLE);
				dev->usb_haltep = 4; // just for changing the haltep value
			}
			if(dev->usb_unhaltep == 2 && dev->usb_haltep == 2)
			{
				USB_WRITE(REG_USBD_EPB_RSP_SC, 0x0);
				USB_WRITE(REG_USBD_EPB_RSP_SC, EP_TOGGLE);
				dev->usb_haltep = 4; // just for changing the haltep value
			}
			else if(dev->usb_disableremotewakeup == 1)
			{
				dev->usb_disableremotewakeup=0;
				dev->usb_remotewakeup=0;
			}
		break;
		
		default:
		break;
	}//switch end
	
	return;
}

static void USB_ISR_SendDescriptor(wbusb_dev *dev)
{
	u32 temp_cnt;
	int volatile i;
	u32 *ptr=NULL;
	u8 *pRemainder;
	ENTER();
	DBGOUT("usb_enumstatus: %x,usb_getstatus:%x\n",dev->usb_enumstatus,dev->usb_getstatus);
	if ((dev->usb_enumstatus) || (dev->usb_getstatus))
	{
		DBGOUT("check...\n");
		if (dev->usb_remlen_flag == 0)
		{
			switch(dev->usb_enumstatus)
			{
				case GET_DEV_Flag:
					MSG2("check..._DeviceDescriptor\n");
					ptr = _DeviceDescriptor;
				break;
				
				case GET_QUL_Flag:
					MSG2("check..._QualifierDescriptor\n");
					ptr = _QualifierDescriptor;
				break;
						
				case GET_CFG_Flag:
					MSG2("check..._ConfigurationBlock/full: %d\n",dev->usb_speedset);
					if (dev->usb_speedset == 2)
						ptr = _ConfigurationBlock;
					else if (dev->usb_speedset == 1)
						ptr = _ConfigurationBlockFull;
					
				break;
				
				case GET_OSCFG_Flag:
					MSG2("check..._OSConfigurationBlock\n");
					ptr = _OSConfigurationBlock;
				break;
				
				case GET_STR_Flag:
					MSG2("check..._StringDescriptor\n");
					if ((dev->usb_cmd_pkt.wValue & 0xff) == 0)
						ptr = _StringDescriptor0;
					if ((dev->usb_cmd_pkt.wValue & 0xff) == 1)
						ptr = _StringDescriptor1;
					if ((dev->usb_cmd_pkt.wValue & 0xff) == 2)
						ptr = _StringDescriptor2;
					if ((dev->usb_cmd_pkt.wValue & 0xff) == 3)
						ptr = _StringDescriptor3;
				break;
				
				case CLASS_IN_Flag:
				{
					if (dev->usb_cmd_pkt.bRequest == 0xFE/*GET_MAX_LUN*/)
					{		
						if(dev->usb_cmd_pkt.wValue != 0 || dev->usb_cmd_pkt.wIndex != 0  || dev->usb_cmd_pkt.wLength != 1)
						{
							/* Invalid Get MaxLun Command */
							printk("Invalid GET MaxLun Command\n");
							USB_WRITE(REG_USBD_CEP_IRQ_ENB, (USB_READ(REG_USBD_CEP_IRQ_ENB) | 0x03/*(CEP_SETUP_TK_IE | CEP_SETUP_PK_IE)*/));	
							USB_WRITE(REG_USBD_CEP_CTRL_STAT, 0x02/*CEP_SEND_STALL*/);			
						}
						else
						{
							/* Valid Get MaxLun Command */
							printk("MaxLun %d\n",g_maxLun -1);
							USB_WRITEB(REG_USBD_CEP_DATA_BUF, g_maxLun -1);
							USB_WRITE(REG_USBD_IN_TRNSFR_CNT, 1);
						}
					}
					else
					{
						/* Invalid GET Command */	
						printk("Invalid GET Command\n");		
						USB_WRITE(REG_USBD_CEP_IRQ_ENB, (USB_READ(REG_USBD_CEP_IRQ_ENB) | 0x03/*(CEP_SETUP_TK_IE | CEP_SETUP_PK_IE)*/));	
						USB_WRITE(REG_USBD_CEP_CTRL_STAT, 0x02/*CEP_SEND_STALL*/);			
					}

				return;
				}
				
				default:
					break;
				
				
			}//switch end
			
			switch(dev->usb_getstatus)
			{
				case GET_CONFIG_FLAG:
					
					USB_WRITEB(REG_USBD_CEP_DATA_BUF, dev->usb_confsel);
					USB_WRITE(REG_USBD_IN_TRNSFR_CNT, 1);
				return;
				
				case GET_INTERFACE_FLAG:
					
					USB_WRITEB(REG_USBD_CEP_DATA_BUF, dev->usb_altsel);
					USB_WRITE(REG_USBD_IN_TRNSFR_CNT, 1);
				return;
						
				case GET_STATUS_FLAG:
					USB_WRITE(REG_USBD_CEP_DATA_BUF, dev->usbGetStatusData);
					USB_WRITE(REG_USBD_IN_TRNSFR_CNT, 2);
				return;
				
				default:
					break;
				
			}//switch end
			
		}
		else
			ptr = dev->usb_ptr;

		if (dev->usb_cmd_pkt.wLength > 0x40)
		{
			dev->usb_remlen_flag = 1;
			dev->usb_remlen = dev->usb_cmd_pkt.wLength - 0x40;
			dev->usb_cmd_pkt.wLength = 0x40;
		}
		else if (dev->usb_remlen != 0)
		{
			dev->usb_remlen_flag = 0;
			dev->usb_cmd_pkt.wLength = dev->usb_remlen;
			dev->usb_remlen = 0;
		}
		else
		{
			MSG2("remlen is zero !\n");
			dev->usb_remlen_flag = 0;
			dev->usb_remlen = 0;
		}
/*		
		temp_cnt = dev->usb_cmd_pkt.wLength / 2;
		MSG2("send desc dev->usb_cmd_pkt.wLength: %d\n",dev->usb_cmd_pkt.wLength);
		MSG2("send desc temp_cnt: %d,%x\n",temp_cnt,ptr);

		for (i=0; i<temp_cnt; i++)
		{
			printk("0x%x ",*ptr);
			USB_WRITEW(REG_USBD_CEP_DATA_BUF, *ptr++);	
		}

		if ((dev->usb_cmd_pkt.wLength % 2) != 0)
			USB_WRITEB(REG_USBD_CEP_DATA_BUF, *ptr & 0xff);
*/
		temp_cnt = dev->usb_cmd_pkt.wLength / 4;

		for (i=0; i<temp_cnt; i++)
			USB_WRITE(REG_USBD_CEP_DATA_BUF, *ptr++);
			
		temp_cnt = dev->usb_cmd_pkt.wLength% 4;
		
		if (temp_cnt)
		{
			pRemainder = (u8 *)ptr;
			for (i=0; i<temp_cnt; i++)
			{
				USB_WRITEB(REG_USBD_CEP_DATA_BUF, *pRemainder);
				pRemainder++;
			}			
		}	

		if (dev->usb_remlen_flag)
			dev->usb_ptr = ptr;

		USB_WRITE(REG_USBD_IN_TRNSFR_CNT, dev->usb_cmd_pkt.wLength);
		
	}
	
	LEAVE();
}

void USB_ISR_EP(u32 IrqSt)
{
	/* Receive data from HOST (CBW/Data) */
	if(IrqSt & 0x10 /*DATA_RxED_IS*/)
		g_dwCBW_flag = 1;

	return;
}

void paser_irq_cep(int irq,wbusb_dev *dev,u32 IrqSt)
{
	DBGOUT("paser_irq_cep: 0x%x\n",irq);
	switch(irq)
	{
		case CEP_SUPTOK:
		case CEP_NAK_SENT:
		case CEP_STALL_SENT:
		case CEP_USB_ERR:
		case CEP_BUFF_FULL:
		case CEP_BUFF_EMPTY:
			DBGOUT("CEP_SUPTOK...%x\n",irq);
		break;

		case CEP_SUPPKT:
			DBGOUT("CEP_SUPPKT\n");
			USB_ISR_ControlPacket(dev);

		break;
		
		case CEP_OUT_TOK:
			DBGOUT("CEP_OUT_TOK\n");
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x004);
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x400);
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x402);		// suppkt int//enb sts completion int
		
		return;
		
		case CEP_IN_TOK:
			DBGOUT("CEP_IN_TOK\n");
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x008);
			if (!(IrqSt & CEP_STS_END))
			{
				DBGOUT("call USB_ISR_SendDescriptor\n");
				USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x20);
				USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x20);
				USB_ISR_SendDescriptor(dev);
				
			}
			else
			{
				DBGOUT("abort\n");
				USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x020);
				USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x420);
			}

			return;
			
			
		return;
		
		case CEP_PING_TOK:
			DBGOUT("CEP_PING_TOK\n");
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x402);		// suppkt int//enb sts completion int
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x010);
		return;
		
		case CEP_DATA_TXD:
			MSG2("CEP_DATA_TXD: CEP_CNT:%d,IRQ_STAT: %x\n",USB_READ(REG_USBD_CEP_CNT),USB_READ(REG_USBD_CEP_IRQ_STAT) & 0x1000);
			
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x020);
			USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	// clear nak so that sts stage is complete
			USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x400);
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x402);		// suppkt int//enb sts completion int
			
		return;
		
		case CEP_DATA_RXD:
			DBGOUT("CEP_DATA_RXD\n");
			/* Data Packet receive(OUT) */
		   	if (dev->usb_enumstatus==CLASS_OUT_Flag)  	       	
				USB_ClassDataOut(dev);    	    	    
			else
			{
				USB_WRITE(REG_USBD_CEP_IRQ_STAT, 0x440);
				USB_WRITE(REG_USBD_CEP_CTRL_STAT, CEP_NAK_CLEAR);	// clear nak so that sts stage is complete
				USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x43e);						
			}
		return;
	
		case CEP_STS_END:
			DBGOUT("CEP_STS_END\n");
			//if (CLASS_CMD_Flag || _usbd_resume || GET_DEV_Flag)
			//	USBModeFlag = 1;
		
			USB_WRITE(REG_USBD_CEP_IRQ_ENB, 0x002);
			USB_ISR_UpdateDevice(dev);
			
		break;
		
		default:
			MSG2("irq: %d not handled !\n",irq);
			break;

	}

	USB_WRITE(REG_USBD_CEP_IRQ_STAT,irq);//clear irq bit

	return ;

}

static irqreturn_t wbusbd_irq(int irq, void *usbdev, struct pt_regs *r)
{
	wbusb_dev *dev;
	u32 volatile IrqStL, IrqEnL;
	u32 volatile  IrqSt, IrqEn;
	int i=0;
	
	dev=(wbusb_dev *)(usbdev);

	IrqStL = USB_READ(REG_USBD_IRQ_STAT_L);	/* get interrupt status */
	IrqEnL = USB_READ(REG_USBD_IRQ_ENB_L);
	
	DBGOUT("IrqStL: 0x%x\n",IrqStL);

	if (!(IrqStL & IrqEnL))
	{
		DBGOUT("Not our interrupt !\n");
		return IRQ_HANDLED;
	}
	
	if (IrqStL & IRQ_USB_STAT)
	{
		IrqSt = USB_READ(REG_USBD_IRQ_STAT);
		IrqEn = USB_READ(REG_USBD_IRQ_ENB);
		
		IrqSt = IrqSt & IrqEn ;
		
		if (!IrqSt)
		{
			DBGOUT("Not our interrupt stat !\n");
			USB_WRITE(REG_USBD_IRQ_STAT_L, IRQ_USB_STAT);
			return IRQ_HANDLED;
		}
		MSG2("IRQ_USB_STAT IrqSt: 0x%x\n",IrqSt);
		for(i=0;i<9;i++)
		{
			if(IrqSt&(1<<i))
			{
				paser_irq_stat(1<<i,dev);
				break;
			}
		}
		
		USB_WRITE(REG_USBD_IRQ_STAT_L, IRQ_USB_STAT);
		
		return IRQ_HANDLED;
		
		
	}//end IRQ_USB_STAT
	
	
	if (IrqStL & IRQ_CEP) 
	{
		IrqSt = USB_READ(REG_USBD_CEP_IRQ_STAT);
		IrqEn = USB_READ(REG_USBD_CEP_IRQ_ENB);
	
		IrqSt = IrqSt & IrqEn ;
		if (!IrqSt)
		{
		  //printk("Not our interrupt IRQ_CEP !\n");
			USB_WRITE(REG_USBD_IRQ_STAT_L, IRQ_CEP);
			return IRQ_HANDLED;
		}
		
		MSG2("IRQ_CEP IrqSt: 0x%x\n",IrqSt);
		
		for(i=0;i<13;i++)
		{
			if(IrqSt&(1<<i))
			{
				paser_irq_cep(1<<i,dev,IrqSt);
				break;
			}
		}
		
		USB_WRITE(REG_USBD_IRQ_STAT_L, IRQ_CEP);
		
		return IRQ_HANDLED;
		
	}
	
	if (IrqStL & IRQ_NCEP) 
	{
		
		if (IrqStL & 0x04)//endpoint A
		{
			IrqSt = USB_READ(REG_USBD_EPA_IRQ_STAT);
			IrqEn = USB_READ(REG_USBD_EPA_IRQ_ENB);
			
			USB_WRITE(REG_USBD_EPA_IRQ_STAT, 0x40);	//data pkt transmited
			USB_ISR_EP(IrqSt & IrqEn);
			USB_WRITE(REG_USBD_EPA_IRQ_STAT, IrqSt);
		}
		
		if (IrqStL & 0x08)//endpoint B
		{
			IrqSt = USB_READ(REG_USBD_EPB_IRQ_STAT);
			IrqEn = USB_READ(REG_USBD_EPB_IRQ_ENB);
			USB_ISR_EP(IrqSt & IrqEn);
			USB_WRITE(REG_USBD_EPB_IRQ_STAT, IrqSt);
		}
		if (IrqStL & 0x10)//endpoint C
		{
			DBGOUT(" Interrupt from Endpoint C \n");
			IrqSt = USB_READ(REG_USBD_EPC_IRQ_STAT);
			IrqEn = USB_READ(REG_USBD_EPC_IRQ_ENB);
			USB_ISR_EP(IrqSt & IrqEn);
			USB_WRITE(REG_USBD_EPC_IRQ_STAT, IrqSt);
		}

		if (IrqStL & 0x20)//endpoint D
		{
			DBGOUT(" Interrupt from Endpoint D \n");
			IrqSt = USB_READ(REG_USBD_EPD_IRQ_STAT);
			IrqEn = USB_READ(REG_USBD_EPD_IRQ_ENB);
			USB_ISR_EP(IrqSt & IrqEn);
			USB_WRITE(REG_USBD_EPD_IRQ_STAT, IrqSt);
		}

		if (IrqStL & 0x40)//endpoint E
		{
			DBGOUT(" Interrupt from Endpoint E \n");
			IrqSt = USB_READ(REG_USBD_EPE_IRQ_STAT);
			IrqEn = USB_READ(REG_USBD_EPE_IRQ_ENB);
			USB_ISR_EP(IrqSt & IrqEn);
			USB_WRITE(REG_USBD_EPE_IRQ_STAT, IrqSt);
		}

		USB_WRITE(REG_USBD_IRQ_STAT_L, IRQ_NCEP);
		
		return IRQ_HANDLED;
		
	}//if end

	return IRQ_HANDLED;


}

int	check_cbw(wbusb_dev	*dev,void* cbw)
{

	if(!dev->usb_online)
	{
		B_task_block(dev);
		return -1;
	}

	SDRAM_USB_Transfer(dev,EP_B,cbw,0x1f);
	
   return -1;

}

static int wbusb_installirq(wbusb_dev *dev)
{

	
	if (request_irq(IRQ_USBD, &wbusbd_irq, SA_INTERRUPT/*|SA_SAMPLE_RANDOM*/,
			driver_name, dev) != 0)	{
	  //	printk("Request irq error\n");
		return -EBUSY;
	}
	return 0;

}


static void USB_Irq_Init(wbusb_dev	*dev)
{

	ENTER();

	init_waitqueue_head(&dev->wusbd_wait_a);//write
	init_waitqueue_head(&dev->wusbd_wait_b);//read
	//init_waitqueue_head(&dev->wusbd_wait_c);//vcom vendor cmd

	if(wbusb_installirq(dev)==0)
	  ; // do nothing
	  //printk("install	usb	device irq ok\n");

	LEAVE();
	
	return ;
}

 wbusb_dev *the_controller=NULL;

 void* wbusb_register_driver(void)
 {
	if(the_controller)
		return the_controller;
	else
		return NULL;
 }


void wbusbd_probe(void)
{

	wbusb_dev	*dev = 0;

	ENTER();

	/* alloc, and start	init */
	dev	= kmalloc (sizeof *dev,	GFP_KERNEL);
	if (dev	== NULL){
		DBGOUT("kmalloc	error !\n");
		goto done;
	}

	memset(dev,	0, sizeof *dev);
	
#ifndef FLUSH_CACHE
	dev->mass_rbuf = dma_alloc_writecombine(NULL, (usb_data_buffer_size * 2 + usb_cbw_buffer_size+ usb_csw_buffer_size) ,
                                            &dev->mass_dma_rbuf, GFP_KERNEL); 
	if (!dev->mass_rbuf) {
			kfree(dev);
			return;
	}
	/* USB Buffer Base (Buffer for reading) */
	usb_buffer_base = dev->mass_dma_rbuf;

	/* Buffer for writting */
	dev->mass_wbuf = dev->mass_rbuf + usb_data_buffer_size;
	dev->mass_dma_wbuf = dev->mass_dma_rbuf + usb_data_buffer_size;

	/* Buffer for cbw */
	dev->mass_cbwbuf = dev->mass_wbuf + usb_data_buffer_size;
	dev->mass_dma_cbwbuf = dev->mass_dma_wbuf + usb_data_buffer_size;

	/* Buffer for csw */
	dev->mass_cswbuf = dev->mass_cbwbuf + usb_cbw_buffer_size;
	dev->mass_dma_cswbuf = dev->mass_dma_cbwbuf + usb_cbw_buffer_size;
#else
	dev->mass_cbwbuf=mass_cbwbuf;
	dev->mass_rbuf=mass_rbuf;
	dev->mass_wbuf=mass_wbuf;
#endif
	dev->rw_data=write_data;
	dev->rd_data=read_data;
	dev->wait_cbw=check_cbw;

	the_controller=dev;

	LEAVE();

done:

	return ;

}

static int wbusbd_mmap(struct file *filp, struct vm_area_struct *vma)
{
           int err;

           unsigned long off = vma->vm_pgoff << PAGE_SHIFT;

           unsigned long physical = usb_buffer_base + off;

           unsigned long vsize = vma->vm_end - vma->vm_start;

           vma->vm_flags |= (VM_IO | VM_RESERVED);

           vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

           if ((err = remap_pfn_range(vma, vma->vm_start, physical>>PAGE_SHIFT, vsize, vma->vm_page_prot)) < 0)

                      printk("IOREMAP ERR : %x\n",err);

           return 0;

}



static size_t usb_client_read(char __user *buf, size_t length)
{
	ENTER();
	MSG2("usb_client_read massbuf : %08x,   Count : %08x\n", buf, length);
	if(length == 0)
		return 0;

	read_data(the_controller, (char*)(the_controller->mass_rbuf), length);

	LEAVE();
	return length;
}

static size_t usb_client_write(const char __user *buf, size_t length)
{
	ENTER();
	MSG2("usb_client_write massbuf : %08x,   Count : %d\n", buf, length);

	if(length == 0)
		return 0;

	write_data(the_controller, (char*)(the_controller->mass_wbuf), length);

	LEAVE();

	return length;
}


static size_t usb_client_put_csw(const char __user *buf)
{
	ENTER();

	write_data(the_controller, (char*)(the_controller->mass_cswbuf), 0x0d);

	LEAVE();

	return 0;
}

static int usb_client_get_cbw(char *buf)
{
	int retval = 0;

	ENTER();

	retval = check_cbw(the_controller, (char *)(the_controller->mass_cbwbuf));
	
	LEAVE();

	return 0;
}

static int wbusb_open(struct inode *ip, struct file *filp)
{

  ENTER();
  //printk("open usb");
  USB_WRITE(REG_AHBCLK, USB_READ(REG_AHBCLK) | USBD_CKE);
  USB_WRITE(REG_AHBIPRST, USB_READ(REG_AHBIPRST) | UDCRST);
  USB_WRITE(REG_AHBIPRST, USB_READ(REG_AHBIPRST) & ~UDCRST);
  USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) | (0x20 | Phy_suspend));    // offset 0x704
  /* wait PHY clock ready */
#if defined(CONFIG_W55FA93_USBD_FIX_TO_FULL_SPEED)  
 USB_WRITE(REG_USBD_OPER, 0x0);
#else
 USB_WRITE(REG_USBD_EPA_MPS, 0x00000200);		// mps 512
#endif
  while(1)
   {     
#if defined(CONFIG_W55FA93_USBD_FIX_TO_FULL_SPEED)  
	if (USB_READ(REG_USBD_OPER) == 0x0)
#else
    	if (USB_READ(REG_USBD_EPA_MPS) == 0x00000200)
#endif
		break;
   }

  if(the_controller == NULL) {
    printk("the_controller == NULL\n");
    return(-ENODEV);

  }

  USB_Init(the_controller);
  USB_Irq_Init(the_controller);

  //__usleep(0xf000);
  mdelay(300);
 // USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) | (0x20 | Phy_suspend | vbus_detect));//power on usb D+ high
	bIsUSBOnLine = 0;
	bIsPlug = 0;
  LEAVE();
  return(0);

}

static int wbusb_release(struct inode *ip, struct file *filp)
{
	ENTER();

	USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) & ~vbus_detect);//D+ low power off usb
	while(USB_READ(REG_USBD_PHY_CTL) & vbus_detect);
	mdelay(200);
	USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) & ~Phy_suspend);//D+ low power off usb
	while(USB_READ(REG_USBD_PHY_CTL) & Phy_suspend);
	USB_WRITE(REG_AHBCLK, USB_READ(REG_AHBCLK) & ~USBD_CKE);
	mdelay(200);
	LEAVE();
	return 0;
}

static ssize_t wbusb_read(struct file *filp, char __user * buf, size_t count, loff_t *lp)
{
	return usb_client_read(buf, count);
}

static ssize_t wbusb_write(struct file *filp, const char __user * buf, size_t count, loff_t *lp)
{
	return usb_client_write(buf, count);
}

static int wbusb_ioctl(struct inode *ip, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	u32 reg,usb_bulkstate,temp;
	u8 tmp[4] __attribute__ ((aligned (4)));
	ENTER();

	if (_IOC_TYPE(cmd) != NUSBD_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > NUSBD_IOC_MAXNR) return -ENOTTY;


	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) return -EFAULT;
	
	
	switch(cmd) {
		case NUSBD_IOC_GETCBW:
			MSG2("NUSBD_IOC_GETCBW\n");
			retval = usb_client_get_cbw((char *)arg);
			break;			
		case NUSBD_GETVLEN:
			
			if(!the_controller->bulk_len)
			{
				MSG2("waiting PC\n");
				C_task_block(the_controller);
			}
			
			MSG2("wakeup now !\n");
			
			*(unsigned long*)arg=the_controller->bulk_len;
			break;
			
      	  	case NUSBD_IOC_SETLUN:
                	MSG2("NUSBD_IOC_SET_LUN\n");
               		copy_from_user((void *)&tmp, (const void *)arg, sizeof(tmp));
                	g_maxLun = (unsigned char)(*(unsigned char*)tmp);
                	break;
      	  	case NUSBD_IOC_GETCBW_STATUS:
                	MSG2("NUSBD_IOC_GET_CBWSTAT\n");
			copy_to_user((char *)arg, (char *)&g_dwCBW_flag, sizeof(g_dwCBW_flag));
                	break;

		case NUSBD_IOC_SET_BULKINFO:
			MSG2("NUSBD_IOC_SET_BULKINFO\n");
			break;

		case NUSBD_IOC_PLUG:
		 	printk("=> Release D+/D-\n");
		  	USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) | vbus_detect);//D+ low power off usb	
			bIsPlug = 1;
			bIsUSBOnLine = 1;
			break;
		case NUSBD_IOC_UNPLUG:
		  	printk("=> Force D+/D- to Low\n");
			USB_WRITE(REG_USBD_PHY_CTL, USB_READ(REG_USBD_PHY_CTL) & ~vbus_detect);//D+ low power off usb	
			bIsPlug = 0;
			bIsUSBOnLine = 0;
                        break;
      	  	case NUSBD_IOC_STATUS:
                	MSG2("NUSBD_IOC_GET_STAT\n");
			copy_to_user((char *)arg, (char *)&g_preventflag , sizeof(g_preventflag ));
                	break;
                case NUSBD_IOC_GET_CABLE_STATUS:			
                        MSG2("NUSBD_IOC_GET_CABLE_STATUS\n");
			reg = USB_READ(REG_USBD_PHY_CTL);	
			if(reg & BIT31)
			{
				printk("=> Cable is pluged\n");
				bIsPlug = 1;
			}
			else
			{
				bIsPlug = 0;
				printk("=> Cable is un-pluged\n");
			}

                        copy_to_user((char *)arg, (char *)&bIsPlug, sizeof(bIsPlug));
                        break;
		case NUSBD_REPLUG:
#ifndef FLUSH_CACHE				
			dma_free_coherent(NULL, CBW_SIZE, the_controller->mass_cbwbuf , the_controller->mass_dma_cbwbuf);
			dma_free_coherent(NULL, PAGE_SIZE, the_controller->mass_rbuf , the_controller->mass_dma_rbuf);
			dma_free_coherent(NULL, PAGE_SIZE, the_controller->mass_wbuf , the_controller->mass_dma_wbuf);
#endif		
			memset(the_controller,0, sizeof *the_controller);
			
#ifndef FLUSH_CACHE
		
		the_controller->mass_cbwbuf= (char *) dma_alloc_coherent(NULL, CBW_SIZE, (dma_addr_t *) &the_controller->mass_dma_cbwbuf, GFP_KERNEL);
		if (!the_controller->mass_cbwbuf) {
				return -ENOMEM;
		}
		
		the_controller->mass_rbuf= (char *) dma_alloc_coherent(NULL, PAGE_SIZE, (dma_addr_t *) &the_controller->mass_dma_rbuf, GFP_KERNEL);
		if (!the_controller->mass_rbuf) {
				return -ENOMEM;
		}
		
		the_controller->mass_wbuf= (char *) dma_alloc_coherent(NULL, PAGE_SIZE, (dma_addr_t *) &the_controller->mass_dma_wbuf, GFP_KERNEL);
		if (!the_controller->mass_wbuf) {
				return -ENOMEM;
		}
#else
		the_controller->mass_cbwbuf=mass_cbwbuf;
		the_controller->mass_rbuf=mass_rbuf;
		the_controller->mass_wbuf=mass_wbuf;
#endif

			the_controller->rw_data=write_data;
			the_controller->rd_data=read_data;
			the_controller->wait_cbw=check_cbw;
			Reset_USB(the_controller);
	
					
			break;			
      	  	case NUSBD_IOC_USB_READ_BUFFER_OFFSET:
                	MSG2("NUSBD_IOC_GET_BUFFER_OFFSET\n");
			temp = 0;
			copy_to_user((u32 *)arg, (u32 *)&temp , sizeof(temp));
                	break;	
      	  	case NUSBD_IOC_USB_WRITE_BUFFER_OFFSET:
                	MSG2("NUSBD_IOC_GET_BUFFER_OFFSET\n");
			temp = usb_data_buffer_size;
			copy_to_user((u32 *)arg, (u32 *)&temp , sizeof(temp));
                	break;	
      	  	case NUSBD_IOC_USB_CBW_BUFFER_OFFSET:
			temp = usb_data_buffer_size * 2;
			copy_to_user((u32 *)arg, (u32 *)&temp , sizeof(temp));
                	break;	
      	  	case NUSBD_IOC_USB_CSW_BUFFER_OFFSET:
			temp = usb_data_buffer_size * 2 + CBW_SIZE;
			copy_to_user((u32 *)arg, (u32 *)&temp , sizeof(temp));
                	break;	
      	  	case NUSBD_IOC_USB_WRITE_BUFFER_SIZE:
			copy_to_user((u32 *)arg, (u32 *)&usb_data_buffer_size , sizeof(usb_data_buffer_size));
                	break;				
      	  	case NUSBD_IOC_USB_READ_BUFFER_SIZE:
			copy_to_user((u32 *)arg, (u32 *)&usb_data_buffer_size , sizeof(usb_data_buffer_size));
                	break;	
      	  	case NUSBD_IOC_USB_CBW_BUFFER_SIZE:	
			copy_to_user((u32 *)arg, (u32 *)&usb_cbw_buffer_size , sizeof(usb_cbw_buffer_size));
			break;	
      	  	case NUSBD_IOC_USB_CSW_BUFFER_SIZE:	
			copy_to_user((u32 *)arg, (u32 *)&usb_csw_buffer_size , sizeof(usb_csw_buffer_size));
                	break;	
		case NUSBD_IOC_PUTCSW:
			retval = usb_client_put_csw((char *)arg);
			break;
		default:
			return -ENOTTY;
	}

	LEAVE();

	return retval;

}

struct cdev wbusb_cdev;

struct file_operations wbusb_fops = {
	.owner	= THIS_MODULE,
	.open	= wbusb_open,
	.release	= wbusb_release,
	.read 	= wbusb_read,
	.write	= wbusb_write,
	.ioctl 	= wbusb_ioctl,
	.mmap = wbusbd_mmap
};



static int __init wbusb_init (void)
{
	int retval;

	ENTER();
	w55fa93_gpio_configure(GPIO_GROUP_E,1);
	w55fa93_gpio_configure(GPIO_GROUP_E,10);
	w55fa93_gpio_configure(GPIO_GROUP_E,11);

	w55fa93_gpio_set_output(GPIO_GROUP_E,1);
	w55fa93_gpio_set_output(GPIO_GROUP_E,10);
	w55fa93_gpio_set_output(GPIO_GROUP_E,11);
	w55fa93_gpio_set(GPIO_GROUP_E,10,1);
	w55fa93_gpio_set(GPIO_GROUP_E,11,1);
	w55fa93_gpio_set(GPIO_GROUP_E,1,1);
	printk("GPIO GPIO Value need to set at boot up\n ");
	msleep(500);
	w55fa93_gpio_set(GPIO_GROUP_E,1,0);

	//default is mass storage device
	_DeviceDescriptor= (u32 *) Mass_DeviceDescriptor;
	_QualifierDescriptor=(u32 *)Mass_QualifierDescriptor;
	_ConfigurationBlock=(u32 *)Mass_ConfigurationBlock;
	_ConfigurationBlockFull=(u32 *)Mass_ConfigurationBlockFull;
	_OSConfigurationBlock=(u32 *)Mass_OSConfigurationBlock;
	_StringDescriptor0=(u32 *)Mass_StringDescriptor0;
	_StringDescriptor1=(u32 *)Mass_StringDescriptor1;
	_StringDescriptor2=(u32 *)Mass_StringDescriptor2;
	_StringDescriptor3=(u32 *)Mass_StringDescriptor3;
	g_STR1_DSCPT_LEN = Mass_StringDescriptor1[0] = sizeof(Mass_StringDescriptor1);
	g_STR2_DSCPT_LEN = Mass_StringDescriptor2[0] = sizeof(Mass_StringDescriptor2);
	g_STR3_DSCPT_LEN = Mass_StringDescriptor3[0] = sizeof(Mass_StringDescriptor3);

	wbusbd_probe();

	retval = register_chrdev_region(MKDEV(NUSBD_MAJOR, 0), 1, "usbclient");
	if ( retval < 0) {
		printk("wbusb : can not register chrdev region\n");
		return retval;
	}

	cdev_init(&wbusb_cdev, &wbusb_fops);
	wbusb_cdev.owner = THIS_MODULE;
	wbusb_cdev.ops = &wbusb_fops;
	retval = cdev_add(&wbusb_cdev, MKDEV(NUSBD_MAJOR, 0), 1);
	if ( retval < 0) {
		printk("wbusb : can not add wbusb_cdev\n");
		unregister_chrdev_region(MKDEV(NUSBD_MAJOR, 0), 1);
		return retval;
	}

	printk("W55FA93 USB 2.0 Device Driver Initilization Success\n");

	LEAVE();
	
	return 0;

}

static void __exit wbusb_cleanup (void)
{
	ENTER();

	cdev_del(&wbusb_cdev);
	unregister_chrdev_region(MKDEV(NUSBD_MAJOR, 0), 1);
	
	Disable_USB(the_controller);
	
	if(the_controller)
		kfree(the_controller);

	LEAVE();
	
	return ;
}

module_init(wbusb_init);
module_exit(wbusb_cleanup);
EXPORT_SYMBOL(bIsUSBOnLine);

MODULE_LICENSE("GPL");
