/*
 * drivers/sound/oss/nuc930_i2s.c
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
 *   2005/11/24     qfu add this file for nuvoton i2s
 *   2006/07/12     ygu modify this file for nuvoton i2s
 */
 
 
#include <linux/soundcard.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/w55fa93_audio.h>
#include <asm/arch/w55fa93_i2s.h>
#include <asm/arch/w55fa93_spu.h>

//#define I2S_DEBUG
//#define I2S_DEBUG_ENTER_LEAVE
//#define I2S_DEBUG_MSG
//#define I2S_DEBUG_MSG2

#ifdef I2S_DEBUG
#define DBG(fmt, arg...)			printk(fmt, ##arg)
#else
#define DBG(fmt, arg...)
#endif

#ifdef I2S_DEBUG_ENTER_LEAVE
#define ENTER()					DBG("[%-10s] : Enter\n", __FUNCTION__)
#define LEAVE()					DBG("[%-10s] : Leave\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef I2S_DEBUG_MSG
#define MSG(fmt)					DBG("[%-10s] : "fmt, __FUNCTION__)
#else
#define MSG(fmt)
#endif

#ifdef I2S_DEBUG_MSG2
#define MSG2(fmt, arg...)			DBG("[%-10s] : "fmt, __FUNCTION__, ##arg)
#else
#define MSG2(fmt, arg...)
#endif

static AUDIO_T	_tIIS;

#define Delay(time)					udelay(time * 10)

#define MSB_FORMAT	1
#define I2S_FORMAT  2

static int	 _bIISActive = 0;
static UINT32 _uIISCR = 0;

#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU	//if spu is playing, iis 
extern int _bSpuActive;
#endif

#define WMDEVID 0

#define UINT8		unsigned char
#define Disable_Int(n)    	outl(1 << (n),REG_AIC_MDCR)  //for mmu,use "outl"
#define Enable_Int(n)       outl(1 << (n),REG_AIC_MECR)  //for mmu,use "outl"

#define AUDIO_WRITE(addr, val)outl(val, addr)
#define AUDIO_READ(addr)inl(addr)  

// PLL clock settings
extern unsigned int w55fa93_apll_clock;
extern unsigned int w55fa93_upll_clock;

// ADC/DAC volume

static int i2sSetRecordVolume(UINT32 ucLeftVol, UINT32 ucRightVol);

/*----- set data format -----*/
static void I2S_Set_Data_Format(int choose_format)
{
	ENTER();
	switch(choose_format){
		case I2S_FORMAT: _uIISCR = _uIISCR | IIS;
				break;
		case MSB_FORMAT: _uIISCR = _uIISCR | MSB_Justified;
				break;
		default:break;
	}
	AUDIO_WRITE(REG_I2S_ACTL_I2SCON,_uIISCR);

	LEAVE();
}

