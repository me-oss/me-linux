/*
 * drivers/sound/oss/w55fa93_spu.c
 *
 * Copyright (c) 2009 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Changelog:
 *
 *   2008/12/16     ghguo add this file for nuvoton w55fa93 adc
 */
 
#include <linux/soundcard.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/w55fa93_audio.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/w55fa93_spu.h>

DECLARE_MUTEX(spu_sem);
EXPORT_SYMBOL(spu_sem);


//#define SPU_DEBUG
//#define SPU_DEBUG_ENTER_LEAVE
//#define SPU_DEBUG_MSG
//#define SPU_DEBUG_MSG2

#define AUDIO_WRITE(addr, val) 	outl(val, addr)
#define AUDIO_READ(addr)		inl(addr)  

//#define AUDIO_WRITE(port,value)     (*((u32 volatile *) (port))=value)
//#define AUDIO_READ(port)            (*((u32 volatile *) (port)))


/*
#define AUDIO_REC_ENABLE
#define LINE_IN_VOLUME_ENABLE
#define AUX_IN_VOLUME_ENABLE
#define CD_IN_VOLUME_ENABLE
*/

#ifdef SPU_DEBUG
#define DBG(fmt, arg...)			printk(fmt, ##arg)
#else
#define DBG(fmt, arg...)
#endif

#ifdef SPU_DEBUG_ENTER_LEAVE
#define ENTER()					DBG("[%-10s] : Enter\n", __FUNCTION__)
#define LEAVE()					DBG("[%-10s] : Leave\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef SPU_DEBUG_MSG
#define MSG(fmt)					DBG("[%-10s] : "fmt, __FUNCTION__)
#else
#define MSG(fmt)
#endif

#ifdef SPU_DEBUG_MSG2
#define MSG2(fmt, arg...)			DBG("[%-10s] : "fmt, __FUNCTION__, ##arg)
#else
#define MSG2(fmt, arg...)
#endif

//static unsigned int s_u32ExtClockMHz=27;
static unsigned int s_u32ExtClockMHz = 12;

static AUDIO_T	_tSpu;

static int _bApuVolumeActive = 0;
int _bSpuActive = 0;
int _u8Channel0 = 0, _u8Channel1 = 1;
	
static volatile int _bPlayDmaToggle, _bRecDmaToggle;


static void spuStopPlay(void);

static int DrvSPU_SetBaseAddress(u32 u32Channel, u32 u32Address);
static int DrvSPU_GetBaseAddress(u32 u32Channel);
static int DrvSPU_SetThresholdAddress(u32 u32Channel, u32 u32Address);
static int DrvSPU_GetThresholdAddress(u32 u32Channel);
static int DrvSPU_SetEndAddress(u32 u32Channel, u32 u32Address);
static int DrvSPU_GetEndAddress(u32 u32Channel);
//static int DrvSPU_GetCurrentAddress(u32 u32Channel);
extern int DrvSPU_GetCurrentAddress(u32 u32Channel);
static int DrvSPU_GetLoopStartAddress(u32 u32Channel, u32 u32Address);
static int DrvSPU_SetDFA(u32 u32Channel, u16 u16DFA);
static int DrvSPU_GetDFA(u32 u32Channel);
//static int DrvSPU_SetPAN(u32 u32Channel, u16 u16PAN);	// MSB 8-bit = left channel; LSB 8-bit = right channel
int DrvSPU_SetPAN(u32 u32Channel, u16 u16PAN);	// MSB 8-bit = left channel; LSB 8-bit = right channel
int DrvSPU_SetPauseAddress(u32 u32Channel, u32 u32Address);

static int DrvSPU_GetPAN(u32 u32Channel);
static int DrvSPU_SetSrcType(u32 u32Channel, u8  u8DataFormat);
static int DrvSPU_GetSrcType(u32 u32Channel);
static int DrvSPU_SetChannelVolume(u32 u32Channel, u8 u8Volume);

static int DrvSPU_GetChannelVolume(u32 u32Channel);
static void DrvSPU_EqOpen(E_DRVSPU_EQ_BAND eEqBand, E_DRVSPU_EQ_GAIN eEqGain);
static void DrvSPU_EqClose(void);
static void DrvSPU_SetVolume(u16 u16Volume);		// MSB: left channel; LSB right channel
static int DrvSPU_EnableInt(u32 u32Channel, u32 u32InterruptFlag);
static int DrvSPU_ChannelOpen(u32 u32Channel);
static int DrvSPU_ChannelClose(u32 u32Channel);
static int DrvSPU_DisableInt(u32 u32Channel, u32 u32InterruptFlag);

static int DrvSPU_ClearInt(u32 u32Channel, u32 u32InterruptFlag);


//control dac power by sysfs
extern int dac_auto_config;

// PLL clock settings
extern unsigned int w55fa93_apll_clock;
extern unsigned int w55fa93_upll_clock;
	
static void  spu_play_isr(void)
{
	int bPlayLastBlock;
	u8 ii;
	u32 u32Channel, u32InterruptFlag;	

	ENTER();
	
	
	u32Channel = 1;
	

//	for (ii=0; ii<32; ii++)
	for (ii=0; ii<2; ii++)
	{
		if (!(AUDIO_READ(REG_SPU_CH_EN) & u32Channel))
		{
			continue;
		}			

		if (AUDIO_READ(REG_SPU_CH_IRQ) & u32Channel)
		{
			while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);

			// load previous channel settings		
			AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (ii << 24));		
			AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
			while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);

			u32InterruptFlag = AUDIO_READ(REG_SPU_CH_EVENT);
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT));						

			/* clear int */
			if (u32InterruptFlag & DRVSPU_ENDADDRESS_INT)
			{			
				//MSG("th2\n");	
				AUDIO_WRITE(REG_SPU_CH_EVENT, (AUDIO_READ(REG_SPU_CH_EVENT) & ~0x3F00) | END_FG);						
			    bPlayLastBlock = _tSpu.fnPlayCallBack(_tSpu.uPlayBufferAddr+ _tSpu.uPlayBufferLength/ 2, 
											_tSpu.uPlayBufferLength/2);   
			}				

			/* clear int */	
			if (u32InterruptFlag & DRVSPU_THADDRESS_INT)
			{
				//MSG("th1\n");	
				AUDIO_WRITE(REG_SPU_CH_EVENT, (AUDIO_READ(REG_SPU_CH_EVENT) & ~0x3F00) | TH_FG);														
			    bPlayLastBlock = _tSpu.fnPlayCallBack(_tSpu.uPlayBufferAddr, 
											_tSpu.uPlayBufferLength/2);   
			}											
			AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) & ~DRVSPU_UPDATE_ALL_PARTIALS);		
			AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | (DRVSPU_UPDATE_IRQ_PARTIAL + DRVSPU_UPDATE_PARTIAL_SETTINGS));							
			while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		}
	
		u32Channel <<= 1; 
	}
	
	AUDIO_WRITE(REG_SPU_CH_IRQ, AUDIO_READ(REG_SPU_CH_IRQ));			

	/* here, we will check whether the next buffer is ready. If not, stop play. */
