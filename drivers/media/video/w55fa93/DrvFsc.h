/****************************************************************
 *                                                              *
 * Copyright (c) Nuvoton Technology Corp. All rights reserved. *
 *                                                              *
 ****************************************************************/
 
#ifndef __DRVFSC_H__
#define __DRVFSC_H__

#include <asm/arch/w55fa93_reg.h>

#define ERRCODE		UINT32

#define E_DRVFSC_INVALID_CHANNEL       	0xF1005000
#define E_DRVFSC_INVALID_INT       		0xF1005001
#define E_DRVFSC_INVALID_ACCESS       	0xF1005002
#define E_DRVFSC_INVALID_MODE		0xF1005003
#define E_DRVFSC_INVALID_BUF			0xF1005004
#define E_DRVFSC_INVALID_FRAMERATE	0xF1005005

#define UINT8		__u8
#define UINT16	__u16
#define UINT32	__u32
#define INT8		__s8
#define INT16		__s16
#define INT32		__s32

#define BOOL		UINT32
#define FALSE		0
#define TRUE			1

#define PBOOL		BOOL*
#define PUINT8		UINT8*
#define PUINT16		UINT16*
#define PUINT32		UINT32*
#define Successful	0
#define ERRCODE 		UINT32 

typedef enum
{
    eDRVFSC_CH0_WS_INT = 0,
    eDRVFSC_CH0_RS_INT,
    eDRVFSC_CH0_WE_INT,
    eDRVFSC_CH0_RE_INT,
    eDRVFSC_CH1_WS_INT,
    eDRVFSC_CH1_RS_INT,
    eDRVFSC_CH1_WE_INT,
    eDRVFSC_CH1_RE_INT
}E_DRVFSC_CHANNEL_INT;

typedef enum
{
    eDRVFSC_CHANNEL_0 = 0,
    eDRVFSC_CHANNEL_1 = 1
}E_DRVFSC_CHANNEL;

typedef enum
{
    eDRVFSC_RIP_SEL = 0,
    eDRVFSC_WIP_SEL = 1
}E_DRVFSC_RWIP_SEL;
typedef enum
{
    eDRVFSC_VPOST_CH0_GPU_CH1 = 0,
    eDRVFSC_GPU_CH0_VPOST_CH1 = 1
}E_DRVFSC_RIP;

typedef enum
{
    eDRVFSC_GPU_CH0_VIDEOIN_CH1 = 0,
    eDRVFSC_VIDEOIN_CH0_GPU_CH1 = 1
}E_DRVFSC_WIP;

typedef enum
{
    eDRVFSC_HARDWARE_FRAME_RATE = 0,
    eDRVFSC_HARDWARE_FRAME_SYNC = 1
   	//eDRVFSC_SOFTWARE_MODE = 2
}E_DRVFSC_FSM;

typedef enum
{
    eDRVFSC_FRAME_RATE_8_16 = 0,
    eDRVFSC_FRAME_RATE_9_16, 
    eDRVFSC_FRAME_RATE_10_16,
    eDRVFSC_FRAME_RATE_11_16,
    eDRVFSC_FRAME_RATE_12_16,
    eDRVFSC_FRAME_RATE_13_16,
    eDRVFSC_FRAME_RATE_14_16,
    eDRVFSC_FRAME_RATE_15_16,    
    eDRVFSC_FRAME_RATE_17_16,
    eDRVFSC_FRAME_RATE_18_16,
    eDRVFSC_FRAME_RATE_19_16,
    eDRVFSC_FRAME_RATE_20_16,
    eDRVFSC_FRAME_RATE_21_16,
    eDRVFSC_FRAME_RATE_22_16,
    eDRVFSC_FRAME_RATE_23_16,
    eDRVFSC_FRAME_RATE_24_16
}E_DRVFSC_FRAMERATE;

ERRCODE 
DrvFSC_Open(
	void
	);
void DrvFSC_Close(
	void
	);	
/*
ERRCODE 
DrvFSC_InstallCallback(
	E_DRVFSC_CHANNEL_INT eIntType, 
	PFN_DRVFSC_CALLBACK pfnCallback,
	PFN_DRVFSC_CALLBACK* pfnOldCallback
);
*/

ERRCODE 
DrvFSC_EnableInt(
	E_DRVFSC_CHANNEL_INT eIntType
);
	
ERRCODE 
DrvFSC_SetChannelAccessControl(
	E_DRVFSC_RIP eReadIP, 
	E_DRVFSC_WIP eWriteIP
	);	
	
ERRCODE 
DrvFSC_SetChannelConfigure(
	E_DRVFSC_CHANNEL eChannel, 
	BOOL bIsEnable,
	E_DRVFSC_FSM eSwitchMethod, 
	BOOL bIsTripleBuf);	

ERRCODE 
DrvFSC_SetChannelBaseAddr(
	E_DRVFSC_CHANNEL eChannel, 
	UINT32 u32Buf, 
	UINT32 u32BaseAddr
	);


#endif














