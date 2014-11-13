/*----------------------------------------------------------------------------------------------------------
                                                                                                         						
 Copyright (c) Nuvoton Technology Corp. All rights reserved.                                            	 	
                                                                                                         						
-----------------------------------------------------------------------------------------------------------*/
 
/*----------------------------------------------------------------------------------------------------------
 Includes of system headers                                                                              				
----------------------------------------------------------------------------------------------------------*/
//#include "w55fa93_adc.h"
#include <asm/io.h>
#include <asm/arch/w55fa93_adc.h>
#include <linux/delay.h>#define DBG_PRINTF(...)

//#define DRVADC_DEBUG_ENTER_LEAVE
//#define DRVADC_DEBUG
//#define DBG_PRINTF					DBG


#ifdef DRVADC_DEBUG
#define DBG(fmt, arg...)					printk(fmt, ##arg)
#else
#define DBG(fmt, arg...)
#endif

#ifdef DRVADC_DEBUG_ENTER_LEAVE
#define ENTER()						DBG("[%-10s] : Enter\n", __FUNCTION__)
#define LEAVE()						DBG("[%-10s] : Leave\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif




#define outp32(addr, val) 	outl(val, addr)
#define inp32(addr)		inl(addr)  
extern unsigned int w55fa93_upll_clock, w55fa93_apll_clock, w55fa93_ahb_clock;
/*----------------------------------------------------------------------------------------------------------
 Global file scope (static) variables                                                                   	 			
----------------------------------------------------------------------------------------------------------*/
static PFN_DRVADC_CALLBACK g_psADCCallBack[4]={0, 0, 0, 0};
static UINT8 g_u8IsRecord;

/*----------------------------------------------------------------------------------------------------------                                                                                                         						
 FUNCTION                                                                                                					
      DrvADC_SetClockSource()		 		                                                               		
                                                                                                         						
 DESCRIPTION                                                                                             					
      To set the ADC clock source.                                                                       				
                                                                                                         						
 INPUTS                                                                                                  					
      clkSrc   									        		                                       	
        E_DRVSYS_SYS_EXT = 0,														
	E_DRVSYS_SYS_X32K = 1,													
	E_DRVSYS_SYS_APLL = 2,													
	E_DRVSYS_SYS_UPLL = 3														
                                                                                                         						
 OUTPUTS                                                                                                 					
      none                            				                                                   				
                                                                                                         						
 RETURN                                                                                                  					
      none				                                                                               				
                                                                                                         						
----------------------------------------------------------------------------------------------------------*/
void 
DrvADC_SetClockSource(E_SYS_SRC clkSrc)
{
	ENTER();
	UINT32 u32ClkSel = (inp32(REG_CLKDIV3) & (~ADC_S));	
	u32ClkSel = u32ClkSel | (clkSrc << 14);

	outp32(REG_CLKDIV3, u32ClkSel);
}

/*----------------------------------------------------------------------------------------------------------                                                                                                      						
 FUNCTION                                                                                                					
      DrvADC_IntHandler() 		                                                                       				
                                                                                                        						
 DESCRIPTION                                                                                             					
      APU Interrupt Service Routine 								                                       	
                                                                                                         						
 INPUTS                                                                                                  					
      u32UserData     The user's parameter    					                                       		
                                                                                                         						
 OUTPUTS                                                                                                 					
      none                    									                                       		
                                                                                                         						
 RETURN                                                                                                  					
      none				                                                                               				
                                                                                                         						
----------------------------------------------------------------------------------------------------------*/
static void 
DrvADC_IntHandler(
	UINT32 u32UserData
	)
{
	
    	UINT32 u32Reg;
    	ENTER();

    	u32Reg = inp32(REG_ADC_CON);    
    	//Process ADC interrupt 
   	u32Reg = inp32(REG_ADC_CON);
    	if( (u32Reg & (ADC_INT_EN | ADC_INT)) == (ADC_INT_EN | ADC_INT))
    	{//ADC interrupt
		if(g_psADCCallBack[eDRVADC_ADC_INT]!=0)
	        	g_psADCCallBack[eDRVADC_ADC_INT]();  
        	// Clean the ADC interrupt      	
        	outp32(REG_ADC_CON, (inp32(REG_ADC_CON) & 
        				~(WT_INT | LVD_INT)) |  
        				ADC_INT);
    		return;
    	}      
    	if( (u32Reg & (LVD_INT_EN | LVD_INT)) == (LVD_INT_EN | LVD_INT))
    	{//LVD interrupt
    		if(g_psADCCallBack[eDRVADC_LVD_INT]!=0)
            		g_psADCCallBack[eDRVADC_LVD_INT]();
       		 // Clean the LVD interrupt     
        	outp32(REG_ADC_CON, (inp32(REG_ADC_CON) & 
        				~(WT_INT | ADC_INT)) |  
        				LVD_INT);    
    		return;
    	}  
    	if( (u32Reg & (WT_INT_EN | WT_INT)) == (WT_INT_EN | WT_INT))
    	{//Wait for trigger
    		if(g_psADCCallBack[eDRVADC_WT_INT]!=0)
            		g_psADCCallBack[eDRVADC_WT_INT]();
         	//Clean the touch panel interrupt     
		outp32(REG_ADC_CON, (inp32(REG_ADC_CON) & 
        				~(LVD_INT | ADC_INT)) |  
        				WT_INT);                
    		return;
    	}    
	u32Reg = inp32(REG_AUDIO_CON);
	if( (u32Reg & (AUDIO_INT_EN | AUDIO_INT)) == (AUDIO_INT_EN | AUDIO_INT))
	{//Audio record interrupt
		if(g_psADCCallBack[eDRVADC_AUD_INT]!=0)
	        g_psADCCallBack[eDRVADC_AUD_INT]();    		
		// Clean the record interrupt 
		outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) | AUDIO_INT);
	}   
}
/*-----------------------------------------------------------------------------------------------------------
 *
 * Function : sysGetPLLControlRegister
 * 
 * DESCRIPTION : 
 *               According to the external clock and expected PLL output clock to get the content of PLL controll register
 *
 * Parameters 
 *             	u32FinKHz : External clock.  Unit:KHz
 *			u32TargetKHz: PLL output clock. Unit:KHz
 * Return 
 *               	0 : No any fit value for the specified PLL output clock
 *			PLL control register. 
 * HISTORY
 *               2010-07-15 
 *
-----------------------------------------------------------------------------------------------------------*/
UINT32 adcGetPLLControlRegister(UINT32 u32FinKHz, 
							UINT32 u32TargetKHz, 
							BOOL* bIsAccuracy,	
							UINT32 u32InaccuacyKHz )
{
	unsigned int u32OUT_DV, u32IN_DV, u32FB_DV;
	unsigned int u32NR, u32NF, u32NO;
	unsigned int au32Array[4] = {1 , 2, 2, 4};
	unsigned u32Target;
	unsigned int u23Register=0;
	UINT32 u32Index;
	for( u32Index =0; u32Index=u32Index+10; u32Index<u32InaccuacyKHz) 
	{
		for(u32OUT_DV =0 ; u32OUT_DV<4; u32OUT_DV=u32OUT_DV+1)
		{
			for(u32IN_DV =0 ; u32IN_DV<32; u32IN_DV=u32IN_DV+1)
			{				
				for(u32FB_DV =0 ; u32FB_DV<512; u32FB_DV=u32FB_DV+1)
				{
					u32NR =  (2 * (u32IN_DV + 2));
					u32NF = (2 * (u32FB_DV + 2));
					u32NO = au32Array[u32OUT_DV];						
					if( (u32FinKHz/u32NR)<1000 )		
						continue;	
					if( (u32FinKHz/u32NR)>15000)		
						continue;	
					if( ((u32FinKHz/u32NR)*u32NF) <100000)
						continue;
					if( ((u32FinKHz/u32NR)*u32NF) >500000)
						continue;				
					u32Target = u32FinKHz*u32NF/u32NR/u32NO;
					if(u32TargetKHz==u32Target)
					{
						u23Register = (u32OUT_DV<<14) | (u32IN_DV<<9) | (u32FB_DV);					
						return u23Register;
					}					
				}
			}	
		}
		u32TargetKHz = u32TargetKHz -4000;		//- 4MHz
	}
		
	return 0;
}