/*----- set sample Frequency -----*/
static void I2S_Set_Sample_Frequency(int choose_sf)
{	
//	int outfs = 0;
	int clk_divider;
	unsigned int PllFreq = w55fa93_upll_clock;		
	unsigned int u32MCLK, u32ClockDivider, u32Remainder;
	
	ENTER();
	
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU	//if spu is playing, return directly
	if(_bSpuActive & SPU_PLAY_ACTIVE)
	{
		printk("SPU is playing ..\n");
		_uIISCR = _uIISCR | BIT16_256FS | SCALE_1;
		AUDIO_WRITE(REG_I2S_ACTL_I2SCON,_uIISCR);
		return;
	}
#endif
		
//	printk("PllFreq = %d\n", PllFreq);	
			
	_uIISCR &= 0xFF0F;		
//	printk("Sampling rate = %d\n", choose_sf);		

//#define CONFIG_I2S_MCLK_256WS	

#ifdef CONFIG_I2S_MCLK_256WS & CONFIG_W55FA93_AUDIO_CLOCK_FROM_APLL
	switch (choose_sf)	//all 16bit, 256fs
	{
		case AU_SAMPLE_RATE_8000:						//8KHz
			w55fa93_set_apll_clock(208896);
			u32MCLK = 12288/6;									
			break;
		case AU_SAMPLE_RATE_11025:						//11.025KHz
			w55fa93_set_apll_clock(169344);		
			u32MCLK = 16934/6;							
			break;
		case AU_SAMPLE_RATE_12000:						//12KHz
			w55fa93_set_apll_clock(208896);
			u32MCLK = 12288/4;
			break;
		case AU_SAMPLE_RATE_16000:						//16KHz
			w55fa93_set_apll_clock(208896);
			u32MCLK = 12288/3;
			break;
		case AU_SAMPLE_RATE_22050:						//22.05KHz
			w55fa93_set_apll_clock(169344);		
			u32MCLK = 16934/3;
			break;
		case AU_SAMPLE_RATE_24000:						//24KHz
			w55fa93_set_apll_clock(208896);
			u32MCLK = 12288/2;
			break;
		case AU_SAMPLE_RATE_32000:						//32KHz
			w55fa93_set_apll_clock(147456);
			u32MCLK = 16384/2;									
			break;
		case AU_SAMPLE_RATE_44100:						//44.1KHz
			w55fa93_set_apll_clock(169344);		
			u32MCLK = 16934*2/3;
			break;
		case AU_SAMPLE_RATE_48000:						//48KHz
			w55fa93_set_apll_clock(208896);
			u32MCLK = 12288;
			break;
		case AU_SAMPLE_RATE_64000:						//64KHz
			w55fa93_set_apll_clock(147456);
			u32MCLK = 16384;
			break;
		case AU_SAMPLE_RATE_88200:						//88.2KHz
			w55fa93_set_apll_clock(135500);		
			u32MCLK = 16934*4/3;
			break;
		case AU_SAMPLE_RATE_96000:						//96KHz
		default:		
			w55fa93_set_apll_clock(208896);
			u32MCLK = 12288*2;
			break;
	}
#else	
	switch (choose_sf)	//all 16bit, 256fs
	{
		case AU_SAMPLE_RATE_8000:							//8KHz
			_uIISCR = _uIISCR | BIT16_256FS | SCALE_6;
			break;
		case AU_SAMPLE_RATE_11025:					//11.025KHz
			_uIISCR = _uIISCR | BIT16_256FS | SCALE_6;
			break;
		case AU_SAMPLE_RATE_12000:						//12KHz
			_uIISCR = _uIISCR | BIT16_256FS | SCALE_4;
			break;
		case AU_SAMPLE_RATE_16000:						//16KHz
			_uIISCR = _uIISCR | BIT16_256FS | SCALE_3;
			break;
		case AU_SAMPLE_RATE_22050:					//22.05KHz
			_uIISCR = _uIISCR | BIT16_256FS | SCALE_3;
			break;
		case AU_SAMPLE_RATE_24000:						//24KHz
			_uIISCR = _uIISCR | BIT16_256FS | SCALE_2;
			break;
		case AU_SAMPLE_RATE_32000:						//32KHz
//			_uIISCR = _uIISCR | BIT16_256FS | SCALE_2;
			_uIISCR = _uIISCR | BIT16_384FS | SCALE_1;
			break;
		case AU_SAMPLE_RATE_44100:					//44.1KHz
//			_uIISCR = _uIISCR | BIT16_256FS | SCALE_31;
			_uIISCR = _uIISCR | BIT16_384FS | SCALE_1;			
			break;
		case AU_SAMPLE_RATE_48000:						//48KHz
			_uIISCR = _uIISCR | BIT16_256FS | SCALE_1;
			break;
		case AU_SAMPLE_RATE_64000:						//64KHz
			_uIISCR = _uIISCR | BIT16_384FS | SCALE_1;
			break;
		case AU_SAMPLE_RATE_88200:						//88.2KHz
			_uIISCR = _uIISCR | BIT16_384FS | SCALE_1;
			break;
		case AU_SAMPLE_RATE_96000:						//96KHz
		default:		
			_uIISCR = _uIISCR | BIT16_256FS | SCALE_1;
			break;
	}
#endif	// CONFIG_I2S_MCLK_256WS
		
	AUDIO_WRITE(REG_I2S_ACTL_I2SCON,_uIISCR);
//	DBG("sample rate=%d\n", choose_sf);
	
#ifdef CONFIG_W55FA93_AUDIO_CLOCK_FROM_APLL	
	
	#ifndef CONFIG_I2S_MCLK_256WS	
		if ( (choose_sf == AU_SAMPLE_RATE_48000) ||
			 (choose_sf == AU_SAMPLE_RATE_32000) ||
			 (choose_sf == AU_SAMPLE_RATE_24000) ||
			 (choose_sf == AU_SAMPLE_RATE_16000) ||
			 (choose_sf == AU_SAMPLE_RATE_12000) ||
			 (choose_sf == AU_SAMPLE_RATE_8000) )
		{
			w55fa93_set_apll_clock(208896);
			u32MCLK = 12288;									
		}
		else if ( (choose_sf == AU_SAMPLE_RATE_64000) ||
			 (choose_sf == AU_SAMPLE_RATE_96000) )
		{
			w55fa93_set_apll_clock(147456);
			u32MCLK = 24576;									
		}
		else if ( choose_sf == AU_SAMPLE_RATE_88200 )
		{
			w55fa93_set_apll_clock(169344);		
			u32MCLK = 16934*2;							
		}
		else
		{
			w55fa93_set_apll_clock(169344);		
			u32MCLK = 16934;							
		}		

	#endif		
		
		PllFreq = w55fa93_apll_clock;
		
	//	PllFreq *= 1000;
	//	printk("==>PllFreq=0x%x\n", PllFreq);
	//	printk("==>nSamplingRate=0x%x\n", nSamplingRate);	
	
		u32ClockDivider = PllFreq / u32MCLK;
	//	printk("==>u32ClockDivider=0x%x\n", u32ClockDivider);			
	
		AUDIO_WRITE(REG_CLKDIV1, (AUDIO_READ(REG_CLKDIV1) & (~ADO_S)) | (0x02 << 19) );	// SPU clock from APLL	
		AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) & (~ADO_N0));			
	
		AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) & (~ADO_N1));		
		u32ClockDivider &= 0xFF;
		u32ClockDivider	--;
		AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) | (u32ClockDivider<<24));	

