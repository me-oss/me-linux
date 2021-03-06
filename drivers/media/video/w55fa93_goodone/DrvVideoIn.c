#include <asm/io.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/videoin.h>
#include <asm/arch/DrvVideoin.h>

#define ERR_PRINTF			printk
#define outp32(addr, value)		outl(value, addr)
#define inp32(addr)			inl(addr)
#define DBG_PRINTF(...)			
//#define DBG_PRINTF			printk 
	
extern unsigned int w55fa93_upll_clock, w55fa93_apll_clock, w55fa93_ahb_clock;

/*========================================================================
	VideoIn Driver Start 
	Remember mknod video0 c 81 0
========================================================================*/
__u32 g_u32DeviceType;
void DrvVideoIn_Init(
	BOOL bIsEnableSnrClock,
	E_DRVVIDEOIN_SNR_SRC eSnrSrc,	
	UINT32 u32SensorFreq,						//KHz unit
	E_DRVVIDEOIN_DEV_TYPE eDevType
	)
{
	INT32 i32Idx0, i32Idx1;
	UINT32 u32PllClock, u32SnrDiv, u32SenSrc, u32SenDiv;
	UINT32 u32Div0, u32Div1;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	DBG_PRINTF("0x%x\n",inp32(REG_AHBCLK));		

	g_u32DeviceType = eDevType;

	u32PllClock = w55fa93_upll_clock;//sysGetPLLOutputKhz(eSYS_UPLL, u32ExtFreq);
	u32SenSrc = 0x18<<16;   
	if(u32PllClock==0) 
	{ 
		u32PllClock = w55fa93_apll_clock;//sysGetPLLOutputKhz(eSYS_APLL, u32ExtFreq);      
		u32SenSrc = 0x10<<16;      
	}   
	DBG_PRINTF("Sensor clock = %d KHz\n", u32SensorFreq);
	u32SenDiv = u32PllClock/u32SensorFreq;    
	DBG_PRINTF("Sensor divider = %d\n", u32SenDiv);
	if(u32PllClock%u32SensorFreq!=0)   
		u32SenDiv = u32SenDiv+1;      
	for(u32Div0=2; u32Div0<=8; u32Div0 = u32Div0+1)	   
	{//u32Div0 should be start from 2   
		for(u32Div1=1; u32Div1<=16; u32Div1 = u32Div1+1)	         
		{      
			if(u32SenDiv==u32Div0*u32Div1)         
				break;            
		}      
		if(u32SenDiv==u32Div0*u32Div1)      
				break;            
	}	     
	u32Div0 = u32Div0-1; 
	u32Div1 = u32Div1-1; 
	DBG_PRINTF("Sensor divider 0= %d\n", u32Div0 );
	DBG_PRINTF("Sensor divider 1= %d\n", u32Div1);
	outp32(REG_CLKDIV0, (inp32(REG_CLKDIV0) & ~(SENSOR_N1 | SENSOR_S | SENSOR_N0)) | 
				 (u32SenSrc | ((u32Div0<<16) | (u32Div1<<24)))  );   
	DBG_PRINTF("Register CLKDIV0 content= 0x%x\n", inp32(REG_CLKDIV0));
	
	if(bIsEnableSnrClock)	
		outp32(REG_AHBCLK, inp32(REG_AHBCLK) | SEN_CKE );						/* Enable Sensor clock */
	else
		outp32(REG_AHBCLK, inp32(REG_AHBCLK) & ~SEN_CKE );						/* Disable Sensor clock */	
	if(eDevType == eDrvVideoIn_SNR_CCIR656)
	{//VSYNC and HSYN by data
	#if 0
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) | ( MF_GPB0 | MF_GPB1 | 						//SCLKO, PCLK
										MF_GPB5 | MF_GPB6 | MF_GPB7 | MF_GPB8 | 	//VDATA[7:0]
										MF_GPB9 | MF_GPB10 | MF_GPB11 | MF_GPB12) );	
	#else
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) | 0x03FFFC0F );									
	#endif																		   								   
	}
	else if(eDevType == eDrvVideoIn_SNR_CCIR601)
	{//VSYNC and HSYN by pins
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) | ( MF_GPB0 | MF_GPB1 | MF_GPB2 | MF_GPB3 |	//SCLKO, PCLK, HSYNC and VSYNC
										MF_GPB5 | MF_GPB6 | MF_GPB7 | MF_GPB8 | 	//VDATA[7:0]
										MF_GPB9 | MF_GPB10 | MF_GPB11 | MF_GPB12) );										
	}
	else if(eDevType == eDrvVideoIn_TVD_CCIR601)
	{
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) | ( MF_GPB0 | MF_GPB1 | MF_GPB2 | MF_GPB3 | MF_GPB4 |	//SCLKO, PCLK, HSYNC, VSYNC and SFILED
										MF_GPB5 | MF_GPB6 | MF_GPB7 | MF_GPB8 | 	//VDATA[7:0]
										MF_GPB9 | MF_GPB10 | MF_GPB11 | MF_GPB12) );								  									
	}
	else if(eDevType == eDrvVideoIn_TVD_CCIR656)
	{
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) | (  MF_GPB1 |					//PCLK
										MF_GPB5 | MF_GPB6 | MF_GPB7 | MF_GPB8 | 	//VDATA[7:0]
										MF_GPB9 | MF_GPB10 | MF_GPB11 | MF_GPB12) );												
	}
	else if(eDevType == eDrvVideoIn_2nd_SNR_CCIR601)
	{
	  	outp32(REG_GPBFUN, inp32(REG_GPBFUN) & 0xFC00000F);	//The 1st sensor must other function(GPIO or other) if enable 2nd sensor
		outp32(REG_GPCFUN, (inp32(REG_GPCFUN) & ~(MF_GPC15 | MF_GPC14 | MF_GPC13 | MF_GPC12 |
										  MF_GPC11 | MF_GPC10 | MF_GPC9 | MF_GPC8)) 
										  | (0x5555<<16));	//Data
	#if 0 
		//H and VSYNC from GPE1 and GPE0 										  
		outp32(REG_GPEFUN, (inp32(REG_GPEFUN) & ~(MF_GPE1 | MF_GPE0))
										  | 0x5);  			//Sync
	#else
		//H and VSYNC from GPB2 and GPE3  
		outp32(REG_GPBFUN, (inp32(REG_GPBFUN) | 0xF0));	
	#endif										  										  								 										  
		outp32(REG_GPBFUN, (inp32(REG_GPBFUN) | 0xF));
										    			//Clk in/out
	}					
}
/*---------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_Open 									                       
//                                                                                                         					
// Parameters:                                                                                             
//      u32EngFreqKHz : Engine working Freq                                                               
//      u32SensorFreq : Sensor working Freq                                                                
// Returns:                                                                                                
//      None                                                                                               
//                                                                                                         
// Description:                                                                                            
//     	Initialize the VIdeo-In engine. Register a call back for driver internal using   
//                                                                                                      
---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvVideoIn_Open(
	UINT32 u32EngFreqKHz, 								/* VideoIn eng working frequency */  
	UINT32 u32SensorFreq									/* Specify the sensor clock */ 
)
{

	UINT32 u32PllClock, u32AHB234; 
	UINT32 u32EngDiv;							
	DBG_PRINTF("%s\n",__FUNCTION__);		
	DBG_PRINTF("0x%x\n",inp32(REG_AHBCLK));							   									
	outp32(REG_AHBCLK, inp32(REG_AHBCLK) | (CAP_CKE | HCLK4_CKE));				/* Enable Cap clock */
	outp32(REG_AHBIPRST, inp32(REG_AHBIPRST) | CAPRST );							/* Global reset Capture */
	outp32(REG_AHBIPRST, inp32(REG_AHBIPRST) & (~CAPRST) );								


	u32AHB234 = w55fa93_ahb_clock;//DrvSYS_GetAHB234Clock();
	u32EngDiv = (u32AHB234 / u32EngFreqKHz)-1;

	outl( (inl(REG_CLKDIV4) & ~CAP_N) |
					(u32EngDiv<<12), REG_CLKDIV4);		
	
/*			
	sysInstallISR(IRQ_LEVEL_1, 
						IRQ_VIDEOIN, 
						(PVOID)VideoInIntHandler);
	sysEnableInterrupt(IRQ_VIDEOIN);	
*/
	return Successful;
} // DrvVideoIn_Open
/*---------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_Close 									                                          
//                                                                                                         
// Parameters:                                                                                             
//      Nonre: 													                                           
//      u32SensorFreq : Sensor working Freq                                                                
// Returns:                                                                                                
//      None                                                                                               
//                                                                                                         
// Description:                                                                                            
//     	Disable pin function,engine clock and interruot                 			                       
//                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void
DrvVideoIn_Close(
	void
	)
{
	// 1. Disable IP's interrupt
	//sysDisableInterrupt(IRQ_VIDEOIN);	
	// 2. Disable IP’s clock
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_AHBCLK, inp32(REG_AHBCLK) & ~CAP_CKE);						
	// 3. Disable Capture pin function 
	if(g_u32DeviceType == eDrvVideoIn_SNR_CCIR656)
	{//VSYNC and HSYN by data
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) & ~( MF_GPB0 | MF_GPB1 | 						//SCLKO, PCLK
										MF_GPB5 | MF_GPB6 | MF_GPB7 | MF_GPB8 | 	//VDATA[7:0]
										MF_GPB9 | MF_GPB10 | MF_GPB11 | MF_GPB12) );		
																   										   		
	}								   
	else if(g_u32DeviceType == eDrvVideoIn_SNR_CCIR601)
	{//VSYNC and HSYN by pins
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) & ~( MF_GPB0 | MF_GPB1 | MF_GPB2 | MF_GPB3 |	//SCLKO, PCLK, HSYNC and VSYNC
										MF_GPB5 | MF_GPB6 | MF_GPB7 | MF_GPB8 | 	//VDATA[7:0]
										MF_GPB9 | MF_GPB10 | MF_GPB11 | MF_GPB12) );	
	}
	else if(g_u32DeviceType == eDrvVideoIn_TVD_CCIR601)
	{
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) & ~( MF_GPB0 | MF_GPB1 | MF_GPB2 | MF_GPB3 | MF_GPB4 |	//SCLKO, PCLK, HSYNC, VSYNC and SFILED
										MF_GPB5 | MF_GPB6 | MF_GPB7 | MF_GPB8 | 	//VDATA[7:0]
										MF_GPB9 | MF_GPB10 | MF_GPB11 | MF_GPB12) );									   					
	}
	else if(g_u32DeviceType == eDrvVideoIn_TVD_CCIR656)
	{
		outp32(REG_GPBFUN, inp32(REG_GPBFUN) & ~( MF_GPB1 | 	//SCLKO and PCLK 
										MF_GPB5 | MF_GPB6 | MF_GPB7 | MF_GPB8 | 	//VDATA[7:0]
										MF_GPB9 | MF_GPB10 | MF_GPB11 |  MF_GPB12) );				   
	}
	else if(g_u32DeviceType == eDrvVideoIn_2nd_SNR_CCIR601)
	{
	  	outp32(REG_GPBFUN, inp32(REG_GPBFUN) & 0xFC00000F);	  	//Disable 1st sensor interface	  
		outp32(REG_GPCFUN, (inp32(REG_GPCFUN) & (~0xFFFF0000)) );		
		outp32(REG_GPEFUN, (inp32(REG_GPEFUN) & (~0xF)) );			//Disable 2nd sensor interface			  
		outp32(REG_GPBFUN, (inp32(REG_GPBFUN) & (~0xF)));													    			
	}					   							   	
} // DrvVideoIn_Close

UINT32 uCapIntCallback = eDRVVIDEOIN_VINT;
/*---------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_SetPacketFrameBufferControl 									                   
//                                                                                                         
// Parameters:                                                                                             
//      bFrameSwitch: Software mode buffer select  				                                           
//					0: Packet buffer 0																	   
//					1: Packet buffer 1																	   
//      bFrameBufferSel :   Buffer control by FSC IP                                                       
// Returns:                                                                                                
//      None                                                                                               
//                                                                                                         
// Description:                                                                                            
//     	Disable pin function,engine clock and interruot                 			                      
//                                                                                                         
---------------------------------------------------------------------------------------------------------*/
void DrvVideoIn_SetPacketFrameBufferControl(
	BOOL bFrameSwitch,
	BOOL bFrameBufferSel
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	UINT32 u32Ctl = inp32(REG_VPECTL) & ~(ADDRSW | FBMODE);
	outp32(REG_VPECTL, u32Ctl |
					((bFrameBufferSel?FBMODE:0) | 	
					(bFrameSwitch?ADDRSW:0)));	
}

