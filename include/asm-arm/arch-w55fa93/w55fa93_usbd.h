/* linux/include/asm-arm/arch-nuc900/nuc900_usb.h
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
 *   2006/08/26     vincen.zswan add this file for nuvoton nuc900 evb.
 */

 
#ifndef __W55FA93_USB_H
#define __W55FA93_USB_H

//#include <asm/arch/nuc900_types.h>

#define _NOMMU ((unsigned long) 0xc0000000) //Get Physical Address for DMA
#define NOMMU (~_NOMMU)
#define NON_CACHE_FLAG		0x80000000

#define USBD_DMA_LEN		0x10000
#define USB_HIGHSPEED	2
#define USB_FULLSPEED	1
#define EPSTADDR				0x400
#define CBW_SIZE	64
#define CSW_SIZE	32
#define ENDPOINTS	2

#define USB_WRITEB(addr,data)	outb(data,addr) 
#define USB_WRITEW(addr,data)	outw(data,addr) 
#define USB_WRITE(addr,data)	outl(data,addr) 
#define USB_READ(addr)	inl(addr)

/* length of descriptors */

#define MSC_DEVICE_DSCPT_LEN	0x12
#define MSC_CONFIG_DSCPT_LEN	0x20
#define MSC_STR0_DSCPT_LEN		0x04
#define MSC_STR1_DSCPT_LEN		0x16
#define MSC_STR2_DSCPT_LEN		0x34
#define MSC_QUALIFIER_DSCPT_LEN		0x0a
#define MSC_OSCONFIG_DSCPT_LEN		0x20


/* length of descriptors */
#define DEVICE_DSCPT_LEN	0x12
#define CONFIG_DSCPT_LEN	0x20
#define STR0_DSCPT_LEN		0x04
#define STR1_DSCPT_LEN		0x10
#define STR2_DSCPT_LEN		0x24
#define STR3_DSCPT_LEN		0x10
#define QUALIFIER_DSCPT_LEN		0x0a
#define OSCONFIG_DSCPT_LEN		0x20

/*
 * Standard requests
 */
#define USBR_GET_STATUS			0x00
#define USBR_CLEAR_FEATURE		0x01
#define USBR_SET_FEATURE		0x03
#define USBR_SET_ADDRESS		0x05
#define USBR_GET_DESCRIPTOR		0x06
#define USBR_SET_DESCRIPTOR		0x07
#define USBR_GET_CONFIGURATION	0x08
#define USBR_SET_CONFIGURATION	0x09
#define USBR_GET_INTERFACE		0x0A
#define USBR_SET_INTERFACE		0x0B
#define USBR_SYNCH_FRAME		0x0C

/*
 * Descriptor types
 */
#define USB_DT_DEVICE			0x01
#define USB_DT_CONFIG			0x02
#define USB_DT_STRING			0x03
#define USB_DT_INTERFACE		0x04
#define USB_DT_ENDPOINT			0x05
#define USB_DT_QUALIFIER		0x06
#define USB_DT_OSCONFIG			0x07
#define USB_DT_IFPOWER			0x08

//USB FEATURE SELECTOR			value
#define DEVICE_REMOTE_WAKEUP	1
#define ENDPOINT_HALT			0
#define TEST_MODE				2

//USB TEST MODES
#define TEST_J					0x01
#define TEST_K					0x02
#define TEST_SE0_NAK			0x03
#define TEST_PACKET				0x04
#define TEST_FORCE_ENABLE		0x05

//Driver definition of tokens
#define OUT_TOK		0x00
#define IN_TOK		0x01
#define SUP_TOK 	0x02
#define PING_TOK	0x03
#define NO_TOK		0x04

//Bit Definitions of IRQ_ENB/STAT register
#define	IRQ_USB_STAT		0x01
#define IRQ_CEP				0x02
#define IRQ_NCEP			0xfc   

//Definition of Bits in USB_IRQ_STS register

#define USB_SOF			0x01	
#define USB_RST_STS		0x02
#define	USB_RESUME		0x04
#define	USB_SUS_REQ		0x08
#define	USB_HS_SETTLE	0x10
#define	USB_VBUS_STS		0x100
#define	USB_DMA_REQ		0x20
#define USABLE_CLK		0x40

//Definition of Bits in USB_OPER register
#define USB_GEN_RES     0x1
#define USB_HS		    0x2
#define USB_CUR_SPD_HS  0x4

//Definition of Bits in CEP_IRQ_STS register

#define CEP_SUPTOK	 	0x0001
#define CEP_SUPPKT		0x0002
#define CEP_OUT_TOK		0x0004
#define CEP_IN_TOK		0x0008
#define CEP_PING_TOK	0x0010
#define CEP_DATA_TXD	0x0020
#define CEP_DATA_RXD	0x0040
#define CEP_NAK_SENT	0x0080
#define CEP_STALL_SENT	0x0100
#define CEP_USB_ERR		0x0200
#define CEP_STS_END		0x0400
#define CEP_BUFF_FULL	0x0800
#define CEP_BUFF_EMPTY	0x1000


