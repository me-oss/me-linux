/* linux/include/asm-arm/arch-w55fa93/w55fa93_audio.h
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
 *   2006/08/26     vincen.zswan add this file for nuvoton w55fa93 evb.
 */

#ifndef _W55FA93_AUDIO_H_
#define _W55FA93_AUDIO_H_

#include <asm/semaphore.h>
#include <asm/io.h>

typedef unsigned char     UINT8;    
typedef unsigned short    UINT16;
typedef unsigned int      UINT32;
typedef void              VOID;
typedef int               INT;

typedef int (*AU_CB_FUN_T)(u32, u32);

//#define OPT_ADD_I2S_REC

typedef enum au_dev_e
{
	AU_DEV_AC97,
	AU_DEV_IIS
} AU_DEV_E;


typedef struct wb_audio_play_codec_t{
	int		(*get_capacity)(void);
	void	(*set_play_buffer)(u32, u32);
	int		(*set_play_volume)(u32 nLeft, u32 nRight);
	int		(*start_play)(AU_CB_FUN_T fnCallBack, int nSamplingRate, int nChannels);
	void	(*stop_play)(void);
	void	(*play_interrupt)(void);
	int		(*init_play)(void);
}NV_AUDIO_PLAY_CODEC_T;

typedef struct wb_audio_record_codec_t{
	void	(*set_record_buffer)(u32, u32);
	int		(*set_record_volume)(u32 nLeft, u32 nRight);
	int		(*start_record)(AU_CB_FUN_T fnCallBack, int nSamplingRate, int nChannels);
	void	(*stop_record)(void);
	void	(*record_interrupt)(void);
	int		(*init_record)(void);
}NV_AUDIO_RECORD_CODEC_T;


typedef struct audio_t
{
	AU_CB_FUN_T 	fnPlayCallBack;
	AU_CB_FUN_T 	fnRecCallBack;
	int				nPlaySamplingRate;
	int				nRecSamplingRate;
	short			sPlayVolume;
	short			sRecVolume;
	u32			uPlayBufferAddr;
	u32			uPlayBufferLength;
	u32			uRecordBufferAddr;
	u32			uRecordBufferLength;
}AUDIO_T;

typedef struct wb_audio_t{
	int state;
	//int open_flags;
	
	int open_play_flags, open_record_flags;	
	int ac97iis_irq;	//ac97_iis has been installed?
	
	int dsp_play_dev, dsp_record_dev, dsp_play_openflag, dsp_record_openflag;
	int mixer_play_dev, mixer_record_dev, mixer_play_openflag, mixer_record_openflag;
	
	unsigned int play_buf_addr, record_buf_addr;
	unsigned int play_buf_length, record_buf_length;
	
	//int nSamplingRate;
	int nPlaySamplingRate, nRecSamplingRate;
	//int nChannels;
	int nPlayChannels, nRecChannels;
	int nPlayVolumeLeft, nPlayVolumeRight, nRecordVolumeLeft, nRecordVolumeRight;
	int	uRecordSrc;
	
	struct{
		int ptr;
	}play_half_buf[2], record_half_buf[2];
	
  	//WB_AUDIO_CODEC_T *codec;
  	NV_AUDIO_PLAY_CODEC_T *play_codec;
  	NV_AUDIO_RECORD_CODEC_T *record_codec;
  	
	struct semaphore dsp_read_sem, dsp_write_sem, mixer_sem;
	struct fasync_struct *fasync_ptr;
	wait_queue_head_t write_wait_queue, read_wait_queue;
}WB_AUDIO_T;
	

#define AU_SAMPLE_RATE_96000	96000
#define AU_SAMPLE_RATE_88200	88200
#define AU_SAMPLE_RATE_64000	64000
#define AU_SAMPLE_RATE_48000	48000
#define AU_SAMPLE_RATE_44100	44100
#define AU_SAMPLE_RATE_32000	32000
#define AU_SAMPLE_RATE_24000	24000
#define AU_SAMPLE_RATE_22050	22050
#define AU_SAMPLE_RATE_20000	20000	/* Only for audio recording from ADC */ 	
#define AU_SAMPLE_RATE_16000	16000
#define AU_SAMPLE_RATE_12000	12000 	/* Only for audio recording from ADC */ 
#define AU_SAMPLE_RATE_11025	11025
#define AU_SAMPLE_RATE_8000	8000

#define AU_CH_MONO		1
#define AU_CH_STEREO	2

/* state code */
#define AU_STATE_NOP			0
#define AU_STATE_PLAYING		1
#define AU_STATE_RECORDING	2

/* capacity */
#define AU_CAP_DUPLEX			1

/* Error Code */
#define ERR_AU_GENERAL_ERROR	-1
#define ERR_AU_NO_MEMORY		-5		/* memory allocate failure */
#define ERR_AU_ILL_BUFF_SIZE	-10		/* illegal callback buffer size */
#define ERR_MA5I_NO_DEVICE		-90		/* have no MA5i chip on board */
#define ERR_DAC_PLAY_ACTIVE	-110	/* DAC playback has been activated */
#define ERR_DAC_NO_DEVICE		-111	/* DAC is not available */
#define ERR_ADC_REC_ACTIVE		-120	/* ADC record has been activated */
#define ERR_ADC_NO_DEVICE		-121	/* ADC is not available */
#define ERR_I2S_PLAY_ACTIVE		-140	/* IIS playback has been activated */
#define ERR_I2S_REC_ACTIVE		-141	/* IIS record has been activated */
#define ERR_I2S_NO_DEVICE		-142	/* has no IIS codec on board */
#define ERR_WM8753_NO_DEVICE	-150	/* has no wm8753 codec on board */
#define ERR_W5691_PLAY_ACTIVE	-160	/* W5691 playback has been activated */
#define ERR_W5691_NO_DEVICE	-161	/* Have no W5691 chip on board */

#define ERR_NO_DEVICE			-201	/* audio device not available */


//use dac on off api??
//#define USE_DAC_ON_OFF_API

//open dac power on at startup time
#define USE_DAC_ALWAYS_ON

#if defined(USE_DAC_ON_OFF_API) && defined(USE_DAC_ALWAYS_ON)
#error "On/Off Apis and DAC always on function can't use at the same time"
#endif

#endif	/* _W55FA93_AUDIO_H_ */