void 
DrvVideoIn_GetPacketFrameBufferControl(
	PBOOL pbFrameSwitch,
	PBOOL pbFrameBufferSel
	)
{
	UINT32 u32Ctl = inp32(REG_VPECTL);
	*pbFrameBufferSel = (u32Ctl & FBMODE) >> 2;
	*pbFrameSwitch = (u32Ctl & ADDRSW) >> 3;
}
/*---------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_InstallCallback			  									                       
//                                                                                                         
// Parameters:                                                                                             
//      eIntType: Interrupt type				  				                                           
//      pvIsr: Call back fucntion					                                                       
// Returns:                                                                                                
//      None                                                                                               
//                                                                                                         
// Description:                                                                                            
//     	Disable pin function,engine clock and interrupt                 			                       
//                                                                                                         
---------------------------------------------------------------------------------------------------------*/
/*
ERRCODE 
DrvVideoIn_InstallCallback(
	E_DRVVIDEOIN_INT_TYPE eIntType, 
	PFN_DRVVIDEOIN_CALLBACK pfnCallback,
	PFN_DRVVIDEOIN_CALLBACK *pfnOldCallback
	)
{
	if(eIntType == eDRVVIDEOIN_VINT)
	{
		*pfnOldCallback = pfnVideoInIntHandlerTable[0];
		pfnVideoInIntHandlerTable[0] = (PFN_DRVVIDEOIN_CALLBACK)(pfnCallback);
	}	
	else if(eIntType == eDRVVIDEOIN_ADDRMINT)
	{
		*pfnOldCallback = pfnVideoInIntHandlerTable[1];
		pfnVideoInIntHandlerTable[1] = (PFN_DRVVIDEOIN_CALLBACK)(pfnCallback);
	}
	else if(eIntType == eDRVVIDEOIN_MEINT)
	{
		*pfnOldCallback = pfnVideoInIntHandlerTable[2];
		pfnVideoInIntHandlerTable[2] = (PFN_DRVVIDEOIN_CALLBACK)(pfnCallback);
	}	
	else if(eIntType == eDRVVIDEOIN_MDINT)
	{
		*pfnOldCallback = pfnVideoInIntHandlerTable[3];
		pfnVideoInIntHandlerTable[3] = (PFN_DRVVIDEOIN_CALLBACK)(pfnCallback);
	}	
	else
		return E_DRVVIDEOIN_INVALID_INT;			
	return Successful;	
}
*/


