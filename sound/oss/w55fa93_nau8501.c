/* linux/sound/oss/nuc930_wau8501.c
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
 *	 2010/01/05		move NAU8501_ADC_Setup() to init function, delete set sample rate in NAU8501_ADC_Setup()
 *					NOTE:8501 is 256fs only
 */
 
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/arch/irqs.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>

#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/w55fa93_audio.h>
#include <asm/arch/w55fa93_i2s.h>

//#define NAU8501_DEBUG
//#define NAU8501_DEBUG_PRINT_LINE
//#define NAU8501_DEBUG_ENABLE_ENTER_LEAVE
//#define NAU8501_DEBUG_ENABLE_MSG
//#define NAU8501_DEBUG_ENABLE_MSG2

#ifdef NAU8501_DEBUG
#define PDEBUG(fmt, arg...)		printk(fmt, ##arg)
#else
#define PDEBUG(fmt, arg...)
#endif

#ifdef NAU8501_DEBUG_PRINT_LINE
#define PRN_LINE()				PDEBUG("[%-20s] : %d\n", __FUNCTION__, __LINE__)
#else
#define PRN_LINE()
#endif

#ifdef NAU8501_DEBUG_ENABLE_ENTER_LEAVE
#define ENTER()					PDEBUG("[%-20s] : Enter...\n", __FUNCTION__)
#define LEAVE()					PDEBUG("[%-20s] : Leave...\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef NAU8501_DEBUG_ENABLE_MSG
#define MSG(msg)				PDEBUG("[%-20s] : %s", __FUNCTION__, msg)
#else
#define MSG(msg)
#endif

#ifdef NAU8501_DEBUG_ENABLE_MSG2
#define MSG2(fmt, arg...)			PDEBUG("[%-20s] : "fmt, __FUNCTION__, ##arg)
#define PRNBUF(buf, count)		{int i;MSG2("CID Data: ");for(i=0;i<count;i++)\
									PDEBUG("%02x ", buf[i]);PDEBUG("\n");}
#else
#define MSG2(fmt, arg...)
#define PRNBUF(buf, count)
#endif

#define	WM_8000		(5<<1)
#define	WM_12000	(4<<1)
#define	WM_16000	(3<<1)
#define	WM_24000	(2<<1)	
#define	WM_32000	(1<<1)
#define	WM_48000	0
           
#define	WM_11025	(4<<1)
#define	WM_22050	(2<<1)
#define	WM_44100	0

unsigned int uPlayVolume, uRecVolume;
static struct i2c_client *save_client;

void NAU8501_Init(void);
void NAU8501_Exit(void);

static void Delay(int nCnt)
{
	 int volatile loop;
	for (loop=0; loop<nCnt*10; loop++);
}

void NAU8501_WriteData(char addr, unsigned short data)
{
	char i2cdata[2];
	
	PDEBUG("(I2C)%d:0x%x\n", addr, data);
	
	i2cdata[0] = ((addr << 1)  | (data >> 8));		//addr(7bit) + data(first bit)
	i2cdata[1] = (char)(data & 0x00FF);			//data(8bit)
	
	i2c_smbus_write_byte_data(save_client, i2cdata[0], i2cdata[1]);
}

void NAU8501_Set_Sample_Frequency(int uSample_Rate)
{
	unsigned short data=0;
	
	switch (uSample_Rate)
	{
		case AU_SAMPLE_RATE_8000:							//8KHz
			data =WM_8000 ;
			break;
		case AU_SAMPLE_RATE_11025:					//11.025KHz
			data = WM_11025 ;			
			break;
		case AU_SAMPLE_RATE_16000:						//16KHz
			data = WM_16000 ;			
			break;
		case AU_SAMPLE_RATE_22050:					//22.05KHz
			data = WM_22050;
			break;
		case AU_SAMPLE_RATE_24000:						//24KHz
			data = WM_24000;
			break;
		case AU_SAMPLE_RATE_32000:						//32KHz
			data = WM_32000 ;			
			break;
		case AU_SAMPLE_RATE_44100:					//44.1KHz
			data = WM_44100 ;
			break;
		case AU_SAMPLE_RATE_48000:						//48KHz
			data = WM_48000;

			break;
		default:break;
	}
	
	PDEBUG("NAU8501 Set SR =0x%x\n", data|1);
	NAU8501_WriteData(7,data); 		
}

void NAU8501_ADC_Setup(void)
{	
 	PDEBUG("==NAU8501_ADC_Setup==\n");
		
	NAU8501_WriteData(0, 0x000);
	Delay(0x200);
		
	NAU8501_WriteData(1, 0x01F);//R1 MICBEN BIASEN BUFIOEN VMIDSEL*/
	NAU8501_WriteData(2, 0x03F);//R2 power up ROUT1 LOUT1 
	NAU8501_WriteData(4, 0x010);//R4 select audio format(I2S format) and word length (16bits)
	NAU8501_WriteData(5, 0x000);//R5 companding ctrl and loop back mode (all disable)
	NAU8501_WriteData(6, 0x000);//R6 clock Gen ctrl(slave mode)
	NAU8501_WriteData(35, 0x008);//R35 enable noise gate
	NAU8501_WriteData(3, 0x00C);//R3 bypass		
	
	NAU8501_WriteData(45, 0x13f);//R45 PGA
	NAU8501_WriteData(46, 0x13f);//R46 PGA
}