/*
	System deafult use UPLL as system clock. 
	APLL is for TV, ADC and SPU use.
	Sample rate for 8000, 12000, 20000, 24000 use APLL 153.6MHz
*/
void DrvADC_AudioRecordSampleRate(E_SYS_SRC eSrcClock, UINT32 u32SampleRateKHz)
{
	UINT32 u32TotalDiv, u32Tmp;
	UINT32 u32IdxN0, u32IdxN1; 
	UINT32 u32IdxN00, u32IdxN11; 					
	UINT32 u32PllOutKHz;
	ENTER();

	if(eSrcClock == eDRVSYS_UPLL)
		u32PllOutKHz = w55fa93_upll_clock; 	
	else
		u32PllOutKHz = w55fa93_apll_clock; 	
	DBG("PLL clock = %d KHz\n", u32PllOutKHz);												
	u32TotalDiv = u32PllOutKHz/(1280*u32SampleRateKHz);
	if(u32TotalDiv>(8*256))						
		return E_DRVADC_CLOCK;						
	
	for(u32IdxN1=1;u32IdxN1<=256;u32IdxN1=u32IdxN1+1)
	{
		for(u32IdxN0=2;u32IdxN0 <= 8;u32IdxN0=u32IdxN0+1)								
		{//u32IdxN0 != 1
			if(u32TotalDiv==(u32IdxN0*u32IdxN1))
			{
				u32IdxN00 = u32IdxN0;
				u32IdxN11 = u32IdxN1;									
				break; 
			}	
		}							
		if(u32TotalDiv==((u32IdxN00)*u32IdxN11))											
			break;
		
	}	
	DBG("DIV0 = %d \n", u32IdxN00);	
	DBG("DIV1 = %d \n", u32IdxN11);	
	u32Tmp = (inp32(REG_CLKDIV3) & ~(ADC_N1 | ADC_S | ADC_N0)) | 
					( (((u32IdxN11-1) <<24) | ((u32IdxN00-1) << 16) | (eSrcClock<<19) ));
	outp32(REG_CLKDIV3, u32Tmp);																					
}		

/*---------------------------------------------------------------------------------------------------------                                                                                                 
 FUNCTION                                                                                                
      DrvADC_Open()			                                                                           
                                                                                                         
 DESCRIPTION                                                                                             
      Open the ADC conversion or Audio record function  			                                       
                                                                                                         
 INPUTS                                                                                                  
      mode:   The work mode of ADC. It could be in normal                                                
              ADC conversion mode or audio recording mode		                                           
	                                                                                                         
      u32ConvClock:                                                                                      
              If working in ADC_NORMAL mode, u32ConvClock is the                                         
              conversion rate.                                                                           
              If working in ADC_RECORD mode, u32ConvClock is the                                         
              sampling rate.                                                                             
                                                                                                         
 OUTPUTS                                                                                                 
      none                                                                                               
                                                                                                         
 RETURN                                                                                                  
      E_SUCCESS           Success			                                                               
      E_DRVADC_ARGUMENT   Wrong argument                                                                 
      E_DRVADC_CLOCK      Unable to output a suitable clock                                              
                                                                                                         
---------------------------------------------------------------------------------------------------------*/

#define REAL_CHIP