//Definition of Bits in CEP_CTRL_STS register
#define CEP_NAK_CLEAR		0x00  //writing zero clears the nak bit
#define CEP_SEND_STALL		0x02

//Definition of Bits in EP_IRQ_STS register
#define EP_BUFF_FULL	0x001
#define EP_BUFF_EMPTY	0x002
#define EP_SHORT_PKT	0x004
#define EP_DATA_TXD		0x008
#define EP_DATA_RXD		0x010
#define EP_OUT_TOK		0x020
#define EP_IN_TOK		0x040
#define EP_PING_TOK		0x080
#define EP_NAK_SENT		0x100
#define EP_STALL_SENT	0x200
#define EP_USB_ERR		0x400

//Bit Definitons of EP_RSP_SC Register
#define EP_BUFF_FLUSH   0x01
#define EP_MODE         0x06
#define EP_MODE_AUTO	0x01
#define EP_MODE_MAN 	0x02
#define EP_MODE_FLY		0x03
#define EP_TOGGLE		0x8
#define EP_HALT			0x10
#define EP_ZERO_IN      0x20
#define EP_PKT_END      0x40


//Bit Definitons of EP_CFG Register
#define EP_VALID		0x01
#define EP_TYPE			0x06 //2-bit size	
#define EP_TYPE_BLK		0x01
#define EP_TYPE_INT		0x02
#define EP_TYPE_ISO		0x03
#define EP_DIR			0x08
#define EP_NO			0xf0 //4-bit size

/* Define Endpoint feature */
#define Ep_In        0x01
#define Ep_Out       0x00
#define Ep_Bulk      0x01
#define Ep_Int       0x02
#define Ep_Iso       0x03
#define EP_A         0x00
#define EP_B         0x01
#define EP_C         0x02
#define EP_D         0x03
#define EP_E         0x04
#define EP_F         0x05


#define	BULK_CBW	0x00
#define	BULK_IN		0x01
#define	BULK_OUT	0x02
#define	BULK_CSW	0x04

typedef struct usb_cmd
{
	u8	bmRequestType;
	u8	bRequest;
	u16	wValue;
	u16	wIndex;
	u16	wLength;
}	USB_CMD_T;

/* for mass storage */
/* flash format */
#define	M_4M		2
#define	M_8M		3
#define	M_16M		4
#define	M_32M		5
#define	M_64M		6
#define	M_128M		7
#define	M_256M		8
#define	M_512M		9
#define	M_1024M		10
#define	M_2048M		11

typedef struct Disk_Par_Inf
{
	u32	partition_size,
			data_location,
			bpb_location,
			fat_location,
			rootdir_location,
			free_size;
	u16	rootdirentryno,
			totalcluster,
			NumCyl;
	u8	NumHead,
			NumSector,
			capacity,
			fatcopyno,
			fatsectorno,
			fat16flg;
} Disk_Par_Info;

/* extern functions */

void FshBuf2CBW(void);
void CSW2FshBuf(void);
void Inquiry_Command(void);
void ModeSense10_Command(void);
void ModeSense6_Command(void);
void ModeSense_Command(void);
void ReqSen_Command(void);

void RdFmtCap_Command_DRAM(void);
void RdCurCap_Command_DRAM(void);
void Rd10_Command_DRAM(void);
void Rd10_Command_Int_DRAM(void);
void Wt10_Command_DRAM(void);

void MassBulk(void);
u8 Flash_Init(void);

void DRAM_Identify(u8 cap);
u8 format(void);
u8 WriteBootSector(u8 fat16);
void put_uint32(u32 value, u8 **p);
void put_uint16(u16 value, u8 **p);
u8 Write_Sector(u32 sector,u8 *buffer);

void usbdHighSpeedInit(void);
void usbdFullSpeedInit(void);

//////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct 
{
    u32 dwAddr;
    u32 dwValue;
}USB_INIT_T;

typedef struct 
{
 u8 Req_Type;
 u8 Req;
 u16 Value;
 u16 Index;
 u16 Length;
}USB_Vender_Cmd_Format_T __attribute__ ((aligned (4)));   // each field of vendor command

typedef struct {
    u8 EP_Num;
    u8 EP_Dir;
    u8 EP_Type;
    u8 p;
} USB_EP_Inf_T __attribute__ ((aligned (4)));