//	if (bPlayLastBlock)
		//;
		/* Fix me: what to do here ? 
		* AUDIO_WRITE(REG_ACTL_PSR, P_DMA_MIDDLE_IRQ | P_DMA_END_IRQ);*/

	LEAVE();
}
#if 1

unsigned int recBufCnt = 0;
static void spu_rec_isr(void)
{
  return;
}

static int spu_reset(void)
{
	return 0;
}

static unsigned int DrvSys_GetPLLOutputMHz(VOID)
{

	unsigned int u32Freqout;
	unsigned int u32OTDV;
	
#ifdef CONFIG_W55FA93_AUDIO_CLOCK_FROM_APLL	
	
	// SPU enigine clock from APLL
	u32OTDV = (((AUDIO_READ(REG_APLLCON)>>14)&0x3)+1)>>1;	
		
	u32Freqout = (s_u32ExtClockMHz*((AUDIO_READ(REG_APLLCON)&0x1FF)+2)/(((AUDIO_READ(REG_APLLCON)>>9)&0x1F)+2))>>u32OTDV;	
#else
	// SPU enigine clock from UPLL 
	u32OTDV = (((AUDIO_READ(REG_UPLLCON)>>14)&0x3)+1)>>1;	
		
	u32Freqout = (s_u32ExtClockMHz*((AUDIO_READ(REG_UPLLCON)&0x1FF)+2)/(((AUDIO_READ(REG_UPLLCON)>>9)&0x1F)+2))>>u32OTDV;	
#endif	
	
	return u32Freqout;
}

static VOID  spuSetPlaySampleRate(INT nSamplingRate)
{
//	unsigned int PllFreq = DrvSys_GetPLLOutputMHz();
	unsigned int PllFreq = w55fa93_upll_clock;
	
	u32 u32ClockDivider, u32RealSampleRate;	
	
	
	ENTER();
	
	/* enable audio enigne clock */
	AUDIO_WRITE(REG_AHBCLK, AUDIO_READ(REG_AHBCLK) | ADO_CKE);
	
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			
	AUDIO_WRITE(REG_AHBCLK, AUDIO_READ(REG_AHBCLK) | SPU_CKE);
#else
	AUDIO_WRITE(REG_AHBCLK, AUDIO_READ(REG_AHBCLK) | I2S_CKE);
#endif

#ifdef CONFIG_W55FA93_AUDIO_CLOCK_FROM_APLL	

	#ifdef CONFIG_I2S_MCLK_256WS
		if ( (nSamplingRate == AU_SAMPLE_RATE_96000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_48000) ||		
			 (nSamplingRate == AU_SAMPLE_RATE_24000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_16000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_12000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_20000) ||		 	// added
			 (nSamplingRate == AU_SAMPLE_RATE_8000) )
		{
			w55fa93_set_apll_clock(208896);
			
		}
		else if ( (nSamplingRate == AU_SAMPLE_RATE_64000) ||
			 	  (nSamplingRate == AU_SAMPLE_RATE_32000) )		
		{
			w55fa93_set_apll_clock(147456);
		}
		else if ( (nSamplingRate == AU_SAMPLE_RATE_44100) ||
			 	  (nSamplingRate == AU_SAMPLE_RATE_22050) ||
			 	  (nSamplingRate == AU_SAMPLE_RATE_11025) )		
		{
			w55fa93_set_apll_clock(169344);
		}
		else
			w55fa93_set_apll_clock(135500);			// 88.2KHz	
	
		PllFreq = w55fa93_apll_clock;
		PllFreq *= 1000;
			
		MSG2("==>PllFreq=0x%x\n", PllFreq);
		MSG2("==>nSamplingRate=0x%x\n", nSamplingRate);	
	
		u32ClockDivider = (PllFreq / (256*nSamplingRate));
		MSG2("==>u32ClockDivider=0x%x\n", u32ClockDivider);			
	
		AUDIO_WRITE(REG_CLKDIV1, (AUDIO_READ(REG_CLKDIV1) & (~ADO_S)) | (0x02 << 19) );	// SPU clock from APLL	
		AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) & (~ADO_N0));			
	#else
		if ( (nSamplingRate == AU_SAMPLE_RATE_48000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_32000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_24000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_16000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_12000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_20000) ||		 	// added
			 (nSamplingRate == AU_SAMPLE_RATE_8000) )
		{
			w55fa93_set_apll_clock(208896);
			
		}
		else if ( (nSamplingRate == AU_SAMPLE_RATE_64000) ||
			 (nSamplingRate == AU_SAMPLE_RATE_96000) )
		{
			w55fa93_set_apll_clock(147456);
		}
		else
			w55fa93_set_apll_clock(169344);		
	
		PllFreq = w55fa93_apll_clock;
		PllFreq *= 1000;
			
		MSG2("==>PllFreq=0x%x\n", PllFreq);
		MSG2("==>nSamplingRate=0x%x\n", nSamplingRate);	
	
		u32ClockDivider = (PllFreq / (128*nSamplingRate));
		MSG2("==>u32ClockDivider=0x%x\n", u32ClockDivider);			
	
		AUDIO_WRITE(REG_CLKDIV1, (AUDIO_READ(REG_CLKDIV1) & (~ADO_S)) | (0x02 << 19) );	// SPU clock from APLL	
		AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) & (~ADO_N0));			
	#endif	// CONFIG_I2S_MCLK_256WS
#else	
	
	/* set play sampling rate */
	//if (nSamplingRate != 48000)
	
//	PllFreq *= 1000000;
	PllFreq *= 1000;
		
	MSG2("==>PllFreq=0x%x\n", PllFreq);
	MSG2("==>nSamplingRate=0x%x\n", nSamplingRate);	

	u32ClockDivider = (PllFreq / (128*nSamplingRate));
	MSG2("==>u32ClockDivider=0x%x\n", u32ClockDivider);			
	
	AUDIO_WRITE(REG_CLKDIV1, (AUDIO_READ(REG_CLKDIV1) & (~ADO_S)) | (0x03 << 19) );	// SPU clock from UPLL				
	AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) & (~ADO_N0));		
	AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) | (0x01<<16));		
	u32ClockDivider /= 2;