static UINT32 u32SrcClk;
static UINT32 u32ApllReg; 
static UINT32 u32ApllClock; 
BOOL bIsAudioInitialize = FALSE;
ERRCODE 
DrvADC_Open(
	E_DRVADC_MODE eDRVADC_Mode, 
	E_SYS_SRC eSrcClock, 
	UINT32 u32ConvClock
	)
{
	UINT32 u32PllOutKHz, u32ExtFreq;
	UINT32 u32Tmp;
	UINT32 u32Reg;
	ENTER();

	/* Back up APLL content */
	u32ApllClock = w55fa93_apll_clock;
	u32ApllReg = inp32(REG_APLLCON);
#if 0
	outp32(REG_APLLCON, 0x867E);	/* APLL to 153.6MHz */  
	w55fa93_apll_clock = 153600;	
#else
	w55fa93_set_apll_clock(153600);
#endif
	if((eDRVADC_Mode != eDRVADC_NORMAL) && (eDRVADC_Mode != eDRVADC_RECORD))
	{													    
	    	return E_DRVADC_ARGUMENT;
	}	           
    	 /* Enable clock and IP reset */
    	outp32(REG_APBCLK, inp32(REG_APBCLK) | ADC_CKE);
    	outp32(REG_APBIPRST, inp32(REG_APBIPRST) | ADCRST);
   	outp32(REG_APBIPRST,  inp32(REG_APBIPRST) & ~ADCRST);
	/* Default to use conv bit to control conversion */
	u32Reg = 0;
	u32Reg = u32Reg | (ADC_CON_ADC_EN); /* Enable ADC */

    	/* Use the same clock source as system */
	outp32(REG_CLKDIV3, (inp32(REG_CLKDIV3) & ~ADC_S) | 
					(eSrcClock << 19));	
	u32SrcClk = eSrcClock;					
	switch(eSrcClock)
	{
		case eDRVSYS_X32K:	
					return E_DRVADC_CLOCK;			/* Wrong clock source */			
					break;
		case eDRVSYS_APLL:												
		case eDRVSYS_UPLL:			
					{
						UINT32 u32TotalDiv;
						UINT32 u32IdxN0, u32IdxN1; 
						UINT32 u32IdxN00, u32IdxN11; 					
#if 0
						u32ExtFreq = sysGetExternalClock();						
						u32PllOutKHz = sysGetPLLOutputKhz(eSYS_UPLL, u32ExtFreq);						
#else
						
						if(eSrcClock == eDRVSYS_UPLL)
							u32PllOutKHz = w55fa93_upll_clock; 	
						else
							u32PllOutKHz = w55fa93_apll_clock; 	
#endif
						DBG_PRINTF("PLL clock = %d KHz\n", u32PllOutKHz);												
						if(eDRVADC_Mode == eDRVADC_RECORD)
							u32TotalDiv = u32PllOutKHz/(1280*u32ConvClock);
						else
							u32TotalDiv = (u32PllOutKHz/50)/u32ConvClock;	
						
						if(u32TotalDiv>(8*256))						
							return E_DRVADC_CLOCK;						
						/*	
						if(u32TotalDiv%2 !=0)	
							u32TotalDiv = u32TotalDiv +1;						
						*/	
						for(u32IdxN1=1;u32IdxN1<=256;u32IdxN1=u32IdxN1+1)
						{
							for(u32IdxN0=2;u32IdxN0 <= 8;u32IdxN0=u32IdxN0+1)								
							{//u32IdxN0 != 1
								if(u32TotalDiv==(u32IdxN0*u32IdxN1))
								{
									u32IdxN00 = u32IdxN0;
									u32IdxN11 = u32IdxN1;		
									DBG_PRINTF("ADC DIV0 = %d \n", u32IdxN00);		
									DBG_PRINTF("ADC DIV1 = %d \n", u32IdxN11);									
									break; 
								}	
							}							
							if(u32TotalDiv==((u32IdxN00)*u32IdxN11))											
								break;
							
						}	
						u32Tmp = (inp32(REG_CLKDIV3) & ~(ADC_N1 | ADC_S | ADC_N0)) | 
										( (((u32IdxN11-1) <<24) | ((u32IdxN00-1) << 16) | (eSrcClock<<19) ));
						outp32(REG_CLKDIV3, u32Tmp);															
					}					
					break;
		case eDRVSYS_EXT:	
					{
						UINT32 u32ExtClk, u32AdcDivN1;
						//u32ExtClk = sysGetExternalClock();	
						u32ExtClk = 12000;  	// 12MHz										
						u32AdcDivN1 = (u32ExtClk)/u32ConvClock;	
						if(u32AdcDivN1>256)
							return E_DRVADC_CLOCK;
						outp32(REG_CLKDIV3, (inp32(REG_CLKDIV3) & ~(ADC_N1 | ADC_N0)) |
											((u32AdcDivN1-1) <<24) );	
					}									
					break;
	}														  

	outp32(REG_ADC_CON, u32Reg);

	g_u8IsRecord = 0;
	if(eDRVADC_Mode == eDRVADC_RECORD)
	{
		/* Reset Record Function */
	    	volatile UINT32 u32Dly=0x100;
	    	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) | AUDIO_RESET);
	    	while(u32Dly--);
	    	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) & ~(AUDIO_RESET));	    
	    	/* Default to Audio Interrupt Mode 0, op offset:b'1000, interrupt enabled */
	    	outp32(REG_AUDIO_CON, (AUDIO_HPEN | AUDIO_INT | AUDIO_INT_EN |AUDIO_VOL_EN | AUDIO_RESET));
	   	outp32(REG_AUDIO_CON, (inp32(REG_AUDIO_CON) & ~(AUDIO_CCYCLE | AUDIO_RESET)) | (0x50<<16));//ADC cycle = 50
	    
	   	/* Hardware offset calibration */       
		if(bIsAudioInitialize==FALSE)
		{
			volatile UINT32 u32Delay;
			bIsAudioInitialize = TRUE;
		        outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) | 
				    			 ( AUDIO_INT | AUDIO_EN ) );

		    	DrvADC_SetOffsetCancellation(FALSE,   	//BOOL bIsMuteEnable,
										FALSE, 	//BOOL bIsOffsetCalibration,
										TRUE, 	//BOOL bIsHardwareMode,
										0x10);	//UINT32 u32Offset   	  		   				  		   				
			DrvADC_SetOffsetCancellationEx(1,		//255 sample
										256);	//Delay sample count  
			
			for(u32Delay=0; u32Delay<800; u32Delay=u32Delay+1)
			{//Delay 800ms for stable 
				udelay(1000);
			}
	
	        	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) & ~AUDIO_EN);
	        	DrvADC_SetOffsetCancellation(FALSE,   	//BOOL bIsMuteEnable,
										FALSE, 	//BOOL bIsOffsetCalibration,
										FALSE, 	//BOOL bIsHardwareMode,
										0x10);	//UINT32 u32Offset 
										//0x1A);	//UINT32 u32Offset 
		}													       	    
	    	g_u8IsRecord = 1;  
	}
	else
	{
		 outp32(REG_AUDIO_CON, (inp32(REG_AUDIO_CON) & ~AUDIO_CCYCLE) | (50<<16));
	}	
	return 0;	
}


/*---------------------------------------------------------------------------------------------------------                                                                                             
 FUNCTION                                                                                                
      DrvADC_Close()			                                                                           
                                                                                                         
 DESCRIPTION                                                                                             
     	close ADC								 					                                       
                                                                                                         
 INPUTS                                                                                                  
      none														                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none														                                       
                                                                                                         
 RETURN                                                                                                  
      none				                                                                               
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void 
DrvADC_Close(
	void
	)
{     
	ENTER();

	while(!DrvADC_IsConvertReady());
	outp32(REG_APBIPRST, inp32(REG_APBIPRST) | ADCRST);
	outp32(REG_APBIPRST,  inp32(REG_APBIPRST) & ~ADCRST);
	//sysEnableInterrupt(IRQ_ADC);
	outp32(REG_ADC_CON, inp32(REG_ADC_CON)&~ADC_CON_ADC_EN);
	outp32(REG_APBCLK, inp32(REG_APBCLK) & ~ADC_CKE);

	/* restore APLL */ 
	outp32(REG_APLLCON, u32ApllReg & 0xFFFF);	/* APLL to 153.6MHz */  
	w55fa93_apll_clock = u32ApllClock;	

	bIsAudioInitialize = FALSE;	
}