#else	

//		PllFreq *= 1000;
		AUDIO_WRITE(REG_CLKDIV1, (AUDIO_READ(REG_CLKDIV1) & (~ADO_S)) | (0x03 << 19) );	// SPU clock from UPLL	
//		AUDIO_WRITE(REG_CLKDIV1, (AUDIO_READ(REG_CLKDIV1) & (~ADO_N0)) | (0x00 << 16)); // SPU clock = UPLL_Clock / 1		
		AUDIO_WRITE(REG_CLKDIV1, (AUDIO_READ(REG_CLKDIV1) & (~ADO_N0)) | (0x01 << 16)); // SPU clock = UPLL_Clock / 2		
		PllFreq /= 2;
	
	if ( (choose_sf%AU_SAMPLE_RATE_11025) == 0)		//eDRVI2S_FREQ_11025, eDRVI2S_FREQ_22050, eDRVI2S_FREQ_44100
		u32MCLK = 16934;					

	else
//		u32MCLK = 12288;	
		u32MCLK = 24576;	
			
//	printk("u32MCLK = %d\n", u32MCLK);
//	printk("PllFreq = %d\n", PllFreq);	
	
	u32ClockDivider = PllFreq / u32MCLK;
	u32Remainder = PllFreq % u32MCLK;
	u32Remainder *= 2;
	
//	printk("u32ClockDivider = %d\n", u32ClockDivider);
		
	if (u32Remainder <= u32MCLK)
		u32ClockDivider--;

//	printk("u32ClockDivider_2 = %d\n", u32ClockDivider);		
                     
	AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) & (~ADO_N1));		
	u32ClockDivider &= 0xFF;
	AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) | (u32ClockDivider<<24));	
#endif

	LEAVE();
}