//WBUSB Structs
typedef struct _wbusb_dev
{
/* Descriptor pointer */
	u32 *pu32DevDescriptor;
	u32 *pu32QulDescriptor;
	u32 *pu32HSConfDescriptor;
	u32 *pu32FSConfDescriptor;	
	u32 *pu32OSConfDescriptor;
	u32 *pu32StringDescriptor[5];
	
	/* Descriptor length */
	u32	u32DevDescriptorLen;	
	u32	u32QulDescriptorLen;	
	u32	u32HSConfDescriptorLen;	
	u32	u32FSConfDescriptorLen;	
	u32	u32OSConfDescriptorLen;	
	u32 	u32StringDescriptorLen[5];



  	u8				epnum;
  	u8				usb_online;
  	
  	u8				usb_speedset;
  	u8				usb_devstate;
  	u8				usb_address;
  	u8				usb_haltep;
  	u8				usb_unhaltep;
  	u8				usb_disableremotewakeup;
  	u8				usb_enableremotewakeup;
  	u8				usb_remotewakeup;
  	u8				usb_remlen_flag;
  	u8				usb_remlen;
	u16 *			usb_ptr;
  	
  	u8				usb_dma_dir;
  	u8				usb_less_mps;
  	
  	void *				mass_wbuf;
  	dma_addr_t		mass_dma_wbuf;
  	
  	void *				mass_rbuf;
  	dma_addr_t		mass_dma_rbuf;
  	
  	void *				mass_cbwbuf;
  	dma_addr_t		mass_dma_cbwbuf;

  	void *				mass_cswbuf;
  	dma_addr_t		mass_dma_cswbuf;
  	
  	USB_CMD_T	usb_cmd_pkt;
  	
  	u8				bulk_first;
  	u8				bulkonlycmd;
  	u32				bulk_len;
  	
  	wait_queue_head_t	 wusbd_wait_a,wusbd_wait_b,wusbd_wait_c;
  	
 	USB_EP_Inf_T			ep[4];
 	USB_Vender_Cmd_Format_T	vcmd ;
 	u16	usb_confsel;
 	u16	usb_altsel;
 	u32	usbGetStatusData;
  
 	enum{
 		//GET_VENO_Flag=1,
		CLASS_IN_Flag=1,
		CLASS_OUT_Flag,
		GET_DEV_Flag,
		GET_CFG_Flag,
		GET_QUL_Flag,
		GET_OSCFG_Flag,
		GET_STR_Flag
 	}usb_enumstatus;
 	
 	enum{
 		GET_CONFIG_FLAG=1,
		GET_INTERFACE_FLAG,
		GET_STATUS_FLAG
	}usb_getstatus;
 	
	int (*wait_cbw)(struct _wbusb_dev *dev,void* cbw);
 	void (*rw_data)(struct _wbusb_dev *dev,u8* buf,u32 length);
 	void (*rd_data)(struct _wbusb_dev *dev,u8* buf,u32 length);
 	
}wbusb_dev __attribute__ ((aligned (4)));

#define NUSBD_MAJOR	250
#define NUSBD_IOC_MAXNR	20
#define NUSBD_IOC_MAGIC 'u'

#define NUSBD_IOC_GETCBW 	_IOR(NUSBD_IOC_MAGIC, 0, char *)
#define NUSBD_GETVLEN		_IOR(NUSBD_IOC_MAGIC, 1, unsigned long *)
#define NUSBD_REPLUG		_IOR(NUSBD_IOC_MAGIC, 2, char *)
#define NUSBD_IOC_SETLUN	_IOR(NUSBD_IOC_MAGIC, 3, char *) 
#define NUSBD_IOC_PLUG		_IO(NUSBD_IOC_MAGIC, 4)
#define NUSBD_IOC_UNPLUG	_IO(NUSBD_IOC_MAGIC, 5)
#define NUSBD_IOC_SET_BULKINFO	_IOW(NUSBD_IOC_MAGIC, 6, char *)
#define NUSBD_IOC_GETSTATUS	_IOR(NUSBD_IOC_MAGIC, 7, char *)
#define NUSBD_IOC_GET_CABLE_STATUS	_IOR(NUSBD_IOC_MAGIC, 8, char *)
#define NUSBD_IOC_GETCBW_STATUS		_IOR(NUSBD_IOC_MAGIC, 9, unsigned long)
#define NUSBD_IOC_STATUS 		_IOR(NUSBD_IOC_MAGIC, 10, char *)

#define NUSBD_IOC_USB_WRITE_BUFFER_OFFSET	_IOR(NUSBD_IOC_MAGIC, 11, u32 *)
#define NUSBD_IOC_USB_READ_BUFFER_OFFSET	_IOR(NUSBD_IOC_MAGIC, 12, u32 *)
#define NUSBD_IOC_USB_CBW_BUFFER_OFFSET		_IOR(NUSBD_IOC_MAGIC, 13, u32 *)
#define NUSBD_IOC_USB_CSW_BUFFER_OFFSET		_IOR(NUSBD_IOC_MAGIC, 14, u32 *)

#define NUSBD_IOC_USB_WRITE_BUFFER_SIZE		_IOR(NUSBD_IOC_MAGIC, 15, u32 *)
#define NUSBD_IOC_USB_READ_BUFFER_SIZE		_IOR(NUSBD_IOC_MAGIC, 16, u32 *)
#define NUSBD_IOC_USB_CBW_BUFFER_SIZE		_IOR(NUSBD_IOC_MAGIC, 17, u32 *)
#define NUSBD_IOC_USB_CSW_BUFFER_SIZE		_IOR(NUSBD_IOC_MAGIC, 18, u32 *)
#define NUSBD_IOC_PUTCSW			_IOW(NUSBD_IOC_MAGIC, 19, char *)

#endif /* __WBUSB_H */