/*----------------------------------------------------------------------------------------------------------
 FUNCTION                                                                                                					
 		DrvADC_StartRecord()		                                                                       			
                                                                                                         						
 DESCRIPTION                                                                                             					
     	Start to record Audio data. This function only can be used in                                      		
      ADC_RECORD mode.                                                                                   				
                                                                                                         						
 INPUTS                                                                                                  					
      none														                        
                                                                                                         						
 OUTPUTS                                                                                                 					
      none														                        
                                                                                                         						
 RETURN                                                                                                  					
      none				                                                                               				
                                                                                                         						
----------------------------------------------------------------------------------------------------------*/
void 
DrvADC_StartRecord(
	E_DRVADC_RECORD_MODE eRecordMode
	)
{
	ENTER();
    	// Clean INT status for safe 
    	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) | 
    				(AUDIO_RESET | AUDIO_INT) );
	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) & 
					~(AUDIO_RESET | AUDIO_INT) );
     	//Enable record function 
    	outp32(REG_AUDIO_CON,  (inp32(REG_AUDIO_CON) & ~AUDIO_INT_MODE) | 
						   (((eRecordMode << 30) &  AUDIO_INT_MODE) |
						   AUDIO_EN) );
	
}


/*----------------------------------------------------------------------------------------------------------
 FUNCTION                                                                                                					
 		DrvADC_StopRecord()                                                                                			
                                                                                                         						
 DESCRIPTION                                                                                             					
     	Stop recording Audio data. This function only can be used in                                       		
      ADC_RECORD mode.                                                                                   				
                                                                                                         						
 INPUTS                                                                                                  					
      none														                        
                                                                                                         						
 OUTPUTS                                                                                                 					
      none														                        
                                                                                                         						
 RETURN                                                                                                  					
      none				                                                                               				
                                                                                                         						
----------------------------------------------------------------------------------------------------------*/
void 
DrvADC_StopRecord(
	void
	)
{
	ENTER();
    	//Disable record function 
	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) & (~AUDIO_EN));
}


/*----------------------------------------------------------------------------------------------------------                                                                                                 						
 FUNCTION                                                                                                					
 		DrvADC_StartConvert()		                                                                       			
                                                                                                         						
 DESCRIPTION                                                                                             					
     	Start to convert ADC data. This function only can be used in                                       	
      ADC_NORMAL mode.                                                                                   				
                                                                                                         						
 INPUTS                                                                                                  					
      u32Channel: The analog input channel and it could be ch2~ch7.                                      		
                                                                                                         						
 OUTPUTS                                                                                                 					
      none														                        
                                                                                                         						
 RETURN                                                                                                  					
      none				                                                                               				
                                                                                                         						
----------------------------------------------------------------------------------------------------------*/
void 
DrvADC_StartConvert(
	UINT32 u32Channel
	)
{
	ENTER();
    	
	if((u32Channel >= 2) && (u32Channel <= 7))
    	{
         	//Clean INT flag for safe and trigger start 
	    	outp32(REG_ADC_CON, (inp32(REG_ADC_CON)|ADC_CONV) | 
	    				((u32Channel << 9)| ADC_INT) );
    	}
}


/*----------------------------------------------------------------------------------------------------------                                                                                               						
 FUNCTION                                                                                                					
 		DrvADC_IsConvertReady()	                                                                           		
                                                                                                         						
 DESCRIPTION                                                                                             					
     	check if ADC (not audio) is converted OK       			    	                                   	
                                                                                                         						
 INPUTS                                                                                                  					
      none														                        
                                                                                                         						
 OUTPUTS                                                                                                 					
      none 														                        
                                                                                                         						
 RETURN                                                                                                  					
      TURE : Conversion finished		    						                                       	
      FALSE: Under converting         							                                       		
                                                                                                         						
----------------------------------------------------------------------------------------------------------*/
BOOL 
DrvADC_IsConvertReady(
	void
	)
{
	ENTER();

	return ((inp32(REG_ADC_CON)&ADC_FINISH)? TRUE:FALSE);		//Check finished?	
}					

BOOL 
DrvADC_Polling_ADCIntStatus(
	void
	)
{
	ENTER();

	return ((inp32(REG_ADC_CON)&ADC_INT)? TRUE:FALSE);			//Check Int?
}
/*----------------------------------------------------------------------------------------------------------                                                                                                         						
 FUNCTION                                                                                                					
 		DrvADC_GetConvertData()	                                                                           		
                                                                                                         						
 DESCRIPTION                                                                                             					
     	Get the converted ADC value in ADC_NORMAL mode    				                        
                                                                                                         						
 INPUTS                                                                                                  					
      none														                        
                                                                                                         						
 OUTPUTS                                                                                                 					
      none 														                       	
                                                                                                         						
 RETURN                                                                                                  					
      The ADC value            		    						                                       		
                                                                                                         						
----------------------------------------------------------------------------------------------------------*/
UINT32 
DrvADC_GetConvertData(
	void
	)
{
	ENTER();
	
    	return (inp32(REG_ADC_XDATA));
}


/*---------------------------------------------------------------------------------------------------------                                                                                                     
 FUNCTION                                                                                                
 		DrvADC_GetRecordData()	                                                                           
                                                                                                         
 DESCRIPTION                                                                                             
     	Get the converted ADC value in ADC_RECORD mode    				                                   
                                                                                                         
 INPUTS                                                                                                  
      none														                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      The pointer to the record data. The data length is 8 samples                                        
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
PINT16 
DrvADC_GetRecordData(void)
{
	ENTER();
     	
	//Return the base address of converted data. There are 8 samples 
    	return (PINT16)REG_AUDIO_BUF0;
}
/*
void DrvADC_EnableHighPassFilter(
	BOOL bIsEnableFilter
	)
{
	UINT32 u32RegData = inp32(REG_AUDIO_CON);
	bIsEnableFilter?(u32RegData=u32RegData|AUDIO_HPEN):(u32RegData=u32RegData&(~AUDIO_HPEN));
	outp32(REG_AUDIO_CON, u32RegData);
}	

BOOL DrvADC_IsEnableHighPassFilter(void)
{
	return ((inp32(REG_AUDIO_CON) & AUDIO_HPEN)>>26);
}
*/
/*---------------------------------------------------------------------------------------------------------                                                                                                         
 FUNCTION                                                                                                
      DrvADC_GetConvertClock()	                                                                       
                                                                                                         
 DESCRIPTION                                                                                             
      To get the conversion clock or recording sampling rate.  	                                       
                                                                                                         
 INPUTS                                                                                                  
      None                                                                                               
                                                                                                         
                                                                                                         
 OUTPUTS                                                                                                 
      None                                                                                               
                                                                                                         
 RETURN                                                                                                  
      Return the conversion clock if it is in ADC_NORMAL mode.                                           
      Return the recording sampling rate if it is in ADC_RECORD mode                                     
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
UINT32 
DrvADC_GetConvertClock(
	void
	)
{   
    	UINT32 u32ConvClock, u32Divider;
    	UINT32 u32ExtFreq, u32PllOutKHz;	
    	UINT32 u32RegData; 
	ENTER();


#if 0
	u32ExtFreq = sysGetExternalClock();
	u32PllOutKHz = sysGetPLLOutputKhz(eSYS_UPLL, u32ExtFreq);
	u32RegData = inp32(REG_CLKDIV3);
#else
	u32PllOutKHz = w55fa93_upll_clock; 	
	//u32PllOutKHz = sysGetPLLOutputKhz(eSYS_UPLL, u32ExtFreq);
	u32RegData = inp32(REG_CLKDIV3);
#endif
    	u32Divider = (((u32RegData &ADC_N1)>> 24) + 1) * (((u32RegData &ADC_N0)>> 16) + 1);
	if(g_u8IsRecord)
	    	u32ConvClock = (u32PllOutKHz / 1280UL) / u32Divider;			//==> record
	else
	    	u32ConvClock = (u32PllOutKHz / 34UL) / u32Divider;				//==> Normal ADC

	return u32ConvClock;
}

/*---------------------------------------------------------------------------------------------------------                                                                                                         
FUNCTION                                                                                               
	DrvADC_EnableLvd()		                                                                           
                                                                                                 
DESCRIPTION                                                                                             
	Enable low voltage detection function                                                              
                                                                                               
INPUTS                                                                                                
	u32Level - [in], Set trigger voltage. it could be 0~7 and the relative voltages are 2.0, 2.1, 2.2  
               2.3, 2.4, 2.5, 2.6, 2.7 Volt.  			                                       
                                                                                                 
OUTPUTS                                                                                               
	none														                                       
                                                                                                
RETURN                                                                                                  
	none				                                                                               
                                                                                                
REMARKS                                                                                                
	LVD interrupt enable must be set first if using LVD interrupt.	                                   
	Because the LVD interrupt status bit will be set after LV_EN 	                                   
	bit was set. 													                                  
                                                                                                        
---------------------------------------------------------------------------------------------------------*/
void 
DrvADC_EnableLvd(
	UINT32 u32Level
	)
{
	ENTER();

	if(u32Level > 7)
		u32Level = 7;
		
	outp32(REG_LV_CON, (inp32(REG_LV_CON) & (~SW_CON)) |
						 (LV_EN | u32Level));
}


