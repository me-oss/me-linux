/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright (c) Nuvoton Technology Corp. All rights reserved.                                             */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
 
/*---------------------------------------------------------------------------------------------------------*/
/* Includes of local headers                                                                               */
/*---------------------------------------------------------------------------------------------------------*/
#include <asm/io.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/videoin.h>
//#include <asm/arch/DrvFsc.h>
#include "DrvFsc.h"

#define ERR_PRINTF			printk
#define outp32(addr, value)		outl(value, addr)
#define inp32(addr)			inl(addr)
#define DBG_PRINTF(...)		
//PFN_DRVFSC_CALLBACK (pfnFSCIntHandlerTable)[8]={0};

/*---------------------------------------------------------------------------------------------------------*/
/* Function: DrvFSC_ISR                                                                              */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*              None                                                                                       */
/*                                                                                                         */
/* Returns:                                                                                                */
/*              None                                                                                       */
/*                                                                                                         */
/* Description:                                                                                            */
/*              Internal used API for preprocessing interrupt of FSC.                                      */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#if 0
static void 
(
	void
	)
{
	volatile UINT32 u32IntStatus, u32Idx=1, u32IntChannel=0;
	volatile UINT32 u32IntEnable;	
	u32IntStatus = inp32(REG_FSINT_CTL);
	u32IntEnable = u32IntStatus & 0xFF00;						//Write one clear 
	u32IntStatus = (u32IntStatus & (u32IntEnable>>8));	
	while(u32Idx <= 0x80)
	{		
		if(u32IntStatus&u32Idx)
		{			
			pfnFSCIntHandlerTable[u32IntChannel]();		
			outp32(REG_FSINT_CTL, u32IntEnable | u32Idx);			//Clear interrupt							
			break;
		}	
		u32Idx = u32Idx <<1;	
		u32IntChannel = u32IntChannel+1;
	}
}
#endif

ERRCODE DrvFSC_EnableInt(
	E_DRVFSC_CHANNEL_INT eIntType
	)
{
	if(eIntType>eDRVFSC_CH1_WE_INT)
		return E_DRVFSC_INVALID_INT;
	outp32(REG_FSINT_CTL, (inp32(REG_FSINT_CTL) & 0xFF00)| /* Write one clear */
						(1<<(eIntType+8)));			/* Enable Int */ 
	return Successful;
}

BOOL 
DrvFSC_IsIntEnabled(
	E_DRVFSC_CHANNEL_INT eIntType
	)
{
	return ( (inp32(REG_FSINT_CTL) & (1<<(eIntType+8)))?TRUE:FALSE );
}
	
ERRCODE 
DrvFSC_DisableInt(
	E_DRVFSC_CHANNEL_INT eIntType
	)
{
	if(eIntType>eDRVFSC_CH1_WE_INT)
		return E_DRVFSC_INVALID_INT;
	outp32(REG_FSINT_CTL, (inp32(REG_FSINT_CTL) & 0xFF00) & /* Write one clear */
						~(1<<(eIntType+8)));		/* Disable Int */ 
	return Successful;
}	

ERRCODE 
DrvFSC_ClearInt(
	E_DRVFSC_CHANNEL_INT eIntType
	)
{
	if(eIntType>eDRVFSC_CH1_WE_INT)
		return E_DRVFSC_INVALID_INT;
	outp32(REG_FSINT_CTL, (inp32(REG_FSINT_CTL) & 0xFF00) | /* Write one clear */
					  (1<<(eIntType+8)) );						
	return Successful;					  
}	

BOOL 
DrvFSC_PollInt(
	E_DRVFSC_CHANNEL_INT eIntType
	)
{
	return ( (inp32(REG_FSINT_CTL) & (1<<eIntType))?TRUE:FALSE );
}	