#endif	
		
	AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) & (~ADO_N1));		
	u32ClockDivider &= 0xFF;
	u32ClockDivider	--;
	AUDIO_WRITE(REG_CLKDIV1, AUDIO_READ(REG_CLKDIV1) | (u32ClockDivider<<24));	
	
	MSG2("==>REG_CLKDIV1=0x%x\n", AUDIO_READ(REG_CLKDIV1));			
	
	_tSpu.nPlaySamplingRate = nSamplingRate;

	LEAVE();
}

static VOID spuSetPlayCallBackFunction(AU_CB_FUN_T fnCallBack)
{
	ENTER();

	_tSpu.fnPlayCallBack = fnCallBack;

	LEAVE();
}

static VOID spuSetRecordCallBackFunction(AU_CB_FUN_T fnCallBack)
{
	ENTER();
	

	LEAVE();
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*      spuSetPcmVolume	                                             	*/
/*                                                                       */
/* DESCRIPTION                                                     	      */
/*      Set pcm left and right channel play volume.                 	*/
/*                                                                       */
/* INPUTS                                                                */
/*      ucLeftVol    play volume of left channel                         */
/*      ucRightVol   play volume of left channel                         */
/*                  0:  mute                                             */
/*                  1:  minimal volume                                   */
/*                  31: maxmum volume                                    */
/*                                                                       */
/* OUTPUTS                                                               */
/*      None                                                             */
/*                                                                       */
/* RETURN                                                                */
/*      0           Success                                              */
/*		Otherwise	error												 */
/*                                                                       */
/*************************************************************************/
static int spuSetPcmVolume(UINT32 ucLeftVol, UINT32 ucRightVol)
{
	ENTER();
	MSG2("Set PCM volume to : %d-%d\n", ucLeftVol, ucRightVol);
	
	//save the flag that ap already sets the volume
	_bApuVolumeActive = 1;

#if defined(USE_DAC_ON_OFF_API)
	AUDIO_WRITE(REG_SPU_DAC_VOL, (AUDIO_READ(REG_SPU_DAC_VOL) & ~(/*ANA_PD | */LHPVL | RHPVL)) | (ucRightVol & 0x3F) | (ucLeftVol & 0x3F) << 8);
#else
	AUDIO_WRITE(REG_SPU_DAC_VOL, (AUDIO_READ(REG_SPU_DAC_VOL) & ~(ANA_PD | LHPVL | RHPVL)) | (ucRightVol & 0x3F) | (ucLeftVol & 0x3F) << 8);
#endif		
	LEAVE();
	
	return 0;
}

static int spuSetRecordVolume(int ucLeftVol, int ucRightVol)
{
    return(0);
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*      spuGetPcmVolume	                                             	 */
/*                                                                       */
/* DESCRIPTION                                                     	     */
/*      Get pcm Left channel play volume.                                */
/*                                                                       */
/* INPUTS                                                                */
/*      None                                                             */
/*                                                                       */
/* OUTPUTS                                                               */
/*      None 				                                             */
/*                                                                       */
/* RETURN                                                                */
/*      Left channel volume level (0 - 63)                               */
/*																		 */
/*************************************************************************/
int spuGetPcmVolume(void)
{
	ENTER();

	return ( (AUDIO_READ(REG_SPU_DAC_VOL) & LHPVL) >> 8);
	LEAVE();
	
	return 0;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*      spuStartPlay                                                   */
/*                                                                       */
/* DESCRIPTION                                                           */
/*      Start playback.                                             */
/*                                                                       */
/* INPUTS                                                                */
/*      fnCallBack     client program provided callback function. The audio */
/*                  driver will call back to get next block of PCM data  */
/*      nSamplingRate  the playback sampling rate. Supported sampling    */
/*                  rate are 48000, 44100, 32000, 24000, 22050, 16000,   */
/*                  11025, and 8000 Hz                                   */
/*      nChannels	number of playback nChannels                          */
/*					1: single channel, otherwise: double nChannels        */
/*                                                                       */
/* OUTPUTS                                                               */
/*      None                                                             */
/*                                                                       */
/* RETURN                                                                */
/*      0           Success                                              */
/*		Otherwise	error												 */
/*                                                                       */
/*************************************************************************/
static int spuStartPlay(AU_CB_FUN_T fnCallBack, INT nSamplingRate, INT nChannels)
{
	//INT	 nStatus;

	ENTER();

	if (_bSpuActive & SPU_PLAY_ACTIVE)
		return ERR_AU_GENERAL_ERROR;		/* AC97 was playing */
		

	/* disable by Qfu , for the installation of irq has been done in wb_audio.c
		the only thing to do is to enable irq by this routine */
	//Enable_Int(AU_PLAY_INT_NUM);
	/* fix me: how to deal with below?
	_DRVAIC_ENABLEINT(DRVAIC_SPU_INT);
	DrvAic_EnableGlobalIRQ();		
    DrvApu_EqOpen(DRVApu_EqBand_2, DRVApu_EqGain_P7DB);	
	*/
	if (nChannels ==1)
	{
		DrvSPU_SetBaseAddress(_u8Channel0, (u32)_tSpu.uPlayBufferAddr);		
		DrvSPU_SetThresholdAddress(_u8Channel0, (_tSpu.uPlayBufferAddr+_tSpu.uPlayBufferLength/2));		
		DrvSPU_SetEndAddress(_u8Channel0, (_tSpu.uPlayBufferAddr+_tSpu.uPlayBufferLength));		
		DrvSPU_SetSrcType(_u8Channel0, DRVSPU_MONO_PCM16);
//		DrvSPU_SetChannelVolume(_u8Channel0, 0x7F);	
		DrvSPU_SetChannelVolume(_u8Channel0, 0x4F);	
//		DrvSPU_SetVolume(0x0202);

#ifdef CONFIG_HEADSET_GPD14_AND_SPEAKER_GPA2
		if ( inl(REG_GPIOD_PIN) & 0x4000)	// headset plug_out
		{
//        	printk("channel_1 plug_out !!!!\n");	    			
//        	outl(inl(REG_GPIOA_DOUT)|(0x0004), REG_GPIOA_DOUT); 	// GPA2 = high        		                			
			DrvSPU_SetPAN(_u8Channel0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel	
		}		
		else								// headset plug_in		
		{
//        	printk("channel_1 plug_in !!!!\n");	    						
//		    outl(inl(REG_GPIOA_DOUT)&(~0x0004), REG_GPIOA_DOUT); 	// GPA2 = low        		                	                			
			DrvSPU_SetPAN(_u8Channel0, 0x1F1F);	// MSB 8-bit = left channel; LSB 8-bit = right channel	
		}			
#else
		DrvSPU_SetPAN(_u8Channel0, 0x1F1F);	// MSB 8-bit = left channel; LSB 8-bit = right channel	
#endif		

	#ifdef CONFIG_I2S_MCLK_256WS
		DrvSPU_SetDFA(_u8Channel0, 0x200);			
	#else
		DrvSPU_SetDFA(_u8Channel0, 0x400);			
	#endif		

		DrvSPU_SetBaseAddress(_u8Channel1, (u32)_tSpu.uPlayBufferAddr);
		DrvSPU_SetThresholdAddress(_u8Channel1, (_tSpu.uPlayBufferAddr+_tSpu.uPlayBufferLength/2));
		DrvSPU_SetEndAddress(_u8Channel1, (_tSpu.uPlayBufferAddr+_tSpu.uPlayBufferLength));
		DrvSPU_SetSrcType(_u8Channel1, DRVSPU_MONO_PCM16);
		DrvSPU_SetChannelVolume(_u8Channel1, 0x0);
		DrvSPU_SetPAN(_u8Channel1, 0x0000);
		
	#ifdef CONFIG_I2S_MCLK_256WS
		DrvSPU_SetDFA(_u8Channel1, 0x200);			
        	printk("set DFA = 0x200 !!!!\n");	    								
	#else
		DrvSPU_SetDFA(_u8Channel1, 0x400);			
	#endif		
		
	}
	else
	{	//left channel 
		DrvSPU_SetBaseAddress(_u8Channel0, (u32)_tSpu.uPlayBufferAddr);		
		DrvSPU_SetThresholdAddress(_u8Channel0, (_tSpu.uPlayBufferAddr+_tSpu.uPlayBufferLength/2));		
		DrvSPU_SetEndAddress(_u8Channel0, (_tSpu.uPlayBufferAddr+_tSpu.uPlayBufferLength));		
		DrvSPU_SetSrcType(_u8Channel0, DRVSPU_STEREO_PCM16_LEFT);		// left channel 
//		DrvSPU_SetChannelVolume(_u8Channel0, 0x7F);	
		DrvSPU_SetChannelVolume(_u8Channel0, 0x4F);	

//		DrvSPU_SetPAN(_u8Channel0, 0x1F1F);	// MSB 8-bit = left channel; LSB 8-bit = right channel	
		DrvSPU_SetPAN(_u8Channel0, 0x1F00);	// MSB 8-bit = left channel; LSB 8-bit = right channel			

	#ifdef CONFIG_I2S_MCLK_256WS
		DrvSPU_SetDFA(_u8Channel0, 0x200);			
        	printk("set DFA = 0x200 !!!!\n");	    										
	#else
		DrvSPU_SetDFA(_u8Channel0, 0x400);			
	#endif		
		

		// right channel
		DrvSPU_SetBaseAddress(_u8Channel1, (u32)_tSpu.uPlayBufferAddr);		
		DrvSPU_SetThresholdAddress(_u8Channel1, (_tSpu.uPlayBufferAddr+_tSpu.uPlayBufferLength/2));		
		DrvSPU_SetEndAddress(_u8Channel1, (_tSpu.uPlayBufferAddr+_tSpu.uPlayBufferLength));		
		DrvSPU_SetSrcType(_u8Channel1, DRVSPU_STEREO_PCM16_RIGHT);		// right channel	
//		DrvSPU_SetChannelVolume(_u8Channel1, 0x7F);
		DrvSPU_SetChannelVolume(_u8Channel1, 0x4F);

#ifdef CONFIG_HEADSET_GPD14_AND_SPEAKER_GPA2
		if ( inl(REG_GPIOD_PIN) & 0x4000)	// headset plug_out
		{
			DrvSPU_SetChannelVolume(_u8Channel0, 0x2F);	
			DrvSPU_SetChannelVolume(_u8Channel1, 0x2F);
			DrvSPU_SetPAN(_u8Channel1, 0x1F00);	// MSB 8-bit = left channel; LSB 8-bit = right channel	
		}		
		else								// headset plug_in		
		{
			DrvSPU_SetChannelVolume(_u8Channel0, 0x4F);	
			DrvSPU_SetChannelVolume(_u8Channel1, 0x4F);
			DrvSPU_SetPAN(_u8Channel1, 0x001F);	// MSB 8-bit = left channel; LSB 8-bit = right channel	
		}			
#else

//		DrvSPU_SetPAN(_u8Channel1, 0x1F1F);	// MSB 8-bit = left channel; LSB 8-bit = right channel	
		DrvSPU_SetPAN(_u8Channel1, 0x001F);	// MSB 8-bit = left channel; LSB 8-bit = right channel	
#endif		

	#ifdef CONFIG_I2S_MCLK_256WS
		DrvSPU_SetDFA(_u8Channel1, 0x200);			
        	printk("set DFA = 0x200 !!!!\n");	    										
	#else
		DrvSPU_SetDFA(_u8Channel1, 0x400);			
	#endif		
		
	}				
	

	/* set play sampling rate */
	spuSetPlaySampleRate(nSamplingRate);
	spuSetPlayCallBackFunction(fnCallBack);
				
	/* set default pcm volume */
	if(!_bApuVolumeActive)
		spuSetPcmVolume(15,15);
		
	/* start playing */
	MSG("SPU start playing...\n");
		
	_bPlayDmaToggle = 0;
	
	MSG2("==>REG_CLKDIV1=0x%x\n", AUDIO_READ(REG_CLKDIV1));
	MSG2("==>REG_CLKDIV2=0x%x\n", AUDIO_READ(REG_CLKDIV2));
	MSG2("==>REG_AHBCLK=0x%x\n", AUDIO_READ(REG_AHBCLK));
	MSG2("==>REG_SPU_CTRL=0x%x\n", AUDIO_READ(REG_SPU_CTRL));
	MSG2("==>REG_SPU_DAC_PAR=0x%x\n", AUDIO_READ(REG_SPU_DAC_PAR));
	MSG2("==>REG_SPU_DAC_VOL=0x%x\n", AUDIO_READ(REG_SPU_DAC_VOL));	
	MSG2("==>REG_SPU_CH_EVENT=0x%x\n", AUDIO_READ(REG_SPU_CH_EVENT));		
//	MSG2("REG_SPU_CH_IRQ = 0x%04x\r\n",AUDIO_READ(REG_SPU_CH_IRQ));
	

	MSG2("channel open, nChannels = 0x%x \n", nChannels);
	MSG2("channel open, _u8Channel0 = 0x%x \n", _u8Channel0);
	MSG2("channel open, _u8Channel1 = 0x%x \n", _u8Channel1);		
	if (nChannels ==1)
	{
		DrvSPU_ChannelOpen(_u8Channel0);		
		DrvSPU_ChannelOpen(_u8Channel1);
	}
	else
	{	
		DrvSPU_ChannelOpen(_u8Channel0);	//left channel 
		DrvSPU_ChannelOpen(_u8Channel1);	// right channel
	}				
	
	/*enable interrupt*/
	MSG2("*** enable AIC interrupt !! ***\N");			
	
	DrvSPU_ClearInt(_u8Channel0, DRVSPU_ALL_INT);
	DrvSPU_DisableInt(_u8Channel0, DRVSPU_ALL_INT);		
	DrvSPU_ClearInt(_u8Channel1, DRVSPU_ALL_INT);
	DrvSPU_DisableInt(_u8Channel1, DRVSPU_ALL_INT);		
	
	
	if (nChannels ==1)
	{
		DrvSPU_EnableInt(_u8Channel0, DRVSPU_ENDADDRESS_INT);
		DrvSPU_EnableInt(_u8Channel0, DRVSPU_THADDRESS_INT);		
	}
	else
	{	/* just open one channel interrupt */
		DrvSPU_EnableInt(_u8Channel0, DRVSPU_ENDADDRESS_INT);
		DrvSPU_EnableInt(_u8Channel0, DRVSPU_THADDRESS_INT);		
		
//		DrvSPU_EnableInt(_u8Channel1, DRVSPU_ENDADDRESS_INT);
		//DrvSPU_EnableInt(_u8Channel1, DRVSPU_THADDRESS_INT);		
	}				

	AUDIO_WRITE(REG_SPU_CH_IRQ,AUDIO_READ(REG_SPU_CH_IRQ));		
	
	/*fixe me: assume spuSetPlayBuffer(); has been called before*/

	#ifdef USE_DAC_ON_OFF_API
		if(dac_auto_config)
		{		
			outl(inl(REG_SPU_DAC_VOL) & ~0x0800000, REG_SPU_DAC_VOL);	//P7	
			msleep(1);
			outl(inl(REG_SPU_DAC_VOL) & ~0x0400000, REG_SPU_DAC_VOL);	//P6
			msleep(1);
			outl(inl(REG_SPU_DAC_VOL) & ~0x01e0000, REG_SPU_DAC_VOL);	//P1-4
			msleep(1);
			outl(inl(REG_SPU_DAC_VOL) & ~0x0200000, REG_SPU_DAC_VOL);	//P5	
			msleep(1);
			
			outl(inl(REG_SPU_DAC_VOL) & ~0x00010000, REG_SPU_DAC_VOL);	//P0			
			printk("dac auto config=%x\n",inl(REG_SPU_DAC_VOL)); 
			msleep(700);		
		}		
	#endif
	
	
	AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) | SPU_EN);

	_bSpuActive |= SPU_PLAY_ACTIVE;
	

	LEAVE();
	
//	while(1);
	return 0;
}

/*************************************************************************/
/*                                                                       */
/* FUNCTION                                                              */
/*      spuStopPlay                                                     */
/*                                                                       */
/* DESCRIPTION                                                           */
/*      Stop playback immdediately.                                 */
/*                                                                       */
/* INPUTS                                                                */
/*      None    	                                                     */
/*                                                                       */
/* OUTPUTS                                                               */
/*      None                                                             */
/*                                                                       */
/* RETURN                                                                */
/*      0           Success                                              */
/*		Otherwise	error												 */
/*                                                                       */
/*************************************************************************/
static void spuStopPlay(void)
{
	int volume;
	
	ENTER();
	if (!(_bSpuActive & SPU_PLAY_ACTIVE))
		return;
			
	if(_bApuVolumeActive)	//save the volume before reset audio engine
		volume = AUDIO_READ(REG_SPU_DAC_VOL) & (LHPVL | RHPVL);		
		
	/* stop playing */

//		AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) & ~SPU_EN);	/*disable spu*/
	
	#ifdef USE_DAC_ON_OFF_API
		if(dac_auto_config)
		{
			mdelay(1);
			
			outl(inl(REG_SPU_DAC_VOL) | 0x10000, REG_SPU_DAC_VOL);	//P0
			mdelay(700);
				
			outl(inl(REG_SPU_DAC_VOL) | 0x200000, REG_SPU_DAC_VOL);	//P5
			mdelay(1);
						
			outl(inl(REG_SPU_DAC_VOL) | 0x1e0000, REG_SPU_DAC_VOL);	//P1-4
			mdelay(1);
			outl(inl(REG_SPU_DAC_VOL) | 0x400000, REG_SPU_DAC_VOL);	//P6
			mdelay(1);
			outl(inl(REG_SPU_DAC_VOL) | 0x800000, REG_SPU_DAC_VOL);	//P7
			MSG2("%x\n",inl(REG_SPU_DAC_VOL));
			mdelay(1);
		}
	#endif
	
	//restore volume
	AUDIO_WRITE(REG_SPU_DAC_VOL, (AUDIO_READ(REG_SPU_DAC_VOL) | volume));

	/* channel close (before SPU disabled) */
	DrvSPU_ChannelClose(_u8Channel0);	//left channel 
	DrvSPU_ChannelClose(_u8Channel1);	// right channel
			
	/* disable audio play interrupt */
	if (!_bSpuActive)
 	{		
		AUDIO_WRITE(REG_AIC_MDCR,(1<<IRQ_SPU));
		
		/*disable interrupt*/

//		DrvSPU_ClearInt(_u8Channel0, DRVSPU_ENDADDRESS_INT);
//		DrvSPU_ClearInt(_u8Channel0, DRVSPU_THADDRESS_INT);		
//		DrvSPU_DisableInt(_u8Channel0, DRVSPU_ENDADDRESS_INT);
//		DrvSPU_DisableInt(_u8Channel0, DRVSPU_THADDRESS_INT);		
		
		AUDIO_WRITE(REG_AIC_MECR,(1<<IRQ_SPU));		
	}
	
	/* disable SPU (after channel being closed) */
	AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) & ~SPU_EN);	/*disable spu*/	
	
	/* reset SPU engine */
	AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) & ~SPU_SWRST);	
	AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) | SPU_SWRST);
	AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) & ~SPU_SWRST);	

	
	_bSpuActive &= ~SPU_PLAY_ACTIVE;       
	
	LEAVE();
}