/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_DisableLvd()		                                                                           
                                                                                                         
 DESCRIPTION                                                                                             
     	Disable the low voltage detection function      			                                       
                                                                                                         
 INPUTS                                                                                                  
      none														                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none														                                       
                                                                                                         
 RETURN                                                                                                  
      none				                                                                               
                                                                                                         
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void 
DrvADC_DisableLvd(
	void
	)
{
	ENTER();

	outp32(REG_LV_CON,inp32(REG_LV_CON)&(~LV_EN));
}


/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_EnableInt()		                                   	   	                                   
                                                                                                         
 DESCRIPTION                                                                                             
     	enable ADC interrupt and setup callback function 	        	                                   
                                                                                                         
 INPUTS                                                                                                  
      callback                                                                                           
          The callback funciton                                                                          
                                                                                                         
      u32UserData                                                                                        
          The user's data to pass to the callback function                                               
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      none														                                       
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvADC_EnableInt(
	E_DRVADC_INT eIntType
	)
{
	ENTER();

   	// Enable adc interrupt 
    	switch(eIntType)
    	{
    		case eDRVADC_ADC_INT: 
		 	outp32(REG_ADC_CON, inp32(REG_ADC_CON) | ADC_INT_EN);	    		                                     
    	    	break; 
	   	case eDRVADC_AUD_INT:
	   		outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) | AUDIO_INT_EN);	 
	   		break;
		case eDRVADC_LVD_INT:
			outp32(REG_ADC_CON, inp32(REG_ADC_CON) | LVD_INT_EN);	 
			break;
		case eDRVADC_WT_INT:     
			outp32(REG_ADC_CON, inp32(REG_ADC_CON) | WT_INT_EN);	 
			break;	
		default:
			return E_DRVADC_INVALID_INT;
    	}			
    	return Successful;
}

ERRCODE 
DrvADC_DisableInt(
	E_DRVADC_INT eIntType
	)
{
	ENTER();

	 // Enable adc interrupt 
    	switch(eIntType)
    	{
    		case eDRVADC_ADC_INT:
    			outp32(REG_ADC_CON, inp32(REG_ADC_CON) & ~ADC_INT_EN);	                                      
    	    	break; 
	   	case eDRVADC_AUD_INT:
	   		outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) & ~AUDIO_INT_EN);	 
	   		break;
		case eDRVADC_LVD_INT:
			outp32(REG_ADC_CON, inp32(REG_ADC_CON) & ~LVD_INT_EN);	
			break;
		case eDRVADC_WT_INT:
			outp32(REG_ADC_CON, inp32(REG_ADC_CON) & ~WT_INT_EN);     
			break;	
		default:
			return E_DRVADC_INVALID_INT;
    }			
    return Successful;
}

ERRCODE 
DrvADC_ClearInt(
	E_DRVADC_INT eIntType
	)
{
	ENTER();

    	switch(eIntType)
    	{
    		case eDRVADC_ADC_INT: 
		 	outp32(REG_ADC_CON, (inp32(REG_ADC_CON) & ~(ADC_INT|LVD_INT|WT_INT)) | 
		 					ADC_INT);	    		                                     
    	    	break; 
	   	case eDRVADC_AUD_INT:
	   		outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) | AUDIO_INT);	 
	   		break;
		case eDRVADC_LVD_INT:
			outp32(REG_ADC_CON, (inp32(REG_ADC_CON) & ~(ADC_INT|LVD_INT|WT_INT)) | 
							LVD_INT);	 
			break;
		case eDRVADC_WT_INT:     
			outp32(REG_ADC_CON, (inp32(REG_ADC_CON) & ~(ADC_INT|LVD_INT|WT_INT)) |  
							WT_INT);	 
			break;	
		default:
			return E_DRVADC_INVALID_INT;	
    }			
   	return Successful;    	
}	

BOOL 
DrvADC_PollInt(
	E_DRVADC_INT eIntType
	)
{
	ENTER();
	
   	UINT32 u32IntSt = 0;
    	switch(eIntType)
    	{
    		case eDRVADC_ADC_INT: 
		 	u32IntSt = inp32(REG_ADC_CON) & ADC_INT;	    		                                     
    	    	break; 
	   	case eDRVADC_AUD_INT:
	   		u32IntSt = inp32(REG_AUDIO_CON) & AUDIO_INT;	 
	   		break;
		case eDRVADC_LVD_INT:
			u32IntSt = inp32(REG_ADC_CON) & LVD_INT;	 
			break;
		case eDRVADC_WT_INT:     
			u32IntSt = inp32(REG_ADC_CON) & WT_INT;	 
			break;	
    }			
    if( u32IntSt != 0)
    	return TRUE;
	else
    	return FALSE;		    	
}