static int NAU8501_Set_ADC_Volume(unsigned char ucLeftVol, unsigned char ucRightVol)
{
	unsigned short data=0;

#if 1	// update R45/R46

	if (ucLeftVol<=8)
		data = 0x100 | ucLeftVol;
	else if (ucLeftVol<=16&&ucLeftVol>8)
		data = 0x100 | ucLeftVol * 4;
	else if (ucLeftVol<=31&&ucLeftVol>16)
		data = 0x100 | (ucLeftVol * 8 + 7);
		
	data >>= 2;
	data &= 0x3F;
	data |= 0x80;	// update gain on 1st zero cross		
		
	NAU8501_WriteData(45, data & 0xFF);
			
	PDEBUG("NAU8501 regInde = 45, regData = 0x%x\n", data);	
			
	if (ucRightVol<=8)
		data = 0x100 | ucRightVol;
	else if (ucRightVol<=16&&ucRightVol>8)
		data = 0x100 | ucRightVol * 4;
	else if (ucRightVol<=31&&ucRightVol>16)
		data = 0x100 | (ucRightVol * 8 + 7);	
	
	data >>= 2;
	data &= 0x3F;
	data |= 0x80;	// update gain on 1st zero cross		
	data |= 0x100;	// R45/R46 register update

	NAU8501_WriteData(46, data);
		
	PDEBUG("NAU8501 regInde = 46, regData = 0x%x\n", data);			

#else
	
	if (ucLeftVol<=8)
		data = 0x100 | ucLeftVol;
	else if (ucLeftVol<=16&&ucLeftVol>8)
		data = 0x100 | ucLeftVol * 4;
	else if (ucLeftVol<=31&&ucLeftVol>16)
		data = 0x100 | (ucLeftVol * 8 + 7);
		
	NAU8501_WriteData(15, data & 0xFF);
			
//	NAU8501_WriteData(15, data);

	PDEBUG("NAU8501 regInde = 15, regData = 0x%x\n", data);	
			
	if (ucRightVol<=8)
		data = 0x100 | ucRightVol;
	else if (ucRightVol<=16&&ucRightVol>8)
		data = 0x100 | ucRightVol * 4;
	else if (ucRightVol<=31&&ucRightVol>16)
		data = 0x100 | (ucRightVol * 8 + 7);	
	
//	NAU8501_WriteData(16, data & 0xFF);	
	NAU8501_WriteData(16, data);
		
	PDEBUG("NAU8501 regInde = 16, regData = 0x%x\n", data);			
#endif	
	return 0;
}

int NAU8501SetRecVolume(unsigned int ucVolL, unsigned int ucVolR)
{
  	PDEBUG("NAU8501SetRecVolume = L:%d R:%d\n", ucVolL, ucVolR);
    NAU8501_Set_ADC_Volume((unsigned char)ucVolL, (unsigned char)ucVolR);
	
	return 0;
}

int bNAU8501Inited = 0;
/*
 *****************************************************************************
 *
 *	I2C Driver Interface
 *
 *****************************************************************************
 */

static struct i2c_driver NAU8501_driver;

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { 0x1A, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	.normal_i2c = normal_addr,
	.probe = ignore,
	.ignore = ignore,
};

static int NAU8501_probe(struct i2c_adapter *adap, int addr, int kind)
{
	struct i2c_client *client;
	int rc;

	client = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	strncpy(client->name, "NAU8501", 6);
	client->addr = addr;
	client->adapter = adap;
	client->driver = &NAU8501_driver;

	if ((rc = i2c_attach_client(client)) != 0) {
		kfree(client);
		return rc;
	}

	save_client = client;
		
	return 0;
}

static int NAU8501_attach(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, NAU8501_probe);
}

static int NAU8501_detach(struct i2c_client *client)
{
	int rc;

	if ((rc = i2c_detach_client(client)) == 0) {
		kfree(i2c_get_clientdata(client));		
	}
	return rc;
}

static struct i2c_driver NAU8501_driver = {
	.driver = {
		.name	= "NAU8501",
	},
	.id = 123,
	.attach_adapter = NAU8501_attach,
	.detach_client = NAU8501_detach,
};

static int NAU8501_init(void)
{	
	return i2c_add_driver(&NAU8501_driver);
}

static void NAU8501_exit(void)
{
	i2c_del_driver(&NAU8501_driver);
}

void NAU8501_Init(void)
{
	if(!bNAU8501Inited)
	{
		NAU8501_init();
		bNAU8501Inited = 1;
		NAU8501_ADC_Setup();
	}	
}
void NAU8501_Exit(void)
{
	NAU8501_exit();
}