static int spuStartRecord(AU_CB_FUN_T fnCallBack, INT nSamplingRate, INT nChannels)
{
	return 0;
}

static void spuStopRecord(void)
{
    return;
}

static void spuSetPlayBuffer(UINT32 uDMABufferAddr, UINT32 uDMABufferLength)
{
	ENTER();

	_tSpu.uPlayBufferAddr = uDMABufferAddr;
	_tSpu.uPlayBufferLength = uDMABufferLength;
	
	MSG2("uDMABufferAddr=%d, uDMABufferLength=%d\n", uDMABufferAddr, uDMABufferLength);
	//AUDIO_WRITE(W55FA93_VA_VRAM_BASE+0x0, 0x0);	//32k internal RAM
	
	//_tSpu.uPlayBufferAddr = 0xffe76000;
	//_tSpu.uPlayBufferLength = 1024*32;

	LEAVE();
}

static void spuSetRecordBuffer(UINT32 uDMABufferAddr, UINT32 uDMABufferLength)
{
    return;
}    

static INT spuInit(VOID)
{
	int nStatus = 0;
	u8 ii;

	ENTER();

		MSG2("init SPU register BEGIN !!\n");
			
		// enable SPU engine clock 
		AUDIO_WRITE(REG_AHBCLK, AUDIO_READ(REG_AHBCLK) | ADO_CKE | SPU_CKE | HCLK4_CKE);	// enable SPU engine clock 
	
		// SPU IP reset
//		AUDIO_WRITE(REG_AHBIPRST, AUDIO_READ(REG_AHBIPRST) | SPURST);						// SPU IP reset
//		AUDIO_WRITE(REG_AHBIPRST, AUDIO_READ(REG_AHBIPRST) & ~SPURST);						// SPU IP reset		
		// disable SPU engine 
		AUDIO_WRITE(REG_SPU_CTRL, 0x00);
		
		// given FIFO size = 4
		AUDIO_WRITE(REG_SPU_CTRL, 0x04000000);		
	
		// reset SPU engine 
	//	AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) | SPU_EN);
		AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) & ~SPU_SWRST);	
		
		AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) | SPU_SWRST);
		AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) & ~SPU_SWRST);	
		
		// disable all channels
		AUDIO_WRITE(REG_SPU_CH_EN, 0x00);		

		for (ii=0; ii<32; ii++)
		{
			DrvSPU_ClearInt(ii, DRVSPU_ALL_INT);
			DrvSPU_DisableInt(ii, DRVSPU_ALL_INT);
		}
		