/*---------------------------------------------------------------------------------------------------------*/
/* Function: DrvFSC_InstallCallback                                                                       */
/*                                                                                                         */
/* Parameters:																							   */
/*				eIntType : Interrupt type                                                                  */
/*              pfnIsr   : Call back function		                                                       */
/*                                                                                                         */
/* Returns:                                                                                                */
/*              None                                                                                       */
/*                                                                                                         */
/* Description:                                                                                            */
/*             	Register call back functions for upper layer to process the interrupt of FSC respectively  */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#if 0
ERRCODE 
DrvFSC_InstallCallback(
	E_DRVFSC_CHANNEL_INT eIntType, 
	PFN_DRVFSC_CALLBACK pfnCallback,
	PFN_DRVFSC_CALLBACK* pfnOldCallback
	)
{
	if(eIntType>eDRVFSC_CH1_WE_INT)
		return E_DRVFSC_INVALID_INT;
	*pfnOldCallback = pfnFSCIntHandlerTable[eIntType]; 	
	pfnFSCIntHandlerTable[eIntType] = (PFN_DRVFSC_CALLBACK)(pfnCallback);		// Setup call back function 
	return Successful;
}
#endif 
/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_Open                                                                               */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      NONE                                                                                      */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               The function is used to start the colck of FSC						   */
/*---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvFSC_Open(
	void
	)
{	
	outp32(REG_AHBCLK, inp32(REG_AHBCLK) | FSC_CKE);			//Enable Clock		

	outp32(REG_AHBIPRST, inp32(REG_AHBIPRST) | FSCRST);			//Global Reset IP
	outp32(REG_AHBIPRST, inp32(REG_AHBIPRST) & (~FSCRST));
	outp32(REG_FSC_CTL, inp32(REG_FSC_CTL) | FSC_EN);			//Internal Reset and Enable IP (Jason2:High Reset)
														//PIN function			
//	DrvAIC_InstallISR((E_DRVAIC_INT_LEVEL)eDRVAIC_INT_LEVEL1, eDRVAIC_INT_FSC, (PVOID)DrvFSC_ISR, 0);
//	DrvAIC_EnableInt(eDRVAIC_INT_FSC);																
	#if 0			
	sysInstallISR(IRQ_LEVEL_1, 
						IRQ_FSC, 
						(PVOID)DrvFSC_ISR);
	sysEnableInterrupt(IRQ_FSC);	
	#endif														
	return Successful;
}
/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_Close                                                                              */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      E_SUCCESS                                                                                 */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               The function is used to Stop the colck of FSC          				   */
/*---------------------------------------------------------------------------------------------------------*/
void DrvFSC_Close(
	void
	)
{
	outp32(REG_AHBIPRST, inp32(REG_AHBIPRST) | FSCRST);			//Global Reset IP
	outp32(REG_AHBIPRST, inp32(REG_AHBIPRST) & (~FSCRST));
	
	outp32(REG_FSC_CTL, inp32(REG_FSC_CTL) | FSC_RST | FSC_EN);	//Internal Reset and Enable IP (Jason2:High Reset)
	outp32(REG_FSC_CTL, inp32(REG_FSC_CTL) & ~(FSC_RST | FSC_EN));                           
}
/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_SetChannelAccessControl                                                            */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      E_SUCCESS or E_DRVFSC_INVALID_ACCESS                                                                                */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               Specify the read IPs and write IPs for both channel 	  				   				   */
/*---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvFSC_SetChannelAccessControl(
	E_DRVFSC_RIP eReadIP, 
	E_DRVFSC_WIP eWriteIP
	)
{
	UINT32 u32RegData;
	if(eReadIP>eDRVFSC_VIDEOIN_CH0_GPU_CH1)
		return E_DRVFSC_INVALID_ACCESS;
	if(eWriteIP>eDRVFSC_VIDEOIN_CH0_GPU_CH1)
		return E_DRVFSC_INVALID_ACCESS;
	u32RegData= inp32(REG_FSC_CTL);
	u32RegData = (u32RegData & ~(FSC_RIP_SEL | FSC_WIP_SEL)) | 
						((((eReadIP<<12) & FSC_RIP_SEL)) |
						((eWriteIP<<8) & FSC_WIP_SEL));
	//printf("0x%x\n", u32RegData);					
	outp32(REG_FSC_CTL,u32RegData);	
	return Successful;  	                            
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_GetChannelAccessControl                                                            */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               peReadIP : Read IP for channel 0 and 1                                                                                     */
/*               peWriteIP: Write IP for channel 0 and 1                                                                                         */
/* Returns:      None                                                                                 */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               Get the read IPs and write IPs for both channel          				   		    	   */
/*---------------------------------------------------------------------------------------------------------*/
void DrvFSC_GetChannelAccessControl(
	E_DRVFSC_RIP* peReadIP, 
	E_DRVFSC_WIP* peWriteIP
	)
{
	UINT32 u32RegData = inp32(REG_FSC_CTL);
	*peWriteIP = (u32RegData & FSC_WIP_SEL)>>8;   
	*peReadIP = (u32RegData & FSC_RIP_SEL)>>12;                             
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_SetChannelConfigure                                                                */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      E_SUCCESS                                                                                 */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               Specify the configuration of channel. For example: Buffer number, switching method		   */
/*---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvFSC_SetChannelConfigure(
	E_DRVFSC_CHANNEL eChannel, 
	BOOL bIsEnable,
	E_DRVFSC_FSM eSwitchMethod, 
	BOOL bIsTripleBuf)
{
	UINT32 u32RegAddr, u32RegData; 
	
	if(eChannel>eDRVFSC_CHANNEL_1)
		return E_DRVFSC_INVALID_CHANNEL;
	if(eSwitchMethod>eDRVFSC_HARDWARE_FRAME_SYNC)
		return E_DRVFSC_INVALID_MODE;
		
	u32RegAddr = (REG_FSC0_CTL+(eChannel&0x01)*0x100);

	u32RegData =  ( ((eSwitchMethod<<2) & FSC_FSM) |
						((bIsTripleBuf<<1) & FSC_BN) ) |
						(bIsEnable & FSC_EN);
#if 0						
	outp32(u32RegAddr, (inp32(u32RegAddr) &  ~(FSC_FSM | FSC_BN | FSC_EN) ) |
						(u32RegData | FSC_RST) );	
	outp32(u32RegAddr, inp32(u32RegAddr) & ~FSC_RST);	
#else
	outp32(u32RegAddr, (inp32(u32RegAddr) &  ~(FSC_FSM | FSC_BN | FSC_EN) ) |
						(u32RegData) );	
	//outp32(u32RegAddr, inp32(u32RegAddr) & ~FSC_RST);	
#endif	
	
	return Successful;  	                            
}
ERRCODE  
DrvFSC_GetChannelConfigure(
	E_DRVFSC_CHANNEL eChannel, 
	PBOOL pbIsEnable,
	E_DRVFSC_FSM* peSwitchMethod, 
	PBOOL pbIsTripleBuf)
{
	
	UINT32 u32RegAddr, u32RegData; 
	
	if(eChannel>eDRVFSC_CHANNEL_1)
		return E_DRVFSC_INVALID_CHANNEL;
		
	u32RegAddr = (REG_FSC0_CTL+(eChannel&0x01)*0x100);
	u32RegData = inp32(u32RegAddr);
	*pbIsEnable = u32RegData & FSC_EN;
	*peSwitchMethod = (u32RegData & FSC_FSM)>>2;   
	*pbIsTripleBuf = (u32RegData & FSC_BN)>>1;     
	return Successful;                          
}
/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_SetChannelBaseAddr                                                                 */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      E_SUCCESS                                                                                 */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               The function is used to specify the buffers address that are shared by write-IP and 	   */
/*					read IP           		     				   										   */
/*---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvFSC_SetChannelBaseAddr(
	E_DRVFSC_CHANNEL eChannel, 
	UINT32 u32Buf, 
	UINT32 u32BaseAddr
	)
{
	UINT32 u32RegAddr;
	if(eChannel>eDRVFSC_CHANNEL_1)
		return E_DRVFSC_INVALID_CHANNEL;
	if(u32Buf>2)
		return E_DRVFSC_INVALID_BUF;	
	 u32RegAddr = (REG_FSC0_BA0+(eChannel&0x01)*0x100)+(u32Buf&0x03)*4;
	outp32(u32RegAddr, u32BaseAddr);
	return Successful;  	                            
}
/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_GetChannelBaseAddr                                                                 */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      E_SUCCESS                                                                                 */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*             The function is used to get the buffers address that are shared by write-IP and 	   	       */
/*					read IP   																			   */
/*---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvFSC_GetChannelBaseAddr(
	E_DRVFSC_CHANNEL eChannel, 
	UINT32 u32Buf, 
	UINT32* pu32BaseAddr
	)
{
	UINT32 u32RegAddr;
	
	if(eChannel>eDRVFSC_CHANNEL_1)
		return E_DRVFSC_INVALID_CHANNEL;
	if(u32Buf>2)
		return E_DRVFSC_INVALID_BUF;	
	u32RegAddr = (REG_FSC0_BA0+(eChannel&0x01)*0x100)+(u32Buf&0x03)*4;
	*pu32BaseAddr = inp32(u32RegAddr);
	return Successful;  	                            
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_GetWriteIpAbondon                                                             	   */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      E_SUCCESS                                                                                 */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               The function is used to get whether the current frame is abandon by write-IP     		   */
/*					It is used in the call back function of FSC											   */	
/*---------------------------------------------------------------------------------------------------------*/
BOOL 
DrvFSC_GetWriteIpAbondon(
	E_DRVFSC_CHANNEL eChannel
	)
{
	UINT32 u32RegAddr = inp32(REG_FSC0_CTL+(eChannel&0x01)*0x100);
	return (((u32RegAddr & FSC_WIP_ABANDON)!=0)?TRUE:FALSE);            
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_SetChannelFrameRateRatio                                                           */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      E_SUCCESS                                                                                 */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               The function is used to set the frame rate ratio of FSC as frame rate control mode        */
/*---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvFSC_SetChannelFrameRateRatio(
	E_DRVFSC_CHANNEL eChannel, 
	BOOL bIsDependWriteIP, 
	E_DRVFSC_FRAMERATE eFrameRate,
	UINT32 u32BlankTime
	)
{	
	UINT32 u32RegAddr, u32ChannelCtl;
	
	if(eChannel>eDRVFSC_CHANNEL_1)
		return E_DRVFSC_INVALID_CHANNEL;
	if(eFrameRate>eDRVFSC_FRAME_RATE_24_16)
		return E_DRVFSC_INVALID_FRAMERATE;	
		
	u32RegAddr = (REG_FSC0_CTL+(eChannel&0x01)*0x100);
	u32ChannelCtl = inp32(u32RegAddr);
	outp32(u32RegAddr, (u32ChannelCtl & ~(FSC_FR_SRC | FSC_FR)) |
						( ((bIsDependWriteIP<< 24) & FSC_FR_SRC) | 
						  ((eFrameRate<< 4) & FSC_FR) ) );	
	u32RegAddr = (REG_FSC0_BCNT+(eChannel&0x01)*0x100);					  
	outp32(u32RegAddr, u32BlankTime); 
	return Successful; 					  						                          
}

/*---------------------------------------------------------------------------------------------------------*/
/* Function:     DrvFSC_GetChannelFrameRateRatio                                                           */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*               NONE                                                                                      */
/*                                                                                                         */
/* Returns:      E_SUCCESS                                                                                 */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*               The function is used to get the frame rate ratio of FSC as frame rate control mode		   */
/*---------------------------------------------------------------------------------------------------------*/
ERRCODE 
DrvFSC_GetChannelFrameRateRatio(
	E_DRVFSC_CHANNEL eChannel, 
	PBOOL pbIsDependWriteIP, 
	E_DRVFSC_FRAMERATE* peFrameRate,
	PUINT32 pu32BlankTime
	)
{
	UINT32 u32RegAddr, u32ChannelCtl;
	if(eChannel>eDRVFSC_CHANNEL_1)
		return E_DRVFSC_INVALID_CHANNEL;
		
	u32RegAddr = (REG_FSC0_CTL+(eChannel&0x01)*0x100);
	u32ChannelCtl = inp32(u32RegAddr);
	*pbIsDependWriteIP =  (u32ChannelCtl & FSC_FR_SRC) >> 24;
	*peFrameRate = (u32ChannelCtl & FSC_FR) >> 4;
	u32RegAddr = (REG_FSC0_BCNT+(eChannel&0x01)*0x100);	
	*pu32BlankTime = inp32(u32RegAddr);
	return Successful; 					  						                          
}

ERRCODE
DrvFSC_GetBufInfo(E_DRVFSC_CHANNEL eChannel,
				UINT32* pu32WBuf,
				UINT32* pu32RBuf
	)
{
	UINT32 u32RegAddr;
	if(eChannel>eDRVFSC_CHANNEL_1)
		return E_DRVFSC_INVALID_CHANNEL;
		
	u32RegAddr = (REG_FSC0_WBUF+(eChannel&0x01)*0x100);	
	*pu32WBuf = inp32(u32RegAddr);
	*pu32RBuf = inp32((u32RegAddr+0x4));
	return Successful;
}