static INT  I2S_reset(void)
{
	ENTER();

	// enable I2S pins
	AUDIO_WRITE(REG_GPBFUN, (AUDIO_READ(REG_GPBFUN) & (~0x3FF0)) | 0x1550);	// GPB[6:2] to be I2S signals
	AUDIO_WRITE(REG_MISFUN, AUDIO_READ(REG_MISFUN) & (~0x01));					// I2S interface for I2S, but not SPU

	// enable I2S enagine clock	
	AUDIO_WRITE(REG_AHBCLK, AUDIO_READ(REG_AHBCLK) | ADO_CKE | I2S_CKE | HCLK4_CKE);	// enable I2S engine clock 
	
#ifdef 	CONFIG_SOUND_W55FA93_PLAY_SPU	//if spu is playing, return directly
	// reset I2S engine 
	if(!(_bSpuActive & SPU_PLAY_ACTIVE))
	{
		AUDIO_WRITE(REG_AHBIPRST, AUDIO_READ(REG_AHBIPRST) | I2SRST);
		AUDIO_WRITE(REG_AHBIPRST, AUDIO_READ(REG_AHBIPRST) & ~I2SRST);	
	}
#else
	{
		AUDIO_WRITE(REG_AHBIPRST, AUDIO_READ(REG_AHBIPRST) | I2SRST);
		AUDIO_WRITE(REG_AHBIPRST, AUDIO_READ(REG_AHBIPRST) & ~I2SRST);	
	}
#endif   
    
	/* reset audio interface */
	AUDIO_WRITE(REG_I2S_ACTL_RESET,AUDIO_READ(REG_I2S_ACTL_RESET) | ACTL_RESET_);
	Delay(100);
	AUDIO_WRITE(REG_I2S_ACTL_RESET,AUDIO_READ(REG_I2S_ACTL_RESET) & ~ACTL_RESET_);
	Delay(100);
	
	/* reset IIS interface */
	AUDIO_WRITE(REG_I2S_ACTL_RESET,AUDIO_READ(REG_I2S_ACTL_RESET) | I2S_RESET);
	Delay(100);
	AUDIO_WRITE(REG_I2S_ACTL_RESET,AUDIO_READ(REG_I2S_ACTL_RESET) & ~I2S_RESET);
	Delay(100);
	
	// set Play & Record interrupt encountered in half of DMA buffer length
	AUDIO_WRITE(REG_I2S_ACTL_CON, (AUDIO_READ(REG_I2S_ACTL_CON) & (~R_DMA_IRQ_SEL)) | (0x01 << 14)); 	
	AUDIO_WRITE(REG_I2S_ACTL_CON, (AUDIO_READ(REG_I2S_ACTL_CON) & (~P_DMA_IRQ_SEL)) | (0x01 << 12)); 		
	
	/* enable audio controller and IIS interface */
	AUDIO_WRITE(REG_I2S_ACTL_RSR, R_FIFO_FULL | R_FIFO_EMPTY | R_DMA_RIA_IRQ);	
	AUDIO_WRITE(REG_I2S_ACTL_PSR, 0x1F);	
	
	AUDIO_WRITE(REG_I2S_ACTL_CON, AUDIO_READ(REG_I2S_ACTL_CON) | I2S_EN | P_DMA_IRQ | R_DMA_IRQ); 
//	AUDIO_WRITE(REG_I2S_ACTL_CON, AUDIO_READ(REG_I2S_ACTL_CON) | P_DMA_IRQ_EN | R_DMA_IRQ_EN); 	

		
//	printk("REG_I2S_ACTL_CON = %x \n", AUDIO_READ(REG_I2S_ACTL_CON));	
//	printk("REG_I2S_ACTL_RSR = %x \n", AUDIO_READ(REG_I2S_ACTL_RSR));		
		
		
	_uIISCR = 0;

	LEAVE();
	
	return 0;
}

void  i2sSetPlaySampleRate(INT nSamplingRate)
{
	_tIIS.nPlaySamplingRate = nSamplingRate;	

#ifdef CONFIG_SOUND_W55FA93_PLAY_NAU8822
	NAU8822_Set_Sample_Frequency(nSamplingRate);
#endif	
}

VOID  i2sSetRecordSampleRate(INT nSamplingRate)
{
	_tIIS.nRecSamplingRate = nSamplingRate;
	
#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8501	
	NAU8501_Set_Sample_Frequency(nSamplingRate);
#endif

#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8822
	NAU8822_Set_Sample_Frequency(nSamplingRate);
#endif
}

VOID i2sSetPlayCallBackFunction(AU_CB_FUN_T fnCallBack)
{
	_tIIS.fnPlayCallBack = fnCallBack;
}

VOID i2sSetRecordCallBackFunction(AU_CB_FUN_T fnCallBack)
{
	_tIIS.fnRecCallBack = fnCallBack;
}