//	nStatus = spu_reset();
//	if (nStatus < 0)
//		return nStatus;

	LEAVE();
	
	return 0;	
}

static INT spuGetCapacity(VOID)
{
	return DSP_CAP_DUPLEX;		/* support full duplex */
}


NV_AUDIO_PLAY_CODEC_T nv_spu_play_codec = {
	set_play_buffer:	spuSetPlayBuffer,
	init_play:			spuInit,	
	start_play:			spuStartPlay,
	stop_play:			spuStopPlay,
	set_play_volume:	spuSetPcmVolume,
	play_interrupt:		spu_play_isr,				
};


static int DrvSPU_SetBaseAddress(
	u32 u32Channel, 
	u32 u32Address
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		AUDIO_WRITE(REG_SPU_S_ADDR, u32Address);		

		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_UPDATE_ALL_SETTINGS);				
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
		return E_SUCCESS;
	}
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}

int DrvSPU_GetCurrentAddress(
//static int DrvSPU_GetCurrentAddress(
	u32 u32Channel
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		return AUDIO_READ(REG_SPU_CUR_ADDR);
	}
	else return 0;	   
}

static int DrvSPU_GetBaseAddress(
	u32 u32Channel
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		return (AUDIO_READ(REG_SPU_S_ADDR));
	}
	else
		return 0;
}

