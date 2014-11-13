/* linux/sound/oss/nuc930_wau8822.c
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
 *   2007/01/26     YAchen add this file for nuvoton WM8978 i2s driver.WM8978_Exit
 *   2008/08/29     vincen.zswan add WM8978_Exit() for nuvoton WM8978 i2s driver.
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

//#define I2C_DEBUG
//#define I2C_DEBUG_PRINT_LINE
//#define I2C_DEBUG_ENABLE_ENTER_LEAVE
//#define I2C_DEBUG_ENABLE_MSG
//#define I2C_DEBUG_ENABLE_MSG2

#ifdef I2C_DEBUG
#define PDEBUG(fmt, arg...)		printk(fmt, ##arg)
#else
#define PDEBUG(fmt, arg...)
#endif

#ifdef I2C_DEBUG_PRINT_LINE
#define PRN_LINE()				PDEBUG("[%-20s] : %d\n", __FUNCTION__, __LINE__)
#else
#define PRN_LINE()
#endif

#ifdef I2C_DEBUG_ENABLE_ENTER_LEAVE
#define ENTER()					PDEBUG("[%-20s] : Enter...\n", __FUNCTION__)
#define LEAVE()					PDEBUG("[%-20s] : Leave...\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef I2C_DEBUG_ENABLE_MSG
#define MSG(msg)				PDEBUG("[%-20s] : %s", __FUNCTION__, msg)
#else
#define MSG(msg)
#endif

#ifdef I2C_DEBUG_ENABLE_MSG2
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

extern unsigned int uPlayVolume, uRecVolume;
int uSamplingRate = AU_SAMPLE_RATE_44100;
static struct i2c_client *save_client;

int NAU8822_WriteData(char addr, unsigned short data);
static int NAU8822_Set_DAC_Volume(unsigned char ucLeftVol, unsigned char ucRightVol);
static int NAU8822_Set_ADC_Volume(unsigned char ucLeftVol, unsigned char ucRightVol);
void NAU8822_Init(void);
void NAU8822_Exit(void);

static void Delay(int nCnt)
{
	 int volatile loop;
	for (loop=0; loop<nCnt*10; loop++);
}

void NAU8822_Set_Sample_Frequency(int uSample_Rate)
{
	unsigned short data=0;
	
	switch (uSample_Rate)
	{
		case AU_SAMPLE_RATE_8000:							//8KHz
			data =WM_8000 ;
			NAU8822_WriteData(6,0x20);	//only support 256fs
			break;
		case AU_SAMPLE_RATE_11025:					//11.025KHz
			data = WM_11025 ;
			NAU8822_WriteData(6,0x20);	//only support 256fs
			break;
		case AU_SAMPLE_RATE_16000:						//16KHz
			data = WM_16000 ;
			NAU8822_WriteData(6,0x20);	//only support 256fs
			break;
		case AU_SAMPLE_RATE_22050:					//22.05KHz
			data = WM_22050;

			break;
		case AU_SAMPLE_RATE_24000:						//24KHz
			data = WM_24000;

			break;
		case AU_SAMPLE_RATE_32000:						//32KHz
			data = WM_32000 ;
			NAU8822_WriteData(6,0x20);	//only support 256fs
			break;
		case AU_SAMPLE_RATE_44100:					//44.1KHz
			data = WM_44100 ;
			NAU8822_WriteData(6,0x20);	//only support 256fs
			break;
		case AU_SAMPLE_RATE_48000:						//48KHz
			data = WM_48000;

			break;
		default:break;
	}
	
	//uSamplingRate = uSample_Rate;
	//printk("NAU8822 Set SR =0x%x\n", data|1);
	NAU8822_WriteData(7,data|1); 	
	//NAU8822_WriteData(6,0x20); 
}

void NAU8822_DAC_Setup(void)
{	
//  printk("==NAU8822_DAC_Setup==\n");
			
	NAU8822_WriteData(0, 0x000);
	Delay(0x100);
	NAU8822_WriteData(1, 0x01F);//R1 OUT4MIXEN OUT3MIXEN MICBEN BIASEN BUFIOEN VMIDSEL
	NAU8822_WriteData(2, 0x1BF);//R2 power up ROUT1 LOUT1 
	NAU8822_WriteData(3, 0x06F);//R3 OUT4EN OUT3EN SPKNEN SPKPEN RMIXEN LMIXEN DACENR DACENL		
	NAU8822_WriteData(4, 0x010);//R4 select audio format(I2S format) and word length (16bits)
	NAU8822_WriteData(5, 0x000);//R5 companding ctrl and loop back mode (all disable)
	NAU8822_WriteData(6, 0x000);//R6 clock Gen ctrl(slave mode)
	NAU8822_WriteData(10, 0x008);//R10 DAC control (softmute disable, oversample select 64x (lowest power) )
	NAU8822_WriteData(43, 0x010);//For differential speaker
	NAU8822_WriteData(45, 0x139);//R10 DAC control (softmute disable, oversample select 64x (lowest power) )
	NAU8822_WriteData(50, 0x001);//R50 DACL2LMIX
	NAU8822_WriteData(51, 0x001);//R51 DACR2RMIX
	NAU8822_WriteData(49, 0x002);
	NAU8822_WriteData(54, 0x139);//For differential speaker
	
	//printk("set DAC sampleRate\n");
//	NAU8822_Set_Sample_Frequency(uSamplingRate);//R7 set sampling rate
}

void NAU8822_ADC_Setup(void)
{	
//  printk("==NAU8822_ADC_Setup==\n");
		
	NAU8822_WriteData(0, 0x000);
	Delay(0x100);
	NAU8822_WriteData(1, 0x01F);//R1 MICBEN BIASEN BUFIOEN VMIDSEL
	NAU8822_WriteData(2, 0x03F);//R2 power up ROUT1 LOUT1 
	NAU8822_WriteData(4, 0x010);//R4 select audio format(I2S format) and word length (16bits)
	NAU8822_WriteData(5, 0x000);//R5 companding ctrl and loop back mode (all disable)
	NAU8822_WriteData(6, 0x000);//R6 clock Gen ctrl(slave mode)
		
	NAU8822_WriteData(35, 0x008);//R35 enable noise gate		
	NAU8822_WriteData(3, 0x00C); //R3 bypass		
		
	NAU8822_WriteData(45, 0x13f);//R45 PGA
	NAU8822_WriteData(46, 0x13f);//R46 PGA

		
//	NAU8822_Set_Sample_Frequency(uSamplingRate);	
}

int NAU8822_WriteData(char addr, unsigned short data)
{
	char i2cdata[2];
	
	//printk("(I2C)%d:0x%x\n", addr, data);
	
	i2cdata[0] = ((addr << 1)  | (data >> 8));		//addr(7bit) + data(first bit)
	i2cdata[1] = (char)(data & 0x00FF);			//data(8bit)
	
	i2c_smbus_write_byte_data(save_client, i2cdata[0], i2cdata[1]);
	
	return 1;
}


int  NAU8822SetPlayVolume(unsigned int ucVolL, unsigned int ucVolR)
{    
//  printk("NAU8822SetPlayVolume = L:%d R:%d\n", ucVolL, ucVolR);
	NAU8822_Set_DAC_Volume((unsigned char)ucVolL, (unsigned char)ucVolR);
		
	return 0;
}


int  NAU8822SetRecVolume(unsigned int ucVolL, unsigned int ucVolR)
{
//  printk("NAU8822SetRecVolume = L:%d R:%d\n", ucVolL, ucVolR);
    NAU8822_Set_ADC_Volume((unsigned char)ucVolL, (unsigned char)ucVolR);
	
	return 0;
}

static int NAU8822_Set_DAC_Volume(unsigned char ucLeftVol, unsigned char ucRightVol)
{	//0~31
	
	/* R11 DACOUTL R12 DACOUTR, R52 HPOUTL R53 HPOUTR, R54 SPOUTL R55 SPOUTR */
	unsigned short data;
	if (ucLeftVol!=0)
		data = (1<<8) | (ucLeftVol*3 + 130);
	else
		data = (1<<8);
	NAU8822_WriteData(11, data);
		
	if (ucRightVol!=0)
		data = (1<<8) | (ucRightVol*3 + 130);
	else
		data = (1<<8);
	NAU8822_WriteData(12, data);
	