void
DrvVideoIn_Reset(
	void
	)
{	
	outp32(REG_VPECTL, inp32(REG_VPECTL) | VPRST);			
	outp32(REG_VPECTL, inp32(REG_VPECTL) & (~VPRST));			
} // DrvVideoIn_Reset

ERRCODE 
DrvVideoIn_EnableInt(
	E_DRVVIDEOIN_INT_TYPE eIntType
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	switch(eIntType)
	{
		case eDRVVIDEOIN_MDINT:
		case eDRVVIDEOIN_ADDRMINT:							
		case eDRVVIDEOIN_MEINT:				
		case eDRVVIDEOIN_VINT:				
				outp32(REG_VPEINT, inp32(REG_VPEINT) | eIntType);
				break;		
		default: 				 		
			return E_DRVVIDEOIN_INVALID_INT;
	}
	return Successful;
} // DrvVideoIn_EnableInt

ERRCODE
DrvVideoIn_DisableInt(
	E_DRVVIDEOIN_INT_TYPE eIntType
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	switch(eIntType)
	{
		case eDRVVIDEOIN_MDINT:
		case eDRVVIDEOIN_ADDRMINT:							
		case eDRVVIDEOIN_MEINT:				
		case eDRVVIDEOIN_VINT:				
				outp32(REG_VPEINT, inp32(REG_VPEINT) & ~eIntType );
				break;		
		default: 				 		
			return E_DRVVIDEOIN_INVALID_INT;
	}
	return Successful;
} // DrvVideoIn_DisableInt