static int DrvSPU_SetThresholdAddress(
	u32 u32Channel, 
	u32 u32Address
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		AUDIO_WRITE(REG_SPU_M_ADDR, u32Address);		

		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_UPDATE_ALL_SETTINGS);				
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
		return E_SUCCESS;
	}
	 
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}
static int DrvSPU_GetThresholdAddress(
	u32 u32Channel
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		return AUDIO_READ(REG_SPU_M_ADDR);
	}
	else
		return 0;
}

static int DrvSPU_SetEndAddress(
	u32 u32Channel, 
	u32 u32Address
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		AUDIO_WRITE(REG_SPU_E_ADDR, u32Address);		

		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_UPDATE_ALL_SETTINGS);				
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
		return E_SUCCESS;
	}
	 
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}

static int DrvSPU_GetEndAddress(
	u32 u32Channel
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		return AUDIO_READ(REG_SPU_E_ADDR);
	}
	else
		return 0;
}



int DrvSPU_SetPauseAddress(
	u32 u32Channel, 
	u32 u32Address
)
{
	
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
//		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
//		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		AUDIO_WRITE(REG_SPU_PA_ADDR, u32Address);		

		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~0xFF) | DRVSPU_UPDATE_PAUSE_PARTIAL);				
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
#if 1
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | ((u32Channel+1) << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~0xFF) | DRVSPU_UPDATE_PAUSE_PARTIAL);				
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
#endif 		
		
		return E_SUCCESS;
	}
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}


int DrvSPU_GetPauseAddress(
	u32 u32Channel
)
{
	
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		return AUDIO_READ(REG_SPU_PA_ADDR);		
	}
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}

static int DrvSPU_GetLoopStartAddress(
	u32 u32Channel, 
	u32 u32Address
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		return AUDIO_READ(REG_SPU_LP_ADDR);
	}
	else return 0;	   
}

static int 
DrvSPU_SetDFA(
	u32 u32Channel, 
	u16 u16DFA
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		AUDIO_WRITE(REG_SPU_CH_PAR_2, u16DFA);		

		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) & ~DRVSPU_UPDATE_ALL_PARTIALS);		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | (DRVSPU_UPDATE_DFA_PARTIAL + DRVSPU_UPDATE_PARTIAL_SETTINGS));				

		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
		return E_SUCCESS;
	}
	 
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}


static int DrvSPU_GetDFA(
	u32 u32Channel
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		return AUDIO_READ(REG_SPU_CH_PAR_2) & 0x1FFF;
	}
	else
		return 0;
}

// MSB 8-bit = left channel; LSB 8-bit = right channel
//static int DrvSPU_SetPAN(
int DrvSPU_SetPAN(
	u32 u32Channel, 
	u16 u16PAN
)
{
	u32 u32PAN;
	
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		u32PAN = u16PAN;
		u32PAN <<= 8;			
		u32PAN &= (PAN_L + PAN_R);
		AUDIO_WRITE(REG_SPU_CH_PAR_1, (AUDIO_READ(REG_SPU_CH_PAR_1) & (~(PAN_L+PAN_R))) | u32PAN);		

		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) & ~DRVSPU_UPDATE_ALL_PARTIALS);		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | (DRVSPU_UPDATE_PAN_PARTIAL + DRVSPU_UPDATE_PARTIAL_SETTINGS));				

		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
		return E_SUCCESS;
	}
	 
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}

