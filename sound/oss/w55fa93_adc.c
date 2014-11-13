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
#include <asm/arch/w55fa93_adc.h>

//#define ADC_DEBUG
//#define ADC_DEBUG_ENTER_LEAVE
//#define ADC_DEBUG_MSG
//#define ADC_DEBUG_MSG2

#ifdef ADC_DEBUG
#define DBG(fmt, arg...)			printk(fmt, ##arg)
#else
#define DBG(fmt, arg...)
#endif

#ifdef ADC_DEBUG_ENTER_LEAVE
#define ENTER()					DBG("[%-10s] : Enter\n", __FUNCTION__)
#define LEAVE()					DBG("[%-10s] : Leave\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef ADC_DEBUG_MSG
#define MSG(fmt)					DBG("[%-10s] : "fmt, __FUNCTION__)
#else
#define MSG(fmt)
#endif

#ifdef ADC_DEBUG_MSG2
#define MSG2(fmt, arg...)			DBG("[%-10s] : "fmt, __FUNCTION__, ##arg)
#else
#define MSG2(fmt, arg...)
#endif

static AUDIO_T	_tADC;

#define Delay(time)					udelay(time * 10)
UINT32 g_u32Period = 4;

static int	 _bIISActive = 0;

#define outp32(addr, val) 	outl(val, addr)
#define inp32(addr)		inl(addr) 


void w55fa93_adc_recording_setup(void)
{
	UINT32 u32Reg;
	UINT32 eMode = eDRVADC_RECORD_MODE_1;
	ENTER();

#if 1
	DrvADC_Open(eDRVADC_RECORD,			//Record mode
					eDRVSYS_APLL, 			//Source clock come from UPLL
					8);						//Deafult 8K sample rate. 
	DrvADC_SetGainControl(eDRVADC_PRE_P14, eDRVADC_POST_P34P5);
//	DrvADC_SetGainControl(eDRVADC_PRE_P14, eDRVADC_POST_P34P5);
							//eDRVADC_POST_P0);  

	DrvADC_SetAutoGainTiming(4,		//Period
							4,		//Attack
							4,		//Recovery	
							4);		//Hold

	DrvADC_SetAutoGainControl(TRUE,
		    					//11, 		//Output target -12db
							//15, 			//Output target -6db
							//13, 			//Output target -9db
							12, 			//Output target -10.5db
		    					 eDRVADC_BAND_P0P5,
		    					 eDRVADC_BAND_N0P5);

	DrvADC_SetOffsetCancellation(FALSE,   	//BOOL bIsMuteEnable,
								FALSE, 	//BOOL bIsOffsetCalibration,
								FALSE, 	//BOOL bIsHardwareMode,
								0x10);	//UINT32 u32Offset
	DrvADC_SetOffsetCancellationEx(1,		//255 sample
								512);	//Delay sample count
    	DrvADC_SetNoiseGate(FALSE, eDRVADC_NG_N48);

#else
	DrvADC_Open(eDRVADC_RECORD,			//Record mode
					eDRVSYS_APLL, 			//Source clock come from UPLL
					8);						//Deafult 8K sample rate. 
	DrvADC_SetGainControl(eDRVADC_PRE_P14, 
							eDRVADC_POST_P0);
	DrvADC_SetClampingAGC(eDRVADC_MAX_P17P25,
							eDRVADC_MIN_N12);							

	DrvADC_SetAutoGainTiming(4,		//Period
							4,		//Attack
							4,		//Recovery	sync
							4);		//Hold

	DrvADC_SetOffsetCancellation(FALSE,   	//BOOL bIsMuteEnable,
								FALSE, 	//BOOL bIsOffsetCalibration,
								FALSE,	//BOOL bIsHardwareMode,
								0x1A);	//UINT32 u32Offset

	DrvADC_SetOffsetCancellationEx(1,		//255 sample
								256);	//Delay sample count
    	DrvADC_SetNoiseGate(FALSE, eDRVADC_NG_N36);

	DrvADC_SetAutoGainControl(TRUE,
		    					12,	//Output target -10.5db		//11, 	//Output target -12db
		    					 eDRVADC_BAND_P0P5,
		    					 eDRVADC_BAND_N0P5);

#endif
	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) & 
			~AUDIO_INT_MODE & ~AUDIO_INT_EN);		// one sample if finish    
	outp32(REG_AGCP1,inp32(REG_AGCP1) | 0x80000000);	// Enabe EDMA for ADC
}
void w55fa93_adc_close(void)
{
	ENTER();
	DrvADC_Close();
}