BOOL
DrvVideoIn_IsIntEnabled(
	E_DRVVIDEOIN_INT_TYPE eIntType
	)
{	
	UINT32 u32IntEnable = inp32(REG_VPEINT);	
	DBG_PRINTF("%s\n",__FUNCTION__);	
	switch(eIntType)
	{
		case eDRVVIDEOIN_MDINT:
				u32IntEnable = u32IntEnable & eDRVVIDEOIN_MDINT;
				break;		
		case eDRVVIDEOIN_ADDRMINT:	
				u32IntEnable = u32IntEnable & eDRVVIDEOIN_ADDRMINT;
				break;					
		case eDRVVIDEOIN_MEINT:			
				u32IntEnable = u32IntEnable & eDRVVIDEOIN_MEINT;
				break;	
		case eDRVVIDEOIN_VINT:				
				u32IntEnable = u32IntEnable & eDRVVIDEOIN_VINT;
				break;		
	}
	return (u32IntEnable?TRUE:FALSE);	
} // DrvVideoIn_IsIntEnabled
/*---------------------------------------------------------------------------------------------------------
//
//	Clear the interrupt status. //Write one clear 
//
//
//
//
//
//
//
//
//
//
---------------------------------------------------------------------------------------------------------*/
ERRCODE
DrvVideoIn_ClearInt(
	E_DRVVIDEOIN_INT_TYPE eIntType
	)
{
	UINT32 u32IntChannel = eIntType >>16;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	switch(eIntType)
	{
		case eDRVVIDEOIN_MDINT:
				outp32(REG_VPEINT, (inp32(REG_VPEINT) & ~((eDRVVIDEOIN_ADDRMINT | eDRVVIDEOIN_MEINT | eDRVVIDEOIN_VINT)>>16)) | 
								u32IntChannel);
				break;					
		case eDRVVIDEOIN_ADDRMINT:		
				outp32(REG_VPEINT, (inp32(REG_VPEINT) & ~((eDRVVIDEOIN_MDINT | eDRVVIDEOIN_MEINT | eDRVVIDEOIN_VINT)>>16)) | 
								u32IntChannel);
				break;					
		case eDRVVIDEOIN_MEINT:
				outp32(REG_VPEINT, (inp32(REG_VPEINT) & ~((eDRVVIDEOIN_MDINT | eDRVVIDEOIN_ADDRMINT | eDRVVIDEOIN_VINT)>>16)) | 
								u32IntChannel);
				break;					
		case eDRVVIDEOIN_VINT:				
				outp32(REG_VPEINT, (inp32(REG_VPEINT) & ~((eDRVVIDEOIN_MDINT | eDRVVIDEOIN_MEINT | eDRVVIDEOIN_ADDRMINT)>>16)) | 
								u32IntChannel);
				break;	
		default: 				 		
			return E_DRVVIDEOIN_INVALID_INT;
	}
	return Successful;


} // DrvVideoIn_ClearInt

/*---------------------------------------------------------------------------------------------------------
//
//	Polling the interrupt status 
//
//
//	return value could be 
//	
//	DRVVIDEOIN_ADDRMINT | DRVVIDEOIN_MEINT | DRVVIDEOIN_VINT
//
//
//
//
//
---------------------------------------------------------------------------------------------------------*/
BOOL
DrvVideoIn_PollInt(
	E_DRVVIDEOIN_INT_TYPE eIntType
	)
{
	UINT32 u32IntStatus = inp32(REG_VPEINT);	
	DBG_PRINTF("%s\n",__FUNCTION__);	
	switch(eIntType)
	{
		case eDRVVIDEOIN_MDINT:
				u32IntStatus = u32IntStatus & (eDRVVIDEOIN_MDINT>>16);
				break;		
		case eDRVVIDEOIN_ADDRMINT:	
				u32IntStatus = u32IntStatus & (eDRVVIDEOIN_ADDRMINT>>16);
				break;					
		case eDRVVIDEOIN_MEINT:			
				u32IntStatus = u32IntStatus & (eDRVVIDEOIN_MEINT>>16);
				break;	
		case eDRVVIDEOIN_VINT:				
				u32IntStatus = u32IntStatus & (eDRVVIDEOIN_VINT>>16);
				break;		
	}
	return (u32IntStatus?TRUE:FALSE);
} // DrvVideoIn_PollInt

/*---------------------------------------------------------------------------------------------------------
//
//	Enable engine and turn on the pipe.
//
//
//
//
//
//
//	u32PipeEnable = 0. Both pipe disable.
//					1. Packet pipe enable.	
//					2. Planar pipe enable.
//					3. Both pipe enable.
---------------------------------------------------------------------------------------------------------*/
void DrvVideoIn_SetPipeEnable(
	BOOL bEngEnable,     				// TRUE: Enable, FALSE: Disable
	E_DRVVIDEOIN_PIPE ePipeEnable    
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPECTL, (inp32(REG_VPECTL) & ~(VPEEN | PKEN | PNEN)) 
    | (((bEngEnable ? VPEEN : 0x0))
    // | ((ePipeEnable & ~(PKEN | PNEN))<<5)) );
    | ((ePipeEnable & 0x03)<<5)) );
} // DrvVideoIn_SetPipeEnable

void DrvVideoIn_GetPipeEnable(
	PBOOL pbEngEnable,      			// TRUE: Enable, FALSE: Disable
	E_DRVVIDEOIN_PIPE* pePipeEnable     // TRUE: Enable, FALSE: Disable
	)
{
	UINT32 u32Temp = inp32(REG_VPECTL);
	
	*pbEngEnable = (u32Temp & VPEEN) ? TRUE : FALSE;
	*pePipeEnable = (u32Temp & (PKEN | PNEN))>>5;
} // DrvVideoIn_GetPipeEnable


void 
DrvVideoIn_SetShadowRegister(
	void
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPECTL,  inp32(REG_VPECTL) | UPDATE);
} // DrvVideoIn_SetShadowRegister