static int DrvSPU_GetPAN(
	u32 u32Channel
)
{
	u32 u32PAN;
	
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		u32PAN = AUDIO_READ(REG_SPU_CH_PAR_1);
		u32PAN >>= 8;
		return (u32PAN & 0xFFFF);
	}
	else
		return 0;
}


static int DrvSPU_SetSrcType(
	u32 u32Channel, 
	u8  u8DataFormat
)
{

	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		AUDIO_WRITE(REG_SPU_CH_PAR_1, (AUDIO_READ(REG_SPU_CH_PAR_1) & ~SRC_TYPE) | u8DataFormat);		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_UPDATE_ALL_SETTINGS );				

		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
		return E_SUCCESS;
	}
	 
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}

static int DrvSPU_GetSrcType(
	u32 u32Channel
)
{
	u8 u8DataFormat;
	
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		u8DataFormat = AUDIO_READ(REG_SPU_CH_PAR_1);
		return (u8DataFormat & 0x07);
	}
	else
		return 0;
}

static int DrvSPU_SetChannelVolume(
	u32 u32Channel, 
	u8 	u8Volume
)
{
	u32 u32PAN;
	
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		u32PAN = u8Volume;
		u32PAN <<= 24;
		AUDIO_WRITE(REG_SPU_CH_PAR_1, (AUDIO_READ(REG_SPU_CH_PAR_1) & 0x00FFFFFF) | u32PAN);		

		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) & ~DRVSPU_UPDATE_ALL_PARTIALS);		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | (DRVSPU_UPDATE_VOL_PARTIAL + DRVSPU_UPDATE_PARTIAL_SETTINGS));				

		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
		return E_SUCCESS;
	}
	 
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}

static int DrvSPU_GetChannelVolume(
	u32 u32Channel
)
{
	u32 u32PAN;
	
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		u32PAN = AUDIO_READ(REG_SPU_CH_PAR_1);
		u32PAN >>= 24;
		return (u32PAN & 0xFF);
	}
	else
		return 0;
}

static void DrvSPU_EqOpen(
	E_DRVSPU_EQ_BAND eEqBand,
	E_DRVSPU_EQ_GAIN eEqGain		
)
{
	switch (eEqBand)
	{
		case eDRVSPU_EQBAND_DC:
			AUDIO_WRITE(REG_SPU_EQGain1, (AUDIO_READ(REG_SPU_EQGain1) & (~Gaindc)) | eEqGain <<16);
			break;
	
		case eDRVSPU_EQBAND_1:
			AUDIO_WRITE(REG_SPU_EQGain0, (AUDIO_READ(REG_SPU_EQGain0) & (~Gain01)) | eEqGain);
			break;
	
		case eDRVSPU_EQBAND_2:
			AUDIO_WRITE(REG_SPU_EQGain0, (AUDIO_READ(REG_SPU_EQGain0) & (~Gain02)) | eEqGain <<4);
			break;

		case eDRVSPU_EQBAND_3:
			AUDIO_WRITE(REG_SPU_EQGain0, (AUDIO_READ(REG_SPU_EQGain0) & (~Gain03)) | eEqGain <<8);
			break;

		case eDRVSPU_EQBAND_4:
			AUDIO_WRITE(REG_SPU_EQGain0, (AUDIO_READ(REG_SPU_EQGain0) & (~Gain04)) | eEqGain <<12);
			break;

		case eDRVSPU_EQBAND_5:
			AUDIO_WRITE(REG_SPU_EQGain0, (AUDIO_READ(REG_SPU_EQGain0) & (~Gain05)) | eEqGain <<16);
			break;

		case eDRVSPU_EQBAND_6:
			AUDIO_WRITE(REG_SPU_EQGain0, (AUDIO_READ(REG_SPU_EQGain0) & (~Gain06)) | eEqGain <<20);
			break;

		case eDRVSPU_EQBAND_7:
			AUDIO_WRITE(REG_SPU_EQGain0, (AUDIO_READ(REG_SPU_EQGain0) & (~Gain07)) | eEqGain <<24);
			break;

		case eDRVSPU_EQBAND_8:
			AUDIO_WRITE(REG_SPU_EQGain0, (AUDIO_READ(REG_SPU_EQGain0) & (~Gain08)) | eEqGain <<28);
			break;

		case eDRVSPU_EQBAND_9:
			AUDIO_WRITE(REG_SPU_EQGain1, (AUDIO_READ(REG_SPU_EQGain1) & (~Gain09)) | eEqGain);
			break;

		default:
		case eDRVSPU_EQBAND_10:
			AUDIO_WRITE(REG_SPU_EQGain1, (AUDIO_READ(REG_SPU_EQGain1) & (~Gain10)) | eEqGain <<4);
			break;
	}
	
	AUDIO_WRITE(REG_SPU_DAC_PAR, AUDIO_READ(REG_SPU_DAC_PAR) | EQU_EN | ZERO_EN);
}

static void DrvSPU_EqClose(void)
{
	AUDIO_WRITE(REG_SPU_DAC_PAR, AUDIO_READ(REG_SPU_DAC_PAR) & (~EQU_EN) & (~ZERO_EN));
}

static void DrvSPU_SetVolume(
	u16 u16Volume	// MSB: left channel; LSB right channel
)	
{
	AUDIO_WRITE(REG_SPU_DAC_VOL, (AUDIO_READ(REG_SPU_DAC_VOL) & ~(DWA_SEL | ANA_PD | LHPVL | RHPVL)) | (u16Volume & 0x3F3F));
}