/*----- set sample Frequency -----*/
VOID  adcSetRecordSampleRate(INT nSamplingRate)
{
	ENTER();
	_tADC.nRecSamplingRate = nSamplingRate;
}

VOID adcSetRecordCallBackFunction(AU_CB_FUN_T fnCallBack)
{
	ENTER();
	_tADC.fnRecCallBack = fnCallBack;
}


static int adcStartRecord(AU_CB_FUN_T fnCallBack, INT nSamplingRate, 
							INT nChannels)
{
	INT		nStatus;

	ENTER();
	g_u32Period = 4; 
	adcSetRecordCallBackFunction(fnCallBack);
	DrvADC_EnableInt(eDRVADC_AUD_INT);
	DBG("Sample Rate = %d", nSamplingRate);
	DrvADC_AudioRecordSampleRate(eDRVSYS_APLL, nSamplingRate/1000);	/* KHz unit */			
	DrvADC_StartRecord(eDRVADC_RECORD_MODE_1);

	LEAVE();
	
	return 0;
}

extern int w55fa93ts_open_again(void);
extern int w55fa93ts_close_again(void);
static void adcStopRecord(void)
{
	ENTER();
	
	UINT32 u32Idx;
	for(	u32Idx=0; u32Idx<0x60; u32Idx=u32Idx+0x4)
	{
		DBG("0x%x = 0x%x\n", u32Idx, inp32(W55FA93_VA_ADC+u32Idx));
		u32Idx = u32Idx+0x04;
		DBG("0x%x = 0x%x\n", u32Idx, inp32(W55FA93_VA_ADC+u32Idx));
		u32Idx = u32Idx+0x04;
		DBG("0x%x = 0x%x\n", u32Idx, inp32(W55FA93_VA_ADC+u32Idx));
		u32Idx = u32Idx+0x04;
		DBG("0x%x = 0x%x\n", u32Idx, inp32(W55FA93_VA_ADC+u32Idx));	
	}
	//2011-0506 w55fa93_adc_close();
	//2011-0506 w55fa93ts_open_again();
	LEAVE();
	
	return;
}


static int adcSetRecordVolume(UINT32 ucLeftVol, UINT32 ucRightVol)
{

	ENTER();

	LEAVE();

	return 0;

}

static void adcSetRecordBuffer(UINT32 uDMABufferAddr, UINT32 uDMABufferLength)
{
	ENTER();

	_tADC.uRecordBufferAddr = uDMABufferAddr; 
	_tADC.uRecordBufferLength = uDMABufferLength;
	
		

	LEAVE();
}
static int edma_adc_int_type=0;	
void adc_edma_isr_type(int IntType)
{
	edma_adc_int_type = IntType;	
}
static void  adc_rec_isr(void)
{
	int bPlayLastBlock = 0;
	
	ENTER();
	
	if(edma_adc_int_type==0x400)	//EDMA HALF interrupt
		bPlayLastBlock = _tADC.fnRecCallBack(_tADC.uRecordBufferAddr, 
										_tADC.uRecordBufferLength/2); 
	else 					//EDMA EMPTY interrupt
		bPlayLastBlock = _tADC.fnRecCallBack(_tADC.uRecordBufferAddr + _tADC.uRecordBufferLength/2, 
										_tADC.uRecordBufferLength/2);	


#if 1
	if(g_u32Period<384)
	{
		g_u32Period = g_u32Period + 16;
		DrvADC_SetAutoGainTiming(g_u32Period ,		//Period
							4,		//Attack
							4,		//Recovery	sync
							4);		//Hold
	}
#endif

	if (bPlayLastBlock)
	{
		
	}	

	LEAVE();
}


INT adcInit(VOID)
{
	int nStatus = 0;
	
	ENTER();
	w55fa93ts_close_again();
	DBG("Close touch panel \n");
	w55fa93_adc_recording_setup();

	LEAVE();
	
	return 0;	
}

NV_AUDIO_RECORD_CODEC_T nv_adc_record_codec = {
	set_record_buffer:		adcSetRecordBuffer,
	init_record:			adcInit,	
	start_record:			adcStartRecord,
	stop_record:			adcStopRecord,
	set_record_volume:	adcSetRecordVolume,	
	record_interrupt:		adc_rec_isr,			
};