void 
DrvVideoIn_SetSensorPolarity(
	BOOL bVsync,       					// TRUE: High Active, FALSE: Low Active
	BOOL bHsync,       					// TRUE: High Active, FALSE: Low Active
	BOOL bPixelClk     					// TRUE: Falling Edge, FALSE: Rising Edge
	)
{
	outp32(REG_VPEPAR, (inp32(REG_VPEPAR) & ~(VSP| HSP | PCLKP)) 
    | (((bVsync ? VSP : 0x0)
    | (bHsync ? HSP : 0x0))
    | (bPixelClk ? PCLKP : 0x0)) );
} // DrvVideoIn_SetSensorPolarity

void 
DrvVideoIn_GetSensorPolarity(
	PBOOL pbVsync,      				// TRUE: High Active, FALSE: Low Active
	PBOOL pbHsync,      				// TRUE: High Active, FALSE: Low Active
	PBOOL pbPixelClk    				// TRUE: Falling Edge, FALSE: Rising Edge
	)
{
	UINT32 u32Temp = inp32(REG_VPEPAR);
	DBG_PRINTF("%s\n",__FUNCTION__);	
	*pbVsync = (u32Temp & VSP)  ? TRUE : FALSE;
	*pbHsync = (u32Temp & HSP) ? TRUE : FALSE;
	*pbPixelClk = (u32Temp & PCLKP) ? TRUE : FALSE;
} // DrvVideoIn_GetSensorPolarity
/*---------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_InstallInterrupt			  									                   
//                                                                                                         
// Parameters:                                                                                            
//     eInputOrder: Data order for input format 														   
//		00 = Y0 U0 Y1 V0																			   
//		01 = Y0 V0 Y1 U0																			  
//		10 = U0 Y0 V0 Y1																			
//		11 = V0 Y0 U0 Y1    	  				                                          			   
//     eInputFormat: 																					
//		0 = YUV.	1=RGB			 					                                            
//		eOutputFormat: 																				   
//		00=YCbCr422.	01=only output Y.   10=RGB555	11=RGB565									
//Returns:                                                                                                
//     None                                                                                               
//                                                                                                        
//Description:                                                                                         
//    	Set the input format, input order and output format               			                      
//                                                                                                       
//--------------------------------------------------------------------------------------------------------*/
void 
DrvVideoIn_SetDataFormatAndOrder(
	E_DRVVIDEOIN_ORDER eInputOrder, 
	E_DRVVIDEOIN_IN_FORMAT eInputFormat, 
	E_DRVVIDEOIN_OUT_FORMAT eOutputFormat
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPEPAR, (inp32(REG_VPEPAR) & ~(OUTFMT | PDORD | INFMT)) 
    | ((((eInputOrder << 2) & PDORD)
    | (eInputFormat & INFMT))
    | ((eOutputFormat <<4 ) & OUTFMT)) );
} // DrvVideoIn_SetDataFormatAndOrder

void DrvVideoIn_GetDataFormatAndOrder(
	E_DRVVIDEOIN_ORDER* peInputOrder, 
	E_DRVVIDEOIN_IN_FORMAT* peInputFormat, 
	E_DRVVIDEOIN_OUT_FORMAT* peOutputFormat
	)
{
	UINT32 u32Temp = inp32(REG_VPEPAR);
	DBG_PRINTF("%s\n",__FUNCTION__);	
	*peInputOrder = (u32Temp & PDORD) >> 2;
	*peInputFormat = u32Temp & INFMT;
	*peOutputFormat = (u32Temp & OUTFMT) >> 4;
} // DrvVideoIn_GetDataFormatAndOrder

/*---------------------------------------------------------------------------------------------------------
//Function: DrvVideoIn_SetPlanarFormat			  									                	   	
//                                                                                                        
//Parameters:                                                                                             	
//     bIsYUV420: Planar Format																		   	
//		FALSE : Planar YUV422																		   	
//		TRUE : Planar YUV420																		   	
//Returns:                                                                                                	
//     None                                                                                               	
//                                                                                                        	
//Description:                                                                                            	
//    	Set the planar output format, 						               			                  
//                                                                                                        
//---------------------------------------------------------------------------------------------------------*/
void DrvVideoIn_SetPlanarFormat(
	BOOL bIsYUV420
)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPEPAR, (inp32(REG_VPEPAR) & ~PNFMT) | 
					(bIsYUV420 ? PNFMT : 0));
}

BOOL 
DrvVideoIn_GetPlanarFormat(void)
{
	return ((inp32(REG_VPEPAR) & PNFMT)>>7);
}


/*----------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_SetMotionDet			  									                	   	
//                                                                                                         	
// Parameters:                                                                                             	
//      bEnable: Enable Motion Detection																	
//			FALSE : Disable MD																			   	
//			TRUE : Enable MD																			   	
//      bBlockSize: Motion Detection Block Size																 
//			FALSE : 16x16																				   	
//			TRUE : 8x8																					   	
//      bSaveMode: Motion Detection Save Mode  																
//			FALSE : 1 bit DIFF + 7 Y Differential														
//			TRUE : 	1 bit DIFF only 													   				
// Returns:                                                                                                
//      None                                                                                               
//                                                                                                         	
// Description:                                                                                            
//     	Set the motion detection parameter					               			                       
//                                                                                                         	
//----------------------------------------------------------------------------------------------------------*/	
void DrvVideoIn_SetMotionDet(
	BOOL bEnable,
	BOOL bBlockSize,	
	BOOL bSaveMode
)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPEMD, (inp32(REG_VPEMD) & ~(MDSM | MDBS | MDEN)) |
			(((bEnable?MDEN:0) | (bBlockSize?MDBS:0)) | 
				(bSaveMode?MDSM:0)));	
}
	