ERRCODE 
DrvADC_InstallCallback(
	E_DRVADC_INT eIntType,
	PFN_DRVADC_CALLBACK pfnCallback,
	PFN_DRVADC_CALLBACK* pfnOldCallback
	)
{
	ENTER();

	switch(eIntType)
    	{
    		case eDRVADC_ADC_INT:
    			*pfnOldCallback = g_psADCCallBack[eDRVADC_ADC_INT];
    			g_psADCCallBack[eDRVADC_ADC_INT] = pfnCallback; 	                                      
    	    	break; 
	   	case eDRVADC_AUD_INT:
	   		*pfnOldCallback = g_psADCCallBack[eDRVADC_AUD_INT];
    		g_psADCCallBack[eDRVADC_AUD_INT] = pfnCallback; 	 
	   		break;
		case eDRVADC_LVD_INT:
			*pfnOldCallback = g_psADCCallBack[eDRVADC_LVD_INT];
    		g_psADCCallBack[eDRVADC_LVD_INT] = pfnCallback; 
			break;
		case eDRVADC_WT_INT:
			*pfnOldCallback = g_psADCCallBack[eDRVADC_WT_INT];
    		g_psADCCallBack[eDRVADC_WT_INT] = pfnCallback;  
			break;	
		default:
			return E_DRVADC_INVALID_INT;
    }			
    return Successful;
}

/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_IsLowVoltage()		                                                                       
                                                                                                         
 DESCRIPTION                                                                                             
     	Get the low voltage status.              	 					                                   
                                                                                                         
 INPUTS                                                                                                  
      None														                                       
                                                                                                         
 OUTPUTS                                                                                                 
      None 														                                       
                                                                                                         
 RETURN                                                                                                  
      TURE: low voltage detected									                                       
      FALSE: not low voltage detected    						                                           
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
BOOL 
DrvADC_IsLowVoltage(
	void
	)
{
	ENTER();

	return ((inp32(REG_LV_STS)&LV_status)? TRUE:FALSE);
}						


/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_IsAudioDataReady()	                                                                       
                                                                                                         
 DESCRIPTION                                                                                             
     	Check if the recording data is converted OK.        			                                   
                                                                                                         
 INPUTS                                                                                                  
      none														                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      TURE: data is ready											                                       
      FALSE: data is not ready									                                       
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
BOOL 
DrvADC_GetRecordReadyFlag(
	void
	)
{
	ENTER();
	
	return ((inp32(REG_AUDIO_CON)&AUDIO_INT)? TRUE:FALSE);
}	


/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_ClearRecordReadyFlag()	                                                                   
                                                                                                         
 DESCRIPTION                                                                                             
     	To clear the recording ready flag.       	        			                                   
                                                                                                         
 INPUTS                                                                                                  
      None														                                       
                                                                                                         
 OUTPUTS                                                                                                 
      None 														                                       
                                                                                                         
 RETURN                                                                                                  
      None	                									                                       
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_ClearRecordReadyFlag(
	void
	)
{
	ENTER();

    	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) & ~AUDIO_INT);
    	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) | AUDIO_INT);
}	


/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_SetMICGain()	                      		                                                   
                                                                                                         
 DESCRIPTION                                                                                             
     	Set record volume gain       									                                   
                                                                                                         
 INPUTS                                                                                                  
      u16MicGainLevel						    					                                       
          The volume gain could be 0 ~ 31 dB.                                                            
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      none														                                       
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_SetMICGain(
	UINT16 u16MicGainLevel
	)
{
	ENTER();

	outp32(REG_AGCP1, (inp32(REG_AGCP1)&(~AUDIO_VOL)) |
	       (u16MicGainLevel & AUDIO_VOL));

	outp32(REG_AUDIO_CON, inp32(REG_AUDIO_CON) | AUDIO_VOL_EN);       
}	


/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_GetMICGain()	                      		                                                   
                                                                                                         
 DESCRIPTION                                                                                             
     	Get record volume gain.       									                                   
                                                                                                         
 INPUTS                                                                                                  
      None        						    					                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      Recording gain in dB.										                                       
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
INT16 DrvADC_GetMICGain(
	void
	)
{
	ENTER();

	return inp32(REG_AGCP1) & AUDIO_VOL;
}	

/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_SetGainControl()	                      		                                               
                                                                                                         
 DESCRIPTION                                                                                             
     	Set Pre-Amplifer and Post-Amplifer					                   							   
                                                                                                         
 INPUTS                                                                                                  
      None        						    					                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      Recording gain in dB.										                                       
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_SetGainControl(
	E_DRVADC_PREGAIN ePreGain, 
	E_DRVADC_POSTGAIN ePostGain
	)
{
	ENTER();

     	outp32(REG_AGCP1, (inp32(REG_AGCP1) & ~(AUDIO_VOL|PRAGA)) | 
     				((ePreGain<<8)|ePostGain)); 								     				
}	

void DrvADC_GetGainControl(
	E_DRVADC_PREGAIN* pePreGain, 
	E_DRVADC_POSTGAIN* pePostGain
	)
{
	ENTER();

	UINT32 u32RegData = inp32(REG_AGCP1);
	*pePreGain =  (u32RegData & PRAGA)>>8;
	*pePostGain = u32RegData & AUDIO_VOL;							     				
}	
/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_SetOffsetCancellation()	                      		                                       
                                                                                                         
 DESCRIPTION                                                                                             
      The function is only for OP offset callcellation				    								
                                                                                                         
 INPUTS                                                                                                  
      None        						    					                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      Recording gain in dB.										                                       
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_SetOffsetCancellation(
	BOOL bIsMuteEnable,
	BOOL bIsOffsetCalibration,
	BOOL bIsHardwareMode,
	UINT32 u32Offset
	)
{
	ENTER();

	outp32(REG_OPOC, (inp32(REG_OPOC) & ~(MUTE_SW | OOC | OPOCM |OPOC_SW)) |
				 ((((bIsMuteEnable ? MUTE_SW:0) |
				 (bIsOffsetCalibration ? OOC:0)) |
				 (bIsHardwareMode ? OPOCM:0)) | 
				 ((u32Offset<<24)&OPOC_SW)) );
}	

void DrvADC_GetOffsetCancellation(
	PBOOL pbIsMuteEnable,
	PBOOL pbIsOffsetCalibration,
	PBOOL pbIsHardwareMode,
	PUINT32 pu32Offset
	)
{
	ENTER();

	UINT32 u32RegData = inp32(REG_OPOC);
	*pbIsMuteEnable = (u32RegData & MUTE_SW)>>31;
	*pbIsOffsetCalibration = (u32RegData & OOC)>>30;
	*pbIsHardwareMode = (u32RegData & OPOCM)>>29;
	*pu32Offset = (u32RegData & OPOC_SW)>>24;
}	