static int DrvSPU_EnableInt(
	u32 u32Channel, 
	u32 u32InterruptFlag 
)
{
	
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);

	MSG2("*** DrvSPU_EnableInt *** \n");
	MSG2("*** download channel data *** \n");	
	MSG2("==>REG_SPU_CH_CTRL=0x%x\n", AUDIO_READ(REG_SPU_CH_CTRL));
	MSG2("==>REG_SPU_CH_EVENT=0x%x\n", AUDIO_READ(REG_SPU_CH_EVENT));		
		
		// set new channel settings for previous channel settings						
		if (u32InterruptFlag & DRVSPU_USER_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) | EV_USR_EN);		
		}
		if (u32InterruptFlag & DRVSPU_SILENT_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) | EV_SLN_EN);				
		}
		if (u32InterruptFlag & DRVSPU_LOOPSTART_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) | EV_LP_EN);						
		}
		if (u32InterruptFlag & DRVSPU_END_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) | EV_END_EN);						
		}

		if (u32InterruptFlag & DRVSPU_ENDADDRESS_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) | END_EN);						
		}

		if (u32InterruptFlag & DRVSPU_THADDRESS_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) | TH_EN);														
		}
		AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) & ~AT_CLR_EN);
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) & ~DRVSPU_UPDATE_ALL_PARTIALS);		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | (DRVSPU_UPDATE_IRQ_PARTIAL + DRVSPU_UPDATE_PARTIAL_SETTINGS));				
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
	MSG2("*** upload channel data *** \n");	
	MSG2("==>REG_SPU_CH_CTRL=0x%x\n", AUDIO_READ(REG_SPU_CH_CTRL));
	MSG2("==>REG_SPU_CH_EVENT=0x%x\n", AUDIO_READ(REG_SPU_CH_EVENT));		
		
		
		return E_SUCCESS;
	}
	 
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}

static int DrvSPU_DisableInt(
	u32 u32Channel, 
	u32 u32InterruptFlag
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		MSG2("wait to finish previous channel settings  11\n"); 						
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		MSG2("load previous channel settings  11\n"); 						
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// set new channel settings for previous channel settings						
		MSG2("set new channel settings for previous channel settings 11\n");		
		if (u32InterruptFlag & DRVSPU_USER_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) & ~EV_USR_EN);		
		}
		if (u32InterruptFlag & DRVSPU_SILENT_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) & ~EV_SLN_EN);				
		}
		if (u32InterruptFlag & DRVSPU_LOOPSTART_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) & ~EV_LP_EN);						
		}
		if (u32InterruptFlag & DRVSPU_END_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) & ~EV_END_EN);						
		}
		if (u32InterruptFlag & DRVSPU_ENDADDRESS_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) & ~END_EN);						
		}
		if (u32InterruptFlag & DRVSPU_THADDRESS_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, AUDIO_READ(REG_SPU_CH_EVENT) & ~TH_EN);
		}
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) & ~DRVSPU_UPDATE_ALL_PARTIALS);		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | (DRVSPU_UPDATE_IRQ_PARTIAL + DRVSPU_UPDATE_PARTIAL_SETTINGS));				
		
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
	
		return E_SUCCESS;
	}
	 
	else
		return E_DRVSPU_WRONG_CHANNEL;	   
}


static int DrvSPU_ClearInt(
	u32 u32Channel, 
	u32 u32InterruptFlag
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		// wait to finish previous channel settings
		MSG2("wait to finish previous channel settings\n"); 				
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);
		
		// load previous channel settings		
		MSG2("load previous channel settings\n"); 				
		AUDIO_WRITE(REG_SPU_CH_CTRL, (AUDIO_READ(REG_SPU_CH_CTRL) & ~CH_NO) | (u32Channel << 24));		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | DRVSPU_LOAD_SELECTED_CHANNEL);
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);

		MSG2("*** DrvSPU_ClearInt *** \n");
		MSG2("*** download channel data *** \n");	
		MSG2("==>REG_SPU_CH_CTRL=0x%x\n", AUDIO_READ(REG_SPU_CH_CTRL));
		MSG2("==>REG_SPU_CH_EVENT=0x%x\n", AUDIO_READ(REG_SPU_CH_EVENT));		
		
		// set new channel settings for previous channel settings
		MSG2("set new channel settings for previous channel settings\n");
		if (u32InterruptFlag & DRVSPU_USER_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, (AUDIO_READ(REG_SPU_CH_EVENT) & ~0x3F00) | EV_USR_FG);		
		}
		if (u32InterruptFlag & DRVSPU_SILENT_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, (AUDIO_READ(REG_SPU_CH_EVENT) & ~0x3F00) | EV_SLN_FG);				
		}
		if (u32InterruptFlag & DRVSPU_LOOPSTART_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, (AUDIO_READ(REG_SPU_CH_EVENT) & ~0x3F00) | EV_LP_FG);						
		}
		if (u32InterruptFlag & DRVSPU_END_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, (AUDIO_READ(REG_SPU_CH_EVENT) & ~0x3F00) | EV_END_FG);						
		}
		if (u32InterruptFlag & DRVSPU_ENDADDRESS_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, (AUDIO_READ(REG_SPU_CH_EVENT) & ~0x3F00) | END_FG);						
		}
		if (u32InterruptFlag & DRVSPU_THADDRESS_INT)
		{
			AUDIO_WRITE(REG_SPU_CH_EVENT, (AUDIO_READ(REG_SPU_CH_EVENT) & ~0x3F00) | TH_FG);														
		}
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) & ~DRVSPU_UPDATE_ALL_PARTIALS);		
		AUDIO_WRITE(REG_SPU_CH_CTRL, AUDIO_READ(REG_SPU_CH_CTRL) | (DRVSPU_UPDATE_IRQ_PARTIAL + DRVSPU_UPDATE_PARTIAL_SETTINGS));				
		
		MSG2("wait to finish previous channel settings 00\n"); 						
		while(AUDIO_READ(REG_SPU_CH_CTRL) & CH_FN);		
		
		MSG2("wait to finish previous channel settings OK\n"); 								
	
		MSG2("*** upload channel data *** \n");	
		MSG2("==>REG_SPU_CH_CTRL=0x%x\n", AUDIO_READ(REG_SPU_CH_CTRL));
		MSG2("==>REG_SPU_CH_EVENT=0x%x\n", AUDIO_READ(REG_SPU_CH_EVENT));		
	
		return E_SUCCESS;
	}
	else
	{
		MSG2("WORNG CHANNEL\n"); 									
		return E_DRVSPU_WRONG_CHANNEL;	   
	}		
		
}

static int DrvSPU_ChannelOpen(
	u32 u32Channel
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		AUDIO_WRITE(REG_SPU_CH_EN, AUDIO_READ(REG_SPU_CH_EN) | (0x0001 << u32Channel));
		return E_SUCCESS;
	}
	else		
		return E_DRVSPU_WRONG_CHANNEL;	   	
}

static int DrvSPU_ChannelClose(
	u32 u32Channel
)
{
	if ( (u32Channel >=eDRVSPU_CHANNEL_0) && (u32Channel <=eDRVSPU_CHANNEL_31) )
	{
		AUDIO_WRITE(REG_SPU_CH_EN, AUDIO_READ(REG_SPU_CH_EN) & ~(0x0001 << u32Channel));
		return E_SUCCESS;
	}		
	else		
		return E_DRVSPU_WRONG_CHANNEL;	   	
}

#endif