void DrvVideoIn_GetMotionDet(
	PBOOL pbEnable,
	PBOOL pbBlockSize,	
	PBOOL pbSaveMode
)
{
	UINT32 u32RegData = inp32(REG_VPEMD);
	*pbEnable = (u32RegData & MDEN);
	*pbBlockSize = (u32RegData & MDBS)>>8; 
	*pbSaveMode = (u32RegData & MDSM)>>9; 		
}
/*----------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_SetMotionDetEx			  									                	   
//                                                                                                         	
// Parameters:                                                                                             	
//      u32DetFreq: MD detect frequency	(0~3)															
//			0 : Each frame  																			   	
//			1 :	Every 2 frame																	   			
//			2 : every 3 frame																				
//			3 : every 4 frame																				
//     u32Threshold: Motion detection threshold (0~31)														
//			(Value << 1) : Differential threshold 															
//			Value 16 meaning threshold = 32																   	
//      u32OutBuffer: Output buffer addree		  															
// Returns:                                                                                                	
//      None                                                                                               	
//                                                                                                         	
// Description:                                                                                            	
//     	Set the motion detection parameter extention 					               			                       	
//                                                                                                         
//----------------------------------------------------------------------------------------------------------*/			
void DrvVideoIn_SetMotionDetEx(
	UINT32 u32DetFreq,
	UINT32 u32Threshold,
	UINT32 u32OutBuffer,
	UINT32 u32LumBuffer
)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPEMD, (inp32(REG_VPEMD) & ~(MDTHR | MDDF)) |
				(((u32DetFreq <<10) & MDDF) |	
				((u32Threshold <<16) & MDTHR)) );
	outp32(REG_MDADDR, u32OutBuffer);	
	outp32(REG_MDYADDR, u32LumBuffer);		
}
void DrvVideoIn_GetMotionDetEx(
	PUINT32 pu32DetFreq,
	PUINT32 pu32Threshold,
	PUINT32 pu32OutBuffer,
	PUINT32 pu32LumBuffer
)
{
	UINT32 u32RegData;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	u32RegData = inp32(REG_VPEMD); 
	*pu32DetFreq = u32RegData & MDDF;
	*pu32Threshold = u32RegData & MDTHR;
	*pu32OutBuffer = inp32(REG_MDADDR);
	*pu32LumBuffer = inp32(REG_MDYADDR);
}	
//===================================================================================
//
//	One shutte or continuous mode
//
//
//
//
//
//
//	bIsOneSutterMode =  0. Continuous mode 
//		     			1. One shutte mode
//===================================================================================
void 
DrvVideoIn_SetOperationMode(
	BOOL bIsOneSutterMode
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPECTL, (inp32(REG_VPECTL) & ~CAPONE) | 
			((bIsOneSutterMode <<16) & CAPONE) );
} // DrvVideoIn_SetOperationMode

BOOL 
DrvVideoIn_GetOperationMode(
	void
	)
{
	//DBG_PRINTF("%s\n",__FUNCTION__);	
	return ( (inp32(REG_VPECTL) & CAPONE) ? TRUE : FALSE );
} // DrvVideoIn_GetOperationMode


UINT32 
DrvVideoIn_GetProcessedDataCount(    // Read Only
	E_DRVVIDEOIN_PIPE ePipe	 		 // Planar or packet pipe		
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	if(ePipe == eDRVVIDEOIN_PACKET)
		return inp32(REG_CURADDRP);				/* Packet pipe */
	else if (ePipe == eDRVVIDEOIN_PLANAR)
		return inp32(REG_CURADDRY);				/* Planar pipe */
	else 
		return 0;		
} // DrvVideoIn_GetProcessedDataCount


void 
DrvVideoIn_SetCropWinStartAddr(
	UINT32 u32VerticalStart, 
	UINT32 u32HorizontalStart
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPECWSP, (inp32(REG_VPECWSP) & ~(CWSPV | CWSPH)) //(Y|X)
    | ((u32VerticalStart << 16)
    | u32HorizontalStart));
} // DrvVideoIn_SetCropWinStartAddr


void 
DrvVideoIn_GetCropWinStartAddr(
	PUINT32 pu32VerticalStart, 
	PUINT32 pu32HorizontalStart
	)
{
	UINT32 u32Temp = inp32(REG_VPECWSP);
	DBG_PRINTF("%s\n",__FUNCTION__);	
	*pu32VerticalStart = (u32Temp & CWSPV) >> 16;
	*pu32HorizontalStart = u32Temp & CWSPH;
} // DrvVideoIn_GetCropWinStartAddr

void 
DrvVideoIn_SetCropWinSize(
	UINT32 u32Height, 
	UINT32 u32Width
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPECWS, (inp32(REG_VPECWS) & ~(CWSH | CWSW)) 
    | ((u32Height << 16)
    | u32Width));
} // DrvVideoIn_SetCropWinSize


void 
DrvVideoIn_GetCropWinSize(
	PUINT32 pu32Height, 
	PUINT32 pu32Width
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	UINT32 u32Temp = inp32(REG_VPECWS);
	
	*pu32Height = (u32Temp & CWSH) >> 16;
	*pu32Width = u32Temp & CWSW;
} // DrvVideoIn_GetCropWinSize

ERRCODE 
DrvVideoIn_SetVerticalScaleFactor(
	E_DRVVIDEOIN_PIPE ePipe,
	UINT8 u8Numerator, 
	UINT8 u8Denominator
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	if(ePipe == eDRVVIDEOIN_PACKET)
	{
		outp32(REG_VPEPKDS, (inp32(REG_VPEPKDS) & ~(DSVN | DSVM)) 
    	| ((u8Numerator << 24)
    	| (u8Denominator << 16)));
    }	
    else if(ePipe == eDRVVIDEOIN_PLANAR)
    {
    	outp32(REG_VPEPNDS, (inp32(REG_VPEPNDS) & ~(DSVN | DSVM)) 
    	| ((u8Numerator << 24)
    	| (u8Denominator << 16)));	
    }
    else
		return E_DRVVIDEOIN_INVALID_PIPE;
	return Successful;	
} // DrvVideoIn_SetVerticalScaleFactor