//HeadPhone
	if (ucLeftVol!=0)	//0~127
		data = (1<<8) | (ucLeftVol + 32);
	else
		data = (1<<8);
	NAU8822_WriteData(52, data);
		
	if (ucRightVol!=0)
		data = (1<<8) | (ucRightVol + 32);
	else
		data = (1<<8);
	NAU8822_WriteData(53, data);
	
//Speaker	
	if (ucLeftVol!=0)
		data = (1<<8) | (ucLeftVol + 32);
	else
		data = (1<<8);
	NAU8822_WriteData(54, data);
		
	if (ucRightVol!=0)
		data = (1<<8) | (ucRightVol + 32);
	else
		data = (1<<8);
		
	NAU8822_WriteData(55, data);
		
	return 0;
}	

static int NAU8822_Set_ADC_Volume(unsigned char ucLeftVol, unsigned char ucRightVol)
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
//		data = 0x100 | ucLeftVol * 8;
		data = 0x100 | (ucLeftVol * 8 + 7);		
	NAU8822_WriteData(15, data);
			
	if (ucRightVol<=8)
		data = 0x100 | ucRightVol;
	else if (ucRightVol<=16&&ucRightVol>8)
		data = 0x100 | ucRightVol * 4;
	else if (ucRightVol<=31&&ucRightVol>16)
		data = 0x100 | (ucRightVol * 8 + 7);	
	
	NAU8822_WriteData(16, data);
#endif
	
	return 0;
}

int bNAU8822Inited = 0;
/*
 *****************************************************************************
 *
 *	I2C Driver Interface
 *
 *****************************************************************************
 */

static struct i2c_driver NAU8822_driver;

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { 0x1A, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	.normal_i2c = normal_addr,
	.probe = ignore,
	.ignore = ignore,
};

static int NAU8822_probe(struct i2c_adapter *adap, int addr, int kind)
{
	struct i2c_client *client;
	int rc;

	client = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	strncpy(client->name, "NAU8822", 6);
	client->addr = addr;
	client->adapter = adap;
	client->driver = &NAU8822_driver;

	if ((rc = i2c_attach_client(client)) != 0) {
		kfree(client);
		return rc;
	}

	save_client = client;
		
	return 0;
}

static int NAU8822_attach(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, NAU8822_probe);
}

static int NAU8822_detach(struct i2c_client *client)
{
	int rc;

	if ((rc = i2c_detach_client(client)) == 0) {
		kfree(i2c_get_clientdata(client));		
	}
	return rc;
}

static struct i2c_driver NAU8822_driver = {
	.driver = {
		.name	= "NAU8822",
	},
	.id = 123,
	.attach_adapter = NAU8822_attach,
	.detach_client = NAU8822_detach,
};

static int NAU8822_init(void)
{	
	return i2c_add_driver(&NAU8822_driver);
}

static void NAU8822_exit(void)
{
	i2c_del_driver(&NAU8822_driver);
}

void NAU8822_Init(void)
{
	if(!bNAU8822Inited)
	{
		NAU8822_init();
		bNAU8822Inited = 1;
	}	
}
void NAU8822_Exit(void)
{
	NAU8822_exit();
}