static int i2sStartPlay(AU_CB_FUN_T fnCallBack, INT nSamplingRate, 
								INT nChannels)
{
	INT		nStatus ;

	ENTER();
	
	if (_bIISActive & I2S_PLAY_ACTIVE){
		MSG("IIS already playing\n");
		return ERR_I2S_PLAY_ACTIVE;		/* IIS was playing */
	}

        DBG("SamplingRate : %d  Channels : %d\n", nSamplingRate, nChannels);

	 if (_bIISActive == 0)
	{
		AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) & ~ PLAY_STEREO);
		if (nChannels == AU_CH_STEREO)
			AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) | PLAY_STEREO); 
	//	nStatus = I2S_reset();
	//	if (nStatus < 0)
	//		return nStatus;	
	}
	
	Enable_Int(IRQ_I2S);

	i2sSetPlayCallBackFunction(fnCallBack);
	i2sSetPlaySampleRate(nSamplingRate);

	I2S_Set_Sample_Frequency(nSamplingRate);
	I2S_Set_Data_Format(I2S_FORMAT);	
	
	/* set DMA play destination base address */
	AUDIO_WRITE(REG_I2S_ACTL_PDSTB, _tIIS.uPlayBufferAddr);  
	
	/* set DMA play buffer length */
	AUDIO_WRITE(REG_I2S_ACTL_PDST_LENGTH, _tIIS.uPlayBufferLength);

	MSG2("DMA Buffer : %x,  Length : %x\n", _tIIS.uPlayBufferAddr,_tIIS.uPlayBufferLength);

#ifdef CONFIG_SOUND_W55FA93_PLAY_NAU8822
//	NAU8822_DAC_Setup()
#endif

	/* start playing */
	MSG("IIS start playing...\n");
	AUDIO_WRITE(REG_I2S_ACTL_PSR, 0x1F);
	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) | I2S_PLAY);
	_bIISActive |= I2S_PLAY_ACTIVE;


	AUDIO_WRITE(REG_I2S_ACTL_CON, AUDIO_READ(REG_I2S_ACTL_CON) | P_DMA_IRQ_EN); 
	
//	printk("REG_I2S_ACTL_CON = %x \n", AUDIO_READ(REG_I2S_ACTL_CON));	
//	printk("REG_I2S_ACTL_RSR = %x \n", AUDIO_READ(REG_I2S_ACTL_RSR));		
	
	LEAVE();
	
	return 0;
}


static void i2sStopPlay(void)
{
	ENTER();
	
	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) & ~(PLAY_STEREO));

	if (!(_bIISActive & I2S_PLAY_ACTIVE))
		return;
	
	MSG("IIS stop playing\n");

	/* stop playing */
	while( AUDIO_READ(REG_I2S_ACTL_RESET) & I2S_PLAY )
		AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) & ~I2S_PLAY);

	/* disable audio play interrupt */
	if (!_bIISActive)
		Disable_Int(IRQ_I2S);
	
	_bIISActive &= ~I2S_PLAY_ACTIVE;

	LEAVE();
	
	return;
}

static int i2sStartRecord(AU_CB_FUN_T fnCallBack, INT nSamplingRate, 
							INT nChannels)
{
	INT		nStatus;

	ENTER();

	if (_bIISActive & I2S_REC_ACTIVE)
		return ERR_I2S_REC_ACTIVE;		/* IIS was recording */

	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) & ~(RECORD_LEFT_CHNNEL & RECORD_RIGHT_CHNNEL));
	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) | RECORD_LEFT_CHNNEL);
	if (nChannels != 1)
		AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) | RECORD_RIGHT_CHNNEL);			

//	printk("i2s recording channel No. = %d \n", nChannels);
//	printk("REG_I2S_ACTL_RESET value = %x \n", AUDIO_READ(REG_I2S_ACTL_RESET ));	
//	printk("REG_I2S_ACTL_CON value = %x \n", AUDIO_READ(REG_I2S_ACTL_CON) );		

	/*if (_bIISActive == 0)
	{
		nStatus = I2S_reset();
		if (nStatus < 0)
			return nStatus;	
	}*/

	Enable_Int(IRQ_I2S);
	
	i2sSetRecordCallBackFunction(fnCallBack);
	i2sSetRecordSampleRate(nSamplingRate);	

	I2S_Set_Sample_Frequency(nSamplingRate);
	I2S_Set_Data_Format(I2S_FORMAT);