ERRCODE 
DrvVideoIn_GetVerticalScaleFactor(
	E_DRVVIDEOIN_PIPE ePipe,
	PUINT8 pu8Numerator, 
	PUINT8 pu8Denominator
	)
{
	UINT32 u32Temp; 
	DBG_PRINTF("%s\n",__FUNCTION__);	
	if(ePipe == eDRVVIDEOIN_PACKET)
		u32Temp = inp32(REG_VPEPKDS);
	else if(ePipe == eDRVVIDEOIN_PLANAR)
		u32Temp = inp32(REG_VPEPNDS);
	else
		return E_DRVVIDEOIN_INVALID_PIPE;	
	*pu8Numerator = (u32Temp & DSVN) >> 24;
	*pu8Denominator = (u32Temp & DSVM) >> 16;		
	return Successful;
} // DrvVideoIn_GetVerticalScaleFactor

ERRCODE 
DrvVideoIn_SetHorizontalScaleFactor(
	E_DRVVIDEOIN_PIPE bPipe,
	UINT8 u8Numerator, 
	UINT8 u8Denominator
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	if(bPipe == eDRVVIDEOIN_PACKET)
	{
		outp32(REG_VPEPKDS, (inp32(REG_VPEPKDS) & ~(DSHN | DSHM)) 
	    | ((u8Numerator << 8)
	    | u8Denominator));
	}
	else  if(bPipe == eDRVVIDEOIN_PLANAR)
	{
		outp32(REG_VPEPNDS, (inp32(REG_VPEPNDS) & ~(DSHN | DSHM)) 
	    | ((u8Numerator << 8)
	    | u8Denominator));
	}	    
	else
		return E_DRVVIDEOIN_INVALID_PIPE;	
	return Successful;
} // DrvVideoIn_SetHorizontalScaleFactor

ERRCODE 
DrvVideoIn_GetHorizontalScaleFactor(
	E_DRVVIDEOIN_PIPE bPipe,
	PUINT8 pu8Numerator, 
	PUINT8 pu8Denominator
	)
{
	UINT32 u32Temp;
	DBG_PRINTF("%s\n",__FUNCTION__);	
	if(bPipe == eDRVVIDEOIN_PACKET)
		u32Temp = inp32(REG_VPEPKDS);
	else  if(bPipe == eDRVVIDEOIN_PLANAR)
		u32Temp = inp32(REG_VPEPNDS);
	else
		return E_DRVVIDEOIN_INVALID_PIPE;			
	*pu8Numerator = (u32Temp & DSHN) >> 8;
	*pu8Denominator = u32Temp & DSHM;
	
	return Successful;
} // DrvVideoIn_GetHorizontalScaleFactor

void 
DrvVideoIn_SetFrameRateScaleFactor(
	UINT8 u8Numerator, 
	UINT8 u8Denominator
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPEFRC, (inp32(REG_VPEFRC) & ~(FRCN | FRCM)) 
    | (((u8Numerator << 8) & FRCN)
    | (u8Denominator & FRCM)));
} // DrvVideoIn_SetFrameRateScaleFactor

void 
DrvVideoIn_GetFrameRateScaleFactor(
	PUINT8 pu8Numerator, 
	PUINT8 pu8Denominator
	)
{
	UINT32 u32Temp = inp32(REG_VPEFRC);
	DBG_PRINTF("%s\n",__FUNCTION__);	
	*pu8Numerator = (u32Temp & FRCN) >> 8;
	*pu8Denominator = u32Temp & FRCM;
} // DrvVideoIn_GetFrameRateScaleFactor

void 
DrvVideoIn_SetAddressMatch(
	UINT32 u32AddressMatch
	)
{
	outp32(REG_CMPADDR, u32AddressMatch);
}

void 
DrvVideoIn_GetAddressMatch(
	PUINT32 pu32AddressMatch
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	*pu32AddressMatch = inp32(REG_CMPADDR);
}

void 
DrvVideoIn_SetStride(
	UINT32 u32PacketStride,
	UINT32 u32PlanarStride	
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);		
	outp32(REG_VSTRIDE, ((u32PlanarStride<<16) & PNSTRIDE) | 
					(u32PacketStride & PKSTRIDE) );
}

void 
DrvVideoIn_GetStride(
	PUINT32	pu32PacketStride,
	PUINT32 pu32PlanarStride	
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	UINT32 u32Tmp =  inp32(REG_VSTRIDE);
	*pu32PlanarStride = (u32Tmp & PNSTRIDE) >>16;
	*pu32PacketStride = u32Tmp & PKSTRIDE;	
}

ERRCODE 
DrvVideoIn_SetBaseStartAddress(
	E_DRVVIDEOIN_PIPE ePipe,
	E_DRVVIDEOIN_BUFFER eBuf,
	UINT32 u32BaseStartAddr
	)	
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	DBG_PRINTF("Addr 0x%x\n",u32BaseStartAddr);	
	if(ePipe==eDRVVIDEOIN_PACKET)
	{
		if(eBuf>eDRVVIDEOIN_BUF1)
			return E_DRVVIDEOIN_INVALID_BUF;
		outp32(REG_PACBA0+eBuf*4, u32BaseStartAddr);	
		DBG_PRINTF("Packet Buf 0x%x\n",u32BaseStartAddr);			
	}	
	else if(ePipe==eDRVVIDEOIN_PLANAR)
	{
		if(eBuf>eDRVVIDEOIN_BUF2)
			return E_DRVVIDEOIN_INVALID_BUF;
		outp32(REG_YBA0+eBuf*4, u32BaseStartAddr);						
	}	
	else
		return E_DRVVIDEOIN_INVALID_PIPE;
	return Successful;	
}