void DrvADC_SetOffsetCancellationEx(
	UINT32 u32SampleNumber,
	UINT32 u32DelaySampleCount
	)
{
	ENTER();

	outp32(REG_OPOC, (inp32(REG_OPOC) & ~(OPOC_TCSN | OPOC_DSC)) |
				 (((u32SampleNumber<<16) & OPOC_TCSN) |
				 (u32DelaySampleCount & OPOC_DSC)) );
}
void DrvADC_GetOffsetCancellationEx(
	PUINT32 pu32SampleNumber,
	PUINT32 pu32DelaySampleCount
	)
{
	ENTER();

	UINT32 u32RegData = inp32(REG_OPOC);	
	*pu32SampleNumber = u32RegData & OPOC_TCSN;
	*pu32DelaySampleCount = u32RegData & OPOC_DSC;
}
void DrvADC_GetOffsetSummarry(
	PUINT32 pu32Offset,
	PUINT32 pu32Summation
	)
{
	UINT32 u32RegData = inp32(REG_OPOCS1);	
	*pu32Offset = u32RegData & OP_OFFSET_CAL;
	*pu32Summation = u32RegData & ADC_DATA_SUM;
}
/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_SetNoiseGate()	                      		                                                   
                                                                                                         
 DESCRIPTION                                                                                             
     	Set Pre-Amplifer, Post-Amplifer and offset(Offset Cancellation					                   
                                                                                                         
 INPUTS                                                                                                  
      None        						    					                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      Noise gate Level gain in -24, -30, -36, -42dB.										               
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_SetNoiseGate(
	BOOL bIsEnable, 
	E_DRVADC_NOISEGATE eNoiseGateLevel
	)
{
	ENTER();

     	outp32(REG_AGC_CON, (inp32(REG_AGC_CON) & ~(NG_EN |NG_LEVEL)) | 
     				(((bIsEnable <<31)& NG_EN) |
     				((eNoiseGateLevel <<12)& NG_LEVEL)) );									     				
    				
}	

/*---------------------------------------------------------------------------------------------------------
                                                                                                         sound/oss/w55fa93_adc.c:1387: error: expected ';' before ')' token

 FUNCTION                                                                                                
 		DrvADC_GetNoiseGate()	                      		                                                   
                                                                                                         
 DESCRIPTION                                                                                             
     	Set Pre-Amplifer, Post-Amplifer and offset(Offset Cancellation					                   
                                                                                                         
 INPUTS                                                                                                  
      None        						    					                                       
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      Noise gate Level gain in -24, -30, -36, -42dB.										               
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_GetNoiseGate(
	PBOOL pbIsEnable, 
	E_DRVADC_NOISEGATE* peNoiseGateLevel
	)
{
	ENTER();

	UINT32 u32RegData = inp32(REG_AGC_CON);
	*pbIsEnable = (u32RegData & NG_EN)>>31;
	*peNoiseGateLevel = (u32RegData & NG_LEVEL)>>12;
    						     				
}	

/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_SetAutoGainControl()	                      		                                               
                                                                                                         
 DESCRIPTION                                                                                             
     	Set the parameter for AGC														                   
                                                                                                         
 INPUTS                                                                                                  
      bIsEnable    	Enable AGC    						    					                       
      u32OutputLevel  Output target level      						    					           
      eUpBand        	A band in the uper side from u32OutputLevel+-eUpBand							   
      eDownBand       A band in the buttom side from u32OutputLevel+-eUpBand					           
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      none																				               
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_SetAutoGainControl(
	BOOL bIsEnable, 
	UINT32 u32OutputLevel,
	E_DRVADC_UPBAND eAdcUpBand,
	E_DRVADC_DOWNBAND eAdcDownBand
	)
{
	ENTER();

     	outp32(REG_AGC_CON, (inp32(REG_AGC_CON) & ~AGC_EN) | 
     				((bIsEnable <<30)& AGC_EN) );     				
     	outp32(REG_AGCP1, ( (inp32(REG_AGCP1) & ~(OTL | UPBAND | DOWNBAND)) | 
     				((u32OutputLevel<<12) & OTL) ) |
     				(((eAdcUpBand <<11)& UPBAND) | 
     				((eAdcDownBand <<10)& DOWNBAND)) );											     				     											     											     											     				
}	
	
/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_GetAutoGainControl()	                      		                                           
                                                                                                         
 DESCRIPTION                                                                                             
     	Set Pre-Amplifer, Post-Amplifer and offset(Offset Cancellation					                   
                                                                                                         
 INPUTS                                                                                                  
      None        						    					                                       
 OUTPUTS                                                                                                 
      bIsEnable    	Enable AGC    						    					                       
      u32OutputLevel  Output target level      						    					           
      eUpBand        	A band in the uper side from u32OutputLevel+-eUpBand							   
      eDownBand       A band in the buttom side from u32OutputLevel+-eUpBand					           
                                                                                                         
 RETURN                                                                                                  
      None.										               										   
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_GetAutoGainControl(
	PBOOL pbIsEnable, 
	PUINT32 pu32OutputLevel,
	E_DRVADC_UPBAND* peAdcUpBand,
	E_DRVADC_DOWNBAND* peAdcDownBand
	)
{
	ENTER();

	UINT32 u32RegData = inp32(REG_AGC_CON);
	*pbIsEnable = (u32RegData & AGC_EN)>>30;
	u32RegData = inp32(REG_AGCP1);
	*pu32OutputLevel = (u32RegData & OTL)>>12; 
    	*peAdcUpBand = 	(u32RegData & UPBAND)>>11; 					     				
    	*peAdcDownBand = (u32RegData & DOWNBAND)>>10; 						     				    						     						     				
}	


void DrvADC_SetAutoGainControlEx(
	BOOL bAttachGainFast, 
	E_DRVADC_PEAK_METHOD ePeakMethod
	)
{
	ENTER();

     	outp32(REG_AGC_CON, (inp32(REG_AGC_CON) & ~(PAVG_MODE | AGAIN_STEP)) | 
     				(((bAttachGainFast <<15)& AGAIN_STEP) |
     				((ePeakMethod<<28) & PAVG_MODE)) );     											     				     											     											     											     				
}

void DrvADC_GetAutoGainControlEx(
	PBOOL pbAttachGainFast, 
	E_DRVADC_PEAK_METHOD* pePeakMethod
	)
{
	ENTER();

	UINT32 u32RegData = inp32(REG_AGC_CON);
	*pbAttachGainFast = (u32RegData & AGAIN_STEP)>>15;
	*pePeakMethod = (u32RegData & PAVG_MODE)>>28; 			     				    						     						     				
}	

/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_SetClampingAGC()	                      		                                               
                                                                                                         
 DESCRIPTION                                                                                             
     	Set the parameter for AGC														                   
                                                                                                         
 INPUTS                                                                                                  
      eAdcMaxClamp    Clamp AGC gain. The output level will be  					                       
						1. Input level + Max gain(db) if the value less OTL								   
						2. OTL if input level + Max gain(db) great OTL									   						         						    					           
      eAdcMinClamp    A band in the uper side from u32OutputLevel+-eUpBand							   
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      none																				               
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_SetClampingAGC(
	E_DRVADC_MAX_CLAMP eAdcMaxClamp,
	E_DRVADC_MIN_CLAMP eAdcMinClamp
	)
{		
	ENTER();

     	outp32(REG_AGCP1, (inp32(REG_AGCP1) & ~(MAXGAIN | MINGAIN )) | 
     				(((eAdcMaxClamp << 20) & MAXGAIN)  |
     				((eAdcMinClamp << 16) & MINGAIN)) );											     				     											     											     											     				
}	

void DrvADC_GetClampingAGC(
	E_DRVADC_MAX_CLAMP* peAdcMaxClamp,
	E_DRVADC_MIN_CLAMP* peAdcMinClamp
	)
{
	ENTER();

	UINT32 u32RegData = inp32(REG_AGCP1);
	*peAdcMaxClamp = (u32RegData & MAXGAIN)>>20;
	*peAdcMinClamp = (u32RegData & MINGAIN)>>16;				     				    						     						     				
}	

/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_SetAutoGainTiming()	                      		                                           
                                                                                                         
 DESCRIPTION                                                                                             
     	Set the parameter for AGC														                   
                                                                                                         
 INPUTS                                                                                                  
      u32Period    	Detect max peak in the how many samples    					                       
      u32Attack 		      						    					           					   
      u32Recovery        	A band in the uper side from u32OutputLevel+-eUpBand						   
      u32Hold       A band in the buttom side from u32OutputLevel+-eUpBand					           
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      none																				               
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_SetAutoGainTiming(
	UINT32 u32Period,
	UINT32 u32Attack,
	UINT32 u32Recovery,
	UINT32 u32Hold
	)
{  		
	ENTER();
	
     	outp32(REG_AGC_CON, ( (inp32(REG_AGC_CON) & ~(PERIOD | ATTACK | RECOVERY| HOLD)) | 
     				( ((u32Period<<16) & PERIOD) |
     				((u32Attack<<8) & ATTACK)) ) |     				
     				( ((u32Recovery <<4)& RECOVERY) | 
     				(u32Hold & HOLD) ) );
    															     				     											     											     											     				
}	
void DrvADC_GetAutoGainTiming(
	PUINT32 pu32Period,
	PUINT32 pu32Attack,
	PUINT32 pu32Recovery,
	PUINT32 pu32Hold
	)
{  		
	ENTER();

	UINT32 u32RegData = inp32(REG_LV_STS);		
	*pu32Period = u32RegData & PERIOD >> 16;
    	*pu32Attack = u32RegData & ATTACK >> 8;
    	*pu32Recovery = u32RegData & RECOVERY >> 4;
    	*pu32Hold = u32RegData & HOLD; 								     				     	
}   


/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_SetTouchScreen()	                      		                                               
                                                                                                         
 DESCRIPTION                                                                                             
     	Set the parameter for TSC														                   
                                                                                                         
 INPUTS                                                                                                  
      eTscMode    	Normal mode, Semi-Auto, Auto or Wait for trigger   					               
      eTscWire 		4 wire, 5 wire, 8 wire or unused   					           					   
      bIsPullup		Control the internal pull up PMOS in switch box									   
      bMAVFilter      Enable or disable MAV filter in TSC auto mode                                      
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      none																				               
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_SetTouchScreen(
	E_DRVADC_TSC_MODE eTscMode,
	E_DRVADC_TSC_TYPE eTscWire,
	BOOL bIsPullup,
	BOOL bMAVFilter	
	)
{  			
	ENTER();
	
    	outp32(REG_ADC_CON, (inp32(REG_ADC_CON) & ~(ADC_TSC_MODE)) | 	
    				(eTscMode << 14) );
    	outp32(REG_ADC_TSC, (inp32(REG_ADC_TSC) & ~(ADC_TSC_TYPE | ADC_PU_EN | ADC_TSC_MAV_EN)) | 	
    				( (((eTscWire << 1) & ADC_TSC_TYPE) | ((bIsPullup <<3) & ADC_PU_EN)) |
    				((bMAVFilter<<9) & ADC_TSC_MAV_EN) ));				
}    
void DrvADC_GetTouchScreen(
	E_DRVADC_TSC_MODE* peTscMode,
	E_DRVADC_TSC_TYPE* peTscWire,
	PBOOL pbIsPullup,
	PBOOL pbMAVFilter	
	)
{  		
	UINT32 u32RegData = inp32(REG_ADC_TSC);		
	ENTER();

	*peTscMode = (inp32(REG_ADC_CON) & ADC_TSC_MODE) >> 14;
    	*peTscWire = (u32RegData & ADC_TSC_TYPE) >> 1;
    	*pbIsPullup = (u32RegData & ADC_PU_EN) >> 3;
    	*pbMAVFilter = (u32RegData & ADC_TSC_MAV_EN) >> 9;						     				     											     											     											     				
}    

/*---------------------------------------------------------------------------------------------------------
                                                                                                         
 FUNCTION                                                                                                
 		DrvADC_GetMovingAverage()	                  		                                               
                                                                                                         
 DESCRIPTION                                                                                             
     	Get the moving average for TSC if MAV filter enable								                   
                                                                                                         
 INPUTS                                                                                                  
      pu32AverageX    10 bit moving average for TSC auto mode if MAV enable				               
      pu32AverageY 	10 bit moving average for TSC auto mode if MAV enable  							   
                                                                                                         
 OUTPUTS                                                                                                 
      none 														                                       
                                                                                                         
 RETURN                                                                                                  
      none																				               
                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvADC_GetMovingAverage(
	PUINT16 	pu16AverageX,
	PUINT16 	pu16AverageY
	)
{  		
	ENTER();

	*pu16AverageX = inp32(REG_TSC_MAV_X) & X_MAV_AVG;
	*pu16AverageY = inp32(REG_TSC_MAV_Y) & Y_MAV_AVG;			
}   

void DrvADC_GetMovingData(
	PUINT16 	pu16ArrayX,
	PUINT16 	pu16ArrayY
	)
{  		
	UINT32 u32Idx, u32RegData;
	ENTER();


	for(u32Idx=0;u32Idx<10;u32Idx=u32Idx+1)
	{
		u32RegData = inp32(REG_TSC_SORT10+u32Idx*4);
		*pu16ArrayX++ =  u32RegData & X_MAV;
		*pu16ArrayY++ =  (u32RegData & Y_MAV) >>16;										     				
	}	
}                                     

BOOL 
DrvADC_GetTouchScreenUpDownState(void)
{
	return (inp32(REG_ADC_TSC)&ADC_UD);
}

void DrvADC_GetTscData(
	PUINT16 	pu16XData,
	PUINT16 	pu16YData
	)
{  		
	ENTER();

	*pu16XData = inp32(REG_ADC_XDATA);
	*pu16YData = inp32(REG_ADC_YDATA);					     				     											     											     											     				
}