#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8822	
//	NAU8822_ADC_Setup();
#endif

#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8501	
//	NAU8501_ADC_Setup();	
#endif
	
	/* set DMA record destination base address */
	AUDIO_WRITE(REG_I2S_ACTL_RDSTB, _tIIS.uRecordBufferAddr);  //uRecordBufferAddr change to physical address
	
	/* set DMA record buffer length */
	AUDIO_WRITE(REG_I2S_ACTL_RDST_LENGTH, _tIIS.uRecordBufferLength);

	/* start recording */
	MSG("IIS start recording...\n");
//	AUDIO_WRITE(REG_I2S_ACTL_RSR, 0x7);
	AUDIO_WRITE(REG_I2S_ACTL_RSR, R_FIFO_FULL | R_FIFO_EMPTY | R_DMA_RIA_IRQ);
		
	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) | I2S_RECORD);
	_bIISActive |= I2S_REC_ACTIVE;

	LEAVE();
	
	return 0;
}


static void i2sStopRecord(void)
{
	ENTER();

	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) & ~(RECORD_RIGHT_CHNNEL | RECORD_LEFT_CHNNEL));

	if (!(_bIISActive & I2S_REC_ACTIVE))
		return;
	
	MSG("IIS stop recording\n");

	/* disable audio record interrupt */
//	if (!_bIISActive)
		Disable_Int(IRQ_I2S);

	/* stop recording */
	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) & ~I2S_RECORD);

	_bIISActive &= ~I2S_REC_ACTIVE;

	LEAVE();
	
	return;
}


extern int  WM8978SetPlayVolume(unsigned int ucVolL, unsigned int ucVolR);
static int i2sSetPlayVolume(UINT32 ucLeftVol, UINT32 ucRightVol)  //0~31
{
	ENTER();

	if (ucLeftVol>31)
		ucLeftVol=31;
	if (ucRightVol>31)
		ucRightVol=31;

	DBG("Set IIS Play volume to : %d-%d\n", ucLeftVol, ucRightVol);
	
	_tIIS.sPlayVolume = (ucRightVol<<8) | ucLeftVol;
	
#ifdef CONFIG_SOUND_W55FA93_PLAY_NAU8822
	 NAU8822SetPlayVolume(ucLeftVol, ucRightVol);    
#endif

	DBG("_tIIS.sPlayVolume=%x\n",_tIIS.sPlayVolume);	
	LEAVE();
	
	return 0;
}

static int i2sSetRecordVolume(UINT32 ucLeftVol, UINT32 ucRightVol)
{
	ENTER();

	if (ucLeftVol>31)
		ucLeftVol=31;
	if (ucRightVol>31)
		ucRightVol=31;

	DBG("Set IIS Record volume to : %d-%d\n", ucLeftVol, ucRightVol);
	
	_tIIS.sRecVolume = (ucRightVol<<8) | ucLeftVol;

#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8822
	 NAU8822SetRecVolume(ucLeftVol, ucRightVol);    
#endif

#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8501
	 NAU8501SetRecVolume(ucLeftVol, ucRightVol);    
#endif
	
	DBG("_tIIS.sRecVolume=%x\n",_tIIS.sRecVolume);
	LEAVE();

	return 0;

}

static void i2sSetPlayBuffer(UINT32 uDMABufferAddr, UINT32 uDMABufferLength)
{
	ENTER();

	_tIIS.uPlayBufferAddr = uDMABufferAddr;  
	_tIIS.uPlayBufferLength = uDMABufferLength;

	LEAVE();
}

static void i2sSetRecordBuffer(UINT32 uDMABufferAddr, UINT32 uDMABufferLength)
{
	ENTER();

	_tIIS.uRecordBufferAddr = uDMABufferAddr; 
	_tIIS.uRecordBufferLength = uDMABufferLength;

	LEAVE();
}