ERRCODE 
DrvVideoIn_GetBaseStartAddress(
	E_DRVVIDEOIN_PIPE ePipe,
	E_DRVVIDEOIN_BUFFER eBuf,
	PUINT32 pu32BaseStartAddr
	)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	if(ePipe==eDRVVIDEOIN_PACKET)
	{
		if(eBuf>eDRVVIDEOIN_BUF1)
			return E_DRVVIDEOIN_INVALID_BUF;
		*pu32BaseStartAddr = inp32(REG_PACBA0+eBuf*4);		
	}	
	else if(ePipe==eDRVVIDEOIN_PLANAR)
	{
		if(eBuf>eDRVVIDEOIN_BUF2)
			return E_DRVVIDEOIN_INVALID_BUF;	
		*pu32BaseStartAddr = inp32(REG_YBA0+eBuf*4);						
	}	
	else
		return E_DRVVIDEOIN_INVALID_PIPE;
	return Successful;		
}


/*---------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_InstallInterrupt			  									                  
//                                                                                                         
// Parameters:                                                                                             
//     u32FieldEnable:  																				   
//			00 = Both fields are disabled																   
//			01 = Field 1 enable																			   
//			10 = Field 2 enable																			   
//			11 = Both fields are enabled  	  		                                          			   
//      eInputType: 																					   
//			0 = CCIR601.	1=CCIR656	 					                                               
// 		bFieldSwap: 																					   
//			Swap field 1 and field 2																	   
// Returns:                                                                                                
//      None                                                                                               
//                                                                                                         
// Description:                                                                                            
//     	Set the input type, fileds enable or disable and fields swap       			                       
//                                                                                                         
//---------------------------------------------------------------------------------------------------------*/

void 
DrvVideoIn_SetInputType(
	UINT32 	u32FieldEnable,						//0: Both fields are disabled. 1: Field 1 enable. 2: Field 2 enable. 3: Both fields are enable
	E_DRVVIDEOIN_TYPE eInputType,				//0: CCIR601.	1: CCIR656 	
	BOOL 	bFieldSwap							//1: Swap two field 
	)	
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPEPAR, (inp32(REG_VPEPAR) & ~(FLDSWAP | FLD1EN | FLD0EN | FLDSWAP)) | 
									((u32FieldEnable << 16) & (FLD1EN | FLD0EN )) |
									((eInputType <<1 ) & SNRTYPE) | 									
    								((bFieldSwap << 13) & FLDSWAP) );
} // DrvVideoIn_SetInputType

void 
DrvVideoIn_GetInputType(
	PUINT32 pu32FieldEnable,					//0: Both fields are disabled. 1: Field 1 enable. 2: Field 2 enable. 3: Both fields are enable
	E_DRVVIDEOIN_TYPE* peInputType,				//0: CCIR601.	1: CCIR656 	
	PBOOL 	pbFieldSwap							//1: Swap two field 
	)	
{
	
	UINT32 u32Tmp = inp32(REG_VPEPAR);	
	DBG_PRINTF("%s\n",__FUNCTION__);	
	*pu32FieldEnable = (u32Tmp & ( FLD1EN | FLD0EN ))>>16;
	*peInputType = (u32Tmp & SNRTYPE )>>1;
	*pbFieldSwap = (u32Tmp & FLDSWAP )>>13;
} // DrvVideoIn_GetInputType

/*---------------------------------------------------------------------------------------------------------
// Function: DrvVideoIn_SetFieldDetection			  									                   
//                                                                                                         
// Parameters:                                                                                             
//      bDetPosition: Available if "bFieldDetMethod"=0              									   
//			00 = Vsync start 																   			   
//			01 = Vsync end																			 	   
//      bFieldDetMethod: 																			       
//				0: Detect field by Vsync & Hsync(SA711113). 											   
//				1: Detect field by field pin(WT8861)													   
// Returns:                                                                                                
//      None                                                                                               
//                                                                                                         
// Description:                                                                                            
//     	Set the input type, fileds enable or disable and fields swap       			                      
//                                                                                                         
//---------------------------------------------------------------------------------------------------------*/
void DrvVideoIn_SetFieldDetection(
	BOOL 	bDetPosition,					 	 	
	BOOL 	bFieldDetMethod						//0: Detect field by Vsync & Hsync(SA711113). 1:Detect field by field pin(WT8861)	 
)
{
	DBG_PRINTF("%s\n",__FUNCTION__);	
	outp32(REG_VPEPAR, (inp32(REG_VPEPAR) & ~(FLDDETP | FLDDETM)) | 
									((bDetPosition << 15) & FLDDETP) |
    								((bFieldDetMethod << 14) & FLDDETM) );
} // DrvVideoIn_SetFieldDetection


void DrvVideoIn_GetFieldDetection(
	PBOOL 	pbDetPosition,						//0: Both fields are disabled. 1: Field 1 enable. 2: Field 2 enable. 3: Both fields are enable. It is only available as CCIR601 	 	
	PBOOL 	pbFieldDetMethod					//0: Detect field by Vsync & Hsync. 1:Detect field by CCIR656. It is only available as CCIR601	 
)
{
	UINT32 u32Tmp = inp32(REG_VPEPAR);	
	DBG_PRINTF("%s\n",__FUNCTION__);		
	*pbDetPosition = (u32Tmp & FLDDETP) >> 15;
	*pbFieldDetMethod = (u32Tmp & FLDDETM) >> 14;	
} // DrvVideoIn_GetFieldDetection
/*========================================================================
	VideoIn Driver End
========================================================================*/