static void I2S_play_isr(void)
{
	int bPlayLastBlock = 0;
	
	ENTER();

	MSG2("[DMA:S:%x,L:%x,C:%x]\n", 
		AUDIO_READ(REG_I2S_ACTL_PDSTB),
		AUDIO_READ(REG_I2S_ACTL_PDST_LENGTH),
		AUDIO_READ(REG_I2S_ACTL_PDSTC));
		

	if (AUDIO_READ(REG_I2S_ACTL_PSR) & P_DMA_RIA_IRQ)
	{
		AUDIO_WRITE(REG_I2S_ACTL_PSR, P_DMA_RIA_IRQ);		
		
		if (!(AUDIO_READ(REG_I2S_ACTL_PSR) & P_DMA_RIA_SN))	// == 0
		{
			bPlayLastBlock = _tIIS.fnPlayCallBack(_tIIS.uPlayBufferAddr, 
										_tIIS.uPlayBufferLength/2);  
		}
		else 												// == BIT5
		{
			bPlayLastBlock = _tIIS.fnPlayCallBack(_tIIS.uPlayBufferAddr + _tIIS.uPlayBufferLength/2, 
										_tIIS.uPlayBufferLength/2);  
		}

		/* check whether the next block is ready. If not, stop play */
	
	//	if (bPlayLastBlock)
	//	{
	//		AUDIO_WRITE(REG_I2S_ACTL_PSR, P_DMA_MIDDLE_IRQ | P_DMA_END_IRQ);
	//	}
		
	}				
	AUDIO_WRITE(REG_I2S_ACTL_CON, AUDIO_READ(REG_I2S_ACTL_CON) | P_DMA_IRQ);


	LEAVE();

}		


static void  I2S_rec_isr(void)
{
	int bPlayLastBlock = 0;
	
	ENTER();
	
	if (AUDIO_READ(REG_I2S_ACTL_RSR) & R_DMA_RIA_IRQ)
	{
		AUDIO_WRITE(REG_I2S_ACTL_RSR, R_DMA_RIA_IRQ);		

		if (!(AUDIO_READ(REG_I2S_ACTL_RSR) & R_DMA_RIA_SN))	// == 0
		{
			bPlayLastBlock = _tIIS.fnRecCallBack(_tIIS.uRecordBufferAddr, _tIIS.uRecordBufferLength/2);  //uRecordBufferAddr change to physical address
		}
		else 												// == BIT5
		{
			bPlayLastBlock = _tIIS.fnRecCallBack(_tIIS.uRecordBufferAddr + _tIIS.uRecordBufferLength/2,  //uRecordBufferAddr change to physical address
										_tIIS.uRecordBufferLength/2);
		}
	
		/* check whether the next block is token away. If not, stop record */
	
//		if (bPlayLastBlock)
//		{
//			AUDIO_WRITE(REG_I2S_ACTL_RSR, R_DMA_MIDDLE_IRQ | R_DMA_END_IRQ);
//		}
	}		
	
	AUDIO_WRITE(REG_I2S_ACTL_CON, AUDIO_READ(REG_I2S_ACTL_CON) | R_DMA_IRQ);	

	LEAVE();
}


INT i2sInit(VOID)
{
	int nStatus = 0;
	
	ENTER();
	
	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) &~PLAY_STEREO);
	AUDIO_WRITE(REG_I2S_ACTL_RESET, AUDIO_READ(REG_I2S_ACTL_RESET) &~RECORD_LEFT_CHNNEL &~RECORD_RIGHT_CHNNEL);
	
	nStatus = I2S_reset();
	if (nStatus < 0)
		return nStatus;
	
	LEAVE();
	
	return 0;	
}

NV_AUDIO_PLAY_CODEC_T nv_i2s_play_codec = {
	set_play_buffer:	i2sSetPlayBuffer,
	init_play:			i2sInit,	
	start_play:			i2sStartPlay,
	stop_play:			i2sStopPlay,
	set_play_volume:	i2sSetPlayVolume,
	play_interrupt:		I2S_play_isr,				
};

NV_AUDIO_RECORD_CODEC_T nv_i2s_record_codec = {
	set_record_buffer:	i2sSetRecordBuffer,
	init_record:		i2sInit,	
	start_record:		i2sStartRecord,
	stop_record:		i2sStopRecord,
	set_record_volume:	i2sSetRecordVolume,	
	record_interrupt:	I2S_rec_isr,			
};
