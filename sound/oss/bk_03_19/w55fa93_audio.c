/* linux/sound/oss/w55fa93_audio.c
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
 *   2008/12/16     ghguo add this file for nuvoton audio.
 */
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/soundcard.h>
#include <linux/sound.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/arch/irqs.h>

#include <asm/arch/w55fa93_audio.h>
#include <asm/arch/w55fa93_spu.h>
#include <asm/arch/w55fa93_reg.h>
//mlsdev008 *********************************
//#define debug_mode
#ifdef debug_mode
#define print(...) printk(__VA_ARGS__)
#else
#define print(...)
#endif

#define SNDCTL_DSP_GETAIN2	1107
#define SNDCTL_DSP_GETAIN3	1108
#define SNDCTL_DSP_REGISTER	1109
#define srcClock 0 //clock source 0 = xin, .....
#define adcDivN1 1 //clock = source clock/u32AdcDivN1
#define SET_NORMAL_AIN2  do{\
	outl( ((ADC_INT_EN | ADC_CON_ADC_EN | ADC_CONV | (2<<9)) & ~ADC_INT), \
	REG_ADC_CON);\
}while(0)
#define SET_NORMAL_AIN3  do{\
	outl( ((ADC_INT_EN | ADC_CON_ADC_EN | ADC_CONV | (3<<9)) & ~ADC_INT), \
	REG_ADC_CON);\
}while(0)
//*******************************************
#ifdef CONFIG_SOUND_W55FA93_RECORD_ADC
#include <asm/arch/DrvEDMA.h>
#include <asm/arch/w55fa93_adc.h>
#define CLIENT_ADC_NAME "w55fa93-adc"
extern NV_AUDIO_RECORD_CODEC_T nv_adc_record_codec;
int edma_channel = 0; 
#endif

#ifdef CONFIG_HEADSET_ENABLED		// for headset_detect

	#define HEADSET_IRQ_NUM 		W55FA93_IRQ(4)  // nIRQ_GPIO2
	#define Enable_IRQ(n)     		outl(1 << (n),REG_AIC_MECR)
	#define Disable_IRQ(n)    		outl(1 << (n),REG_AIC_MDCR)
	
	static int earphone = -1;	

#endif

#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			
	extern int DrvSPU_SetPAN(u32 u32Channel, u16 u16PAN);	// MSB 8-bit = left channel; LSB 8-bit = right channel
	extern int DrvSPU_SetPauseAddress(u32 u32Channel, u32 u32Address);
	extern int DrvSPU_GetPauseAddress(u32 u32Channel);	
	extern int spuGetPcmVolume(void);
	extern int DrvSPU_GetCurrentAddress(u32 u32Channel);
	extern struct semaphore spu_sem;
	
#endif	

#define AUDIO_WRITE(addr, val)		outl(val, addr)    
#define AUDIO_READ(addr)			inl(addr)              

#define WB_AUDIO_DEFAULT_SAMPLERATE		AU_SAMPLE_RATE_44100
#define WB_AUDIO_DEFAULT_CHANNEL		2
#define WB_AUDIO_DEFAULT_VOLUME			25						//0-31(for SOUND_MIXER_PCM)

//#define WB_AU_DEBUG
//#define WB_AU_DEBUG_ENTER_LEAVE
//#define WB_AU_DEBUG_MSG
//#define WB_AU_DEBUG_MSG2

#ifdef WB_AU_DEBUG
#define DBG(fmt, arg...)			printk(fmt, ##arg)
#else
#define DBG(fmt, arg...)
#endif

#ifdef WB_AU_DEBUG_ENTER_LEAVE
#define ENTER()					DBG("[%-10s] : Enter\n", __FUNCTION__)
#define LEAVE()					DBG("[%-10s] : Leave\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef WB_AU_DEBUG_MSG
#define MSG(fmt)					DBG("[%-10s] : "fmt, __FUNCTION__)
#else
#define MSG(fmt)
#endif

#ifdef WB_AU_DEBUG_MSG2
#define MSG2(fmt, arg...)			DBG("[%-10s] : "fmt, __FUNCTION__, ##arg)
#else
#define MSG2(fmt, arg...)
#endif

#define OPT_ADD_PAUSE
#define OPT_SET_RECORD_FRAGMENT

unsigned int  phaddrplay;
unsigned int dma_play_buf_addr;
unsigned int  phaddrrecord;
unsigned int dma_rec_buf_addr;

#define AUDIO_BUFFER_ORDER	4	
#define FRAG_SIZE	(( PAGE_SIZE << AUDIO_BUFFER_ORDER ) / 2)

unsigned int dmawrbuf=0;

extern NV_AUDIO_PLAY_CODEC_T nv_spu_play_codec;
extern NV_AUDIO_PLAY_CODEC_T nv_i2s_play_codec;
extern NV_AUDIO_RECORD_CODEC_T nv_i2s_record_codec;

#define TRUE	1
#define FALSE	0
static int dspwriteptr=0;
static int s_PlayChannels = 0;
static int s_IsDefaultVolume = TRUE;	
static int s_PauseAddress;

//control dac power by sysfs
int dac_auto_config;
//static 	struct mutex buf_mutex;	//buffer mutex lock
static volatile WB_AUDIO_T audio_dev;

extern NV_AUDIO_PLAY_CODEC_T nv_spu_play_codec;
extern NV_AUDIO_PLAY_CODEC_T nv_i2s_play_codec;
extern NV_AUDIO_RECORD_CODEC_T nv_i2s_record_codec;

#if defined(CONFIG_SPU_BUF_8K)
static int fragment_size = 1 << 13, max_fragments = 2;  // 13 => 8k + 8k 
static int r_fragment_size = 1 << 13, r_max_fragments = 2;  	// 13 => 8k + 8k (for recording) 
static int max_fragment_size = 1 << 13;
#elif defined (CONFIG_SPU_BUF_32K)
static int fragment_size = 1 << 15, max_fragments = 2;  // 13 => 8k + 8k 
static int r_fragment_size = 1 << 15, r_max_fragments = 2;  	// 15 => 32k + 32k 
static int max_fragment_size = 1 << 15;
#else //default to 32k 
static int fragment_size = 1 << 15, max_fragments = 2;  // 13 => 8k + 8k 
static int r_fragment_size = 1 << 15, r_max_fragments = 2;  	// 15 => 32k + 32k 
static int max_fragment_size = 1 << 15;
#endif

#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			
	extern u8 _u8Channel0, _u8Channel1;
#endif	

static int silence_len;

//enable silence function in ISR?
static int enableFillSilence = 0;

static int dev_dsp[2] = {-1, -1}, dev_mixer[2] = {-1, -1};
#ifdef CONFIG_SOUND_W55FA93_RECORD_ADC
static int nAdcSamplingRate[]={AU_SAMPLE_RATE_8000,
							AU_SAMPLE_RATE_12000};
extern int w55fa93_edma_setAPB(int channel, E_DRVEDMA_APB_DEVICE eDevice, E_DRVEDMA_APB_RW eRWAPB, E_DRVEDMA_TRANSFER_WIDTH eTransferWidth);
#endif	
	
static int nSamplingRate[]={AU_SAMPLE_RATE_8000,
							AU_SAMPLE_RATE_11025,
							AU_SAMPLE_RATE_12000,							
							AU_SAMPLE_RATE_16000,
							AU_SAMPLE_RATE_20000,							
							AU_SAMPLE_RATE_22050,
							AU_SAMPLE_RATE_24000,
							AU_SAMPLE_RATE_32000,
							AU_SAMPLE_RATE_44100,
							AU_SAMPLE_RATE_48000,
							AU_SAMPLE_RATE_64000,
							AU_SAMPLE_RATE_88200,							
							AU_SAMPLE_RATE_96000};														

#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                

#define SPU_FAILED	-1

int spu_SetPAN(u32 u32Channel, u16 u16PAN)
{
	if (down_interruptible(&spu_sem))
	        return SPU_FAILED;
	
	DrvSPU_SetPAN(u32Channel, u16PAN);	// MSB 8-bit = left channel; LSB 8-bit = right channel	
    up(&spu_sem);	
}	

int spu_GetCurrentAddress(u32 u32Channel)
{
	int u32Address;	

#ifdef CONFIG_HEADSET_ENABLED		// for headset_detect
	Disable_IRQ(HEADSET_IRQ_NUM);	
#endif	
	
	if (down_interruptible(&spu_sem))
	        return SPU_FAILED;
	
	u32Address = DrvSPU_GetCurrentAddress(u32Channel);	
    up(&spu_sem);		

#ifdef CONFIG_HEADSET_ENABLED		// for headset_detect
	Enable_IRQ(HEADSET_IRQ_NUM);	    
#endif
	
    return u32Address;
    
}	

int spu_SetPauseAddress(u32 u32Channel, u32 u32Address)
{
#ifdef CONFIG_HEADSET_ENABLED		// for headset_detect
	Disable_IRQ(HEADSET_IRQ_NUM);	
#endif
	
	if (down_interruptible(&spu_sem))
	        return SPU_FAILED;
	
	DrvSPU_SetPauseAddress(u32Channel, u32Address);	
	s_PauseAddress = u32Address;
    up(&spu_sem);		

#ifdef CONFIG_HEADSET_ENABLED		// for headset_detect
	Enable_IRQ(HEADSET_IRQ_NUM);	        
#endif	
}	

int spu_GetPcmVolume(void)
{
	int volume;

#ifdef CONFIG_HEADSET_ENABLED		// for headset_detect
	Disable_IRQ(HEADSET_IRQ_NUM);	
#endif
	
	if (down_interruptible(&spu_sem))
	        return SPU_FAILED;
	
	volume = spuGetPcmVolume();
    up(&spu_sem);		

#ifdef CONFIG_HEADSET_ENABLED		// for headset_detect    
	Enable_IRQ(HEADSET_IRQ_NUM);	        
#endif
	
    return volume;	
	
}	
#endif



#ifdef CONFIG_HEADSET_ENABLED		

	void headset_detection(void);
	static irqreturn_t headset_detect_irq(int irq, void *dev_id, struct pt_regs *regs)
	{
	
	        u32 src;
	
	      	Disable_IRQ(HEADSET_IRQ_NUM);		
	      		
	#if defined (CONFIG_HEADSET_GPB2_AND_SPEAKER_GPB3)
	        //printk("headset detect irq \n");
	        src = inl(REG_IRQTGSRC0);
	        outl(src & 0x00040000, REG_IRQTGSRC0);
	        	      		        
			if(src & 0x00040000)
				headset_detection();	        

	#elif defined (CONFIG_HEADSET_GPD14_AND_SPEAKER_GPA2)
	        //printk("headset detect irq \n");
	        src = inl(REG_IRQTGSRC1);
	        outl(src & 0x40000000, REG_IRQTGSRC1);
	        	      		        
			if(src & 0x40000000)
				headset_detection();	        
	
	#elif defined (CONFIG_HEADSET_GPE0_AND_SPEAKER_GPE1)			
	        //printk("headset detect irq \n");
	        src = inl(REG_IRQTGSRC2);
	        outl(src & 0x00000001, REG_IRQTGSRC2);
	        	      		        
			if(src & 0x00000001)
				headset_detection();	        

	#elif defined (CONFIG_HEADSET_GPD3_AND_SPEAKER_GPD4)
	        //printk("headset detect irq \n");  
	        src = inl(REG_IRQTGSRC1);
	        outl(src & 0x00080000, REG_IRQTGSRC1);
	        	      		        
			if(src & 0x00080000)
				headset_detection();	        
				
	#endif				
				
	 //       Disable_IRQ(HEADSET_IRQ_NUM);

	        // clear source

	        
	      	Enable_IRQ(HEADSET_IRQ_NUM);			        
	        return IRQ_HANDLED;
	}
	
	void headset_detection(void)
	{
	        int val;
	
	#if defined (CONFIG_HEADSET_GPB2_AND_SPEAKER_GPB3)	
	        //detect headset is plugged in or not
	        val = inl(REG_GPIOB_PIN);
	
	        //disable/enable speaker
	        if (val & 0x004) 		// GPIOB_2
	        {			
				if(earphone == 1)
					return;
	
		      	mdelay(30);		
	        	val = inl(REG_GPIOB_PIN);		      	
		        if (val & 0x004) 		// GPIOB_2
		        {			
	                //printk("headset plugged in!!\n");		        	
	                earphone = 1;
					outl(inl(REG_GPIOB_DOUT) & ~(1 << 3), REG_GPIOB_DOUT);	// switch GGPIOB_3
					
				}
				else
				{
	                //printk("headset plugged out!!\n");		        						
	                earphone = 0;
					outl(inl(REG_GPIOB_DOUT) | (1 << 3), REG_GPIOB_DOUT);	// switch GGPIOB_3
				}										
				
	        } 
	        else 
	        {
				if(earphone == 0)
					return;

		      	mdelay(30);		
	        	val = inl(REG_GPIOB_PIN);		      	
		        if (val & 0x004) 		// GPIOB_2
		        {			
	                //printk("headset plugged in!!\n");		        	
	                earphone = 1;
					outl(inl(REG_GPIOB_DOUT) & ~(1 << 3), REG_GPIOB_DOUT);	// switch GGPIOB_3
					
				}
				else
				{
	                //printk("headset plugged out!!\n");		        						
	                earphone = 0;
					outl(inl(REG_GPIOB_DOUT) | (1 << 3), REG_GPIOB_DOUT);	// switch GGPIOB_3
				}										
					
	    	}
	    	
	#elif defined (CONFIG_HEADSET_GPD14_AND_SPEAKER_GPA2)			
	
	        //detect headset is plugged in or not
	        val = inl(REG_GPIOD_PIN);
	
	        //disable/enable speaker
	        if (val & 0x4000) 		// GPIOD_14
	        {			
				if(earphone == 0)
					return;

				outl(0x03, REG_SPU_CH_PAUSE);	
				
		      	mdelay(30);		
	        	val = inl(REG_GPIOD_PIN);		      	
		        if (val & 0x4000) 		// GPIOD_14
		        {			
	                //printk("headset plugged out!!\n");		        								        	
		        	outl(inl(REG_GPIOA_DOUT)|(0x0004), REG_GPIOA_DOUT); 	// GPA2 = high        		                
	                earphone = 0;
	                
	                
		#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                
				//	DrvSPU_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel	
					if (s_PlayChannels == 1) {
						spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
					else if (s_PlayChannels == 2) {
						spu_SetPAN(1, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
		#endif					
				}
				else
				{
	                //printk("headset plugged in!!\n");		        	
		        	outl(inl(REG_GPIOA_DOUT)&(~0x0004), REG_GPIOA_DOUT); 	// GPA2 = low        		                	                
	                earphone = 1;

		#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                
				//	DrvSPU_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
					if (s_PlayChannels == 1) {
						spu_SetPAN(0, 0x1F1F);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
					else if (s_PlayChannels == 2) {					
						spu_SetPAN(1, 0x001F);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
					}
		#endif					
				}										
				
	        } 
	        else 
	        {
				if(earphone == 1)
					return;

				outl(0x03, REG_SPU_CH_PAUSE);
				
		      	mdelay(30);		
	        	val = inl(REG_GPIOD_PIN);		      	
		        if (val & 0x4000) 		// GPIOD_14
		        {			
	                //printk("headset plugged out!!\n");		        								        	
		        	outl(inl(REG_GPIOA_DOUT)|(0x0004), REG_GPIOA_DOUT); 	// GPA2 = high        		                	                
	                earphone = 0;
	                
		#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                
				//	DrvSPU_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel	
	
					if (s_PlayChannels == 1) {
						spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
					else if (s_PlayChannels == 2) {
						spu_SetPAN(1, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
		#endif					
				}
				else
				{
	                //printk("headset plugged in!!\n");		        	
		        	outl(inl(REG_GPIOA_DOUT)&(~0x0004), REG_GPIOA_DOUT); 	// GPA2 = low        		                	                	                
	                earphone = 1;
	                
		#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                
				//	DrvSPU_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
					if (s_PlayChannels == 1) {
						spu_SetPAN(0, 0x1F1F);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
					else if (s_PlayChannels == 2) {					
						spu_SetPAN(1, 0x001F);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
					}
		#endif					
				}										
	    	}
	
			outl(0x00, REG_SPU_CH_PAUSE);
			
	#elif defined (CONFIG_HEADSET_GPE0_AND_SPEAKER_GPE1)			
	
	        //detect headset is plugged in or not
	        val = inl(REG_GPIOE_PIN);
	
	        //disable/enable speaker
	        if (val & 0x001) 		// GPIOE_0
	        {			
				if(earphone == 1)
					return;
	
		      	mdelay(30);		
	        	val = inl(REG_GPIOE_PIN);		      	
		        if (val & 0x001) 		// GPIOE_0
		        {			
	                //printk("headset plugged in!!\n");		        	
	                earphone = 1;
					outl(inl(REG_GPIOE_DOUT) & ~(1 << 1), REG_GPIOE_DOUT);	// switch GGPIOE_1

				}
				else
				{
	                //printk("headset plugged out!!\n");		        						
	                earphone = 0;
					outl(inl(REG_GPIOE_DOUT) | (1 << 1), REG_GPIOE_DOUT);	// switch GGPIOE_1
				}										
				
	        } 
	        else 
	        {
				if(earphone == 0)
					return;

		      	mdelay(30);		
	        	val = inl(REG_GPIOE_PIN);		      	
		        if (val & 0x001) 		// GPIOE_0
		        {			
	                //printk("headset plugged in!!\n");		        	
	                earphone = 1;
					outl(inl(REG_GPIOE_DOUT) & ~(1 << 1), REG_GPIOE_DOUT);	// switch GGPIOE_1
				
				}
				else
				{
	                //printk("headset plugged out!!\n");		        						
	                earphone = 0;
					outl(inl(REG_GPIOE_DOUT) | (1 << 1), REG_GPIOE_DOUT);	// switch GGPIOE_1
				}										
	    	}
	    	
	#elif defined (CONFIG_HEADSET_GPD3_AND_SPEAKER_GPD4)			
	
	        //detect headset is plugged in or not
	        val = inl(REG_GPIOD_PIN);
	
	        //disable/enable speaker
	        if (val & 0x0008) 		// GPIOD_3
	        {			
				if(earphone == 0)
					return;

				outl(0x03, REG_SPU_CH_PAUSE);	
				
		      	mdelay(30);		
	        	val = inl(REG_GPIOD_PIN);		      	
		        if (val & 0x0008) 		// GPIOD_3
		        {			
	                //printk("headset plugged out!!\n");		        								        	
		        	outl(inl(REG_GPIOD_DOUT)|(0x0010), REG_GPIOD_DOUT); 	// GPD4 = high        		                
	                earphone = 0;
	                
	                
		#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                
				//	spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel	
					if (s_PlayChannels == 1) {
						spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
					else if (s_PlayChannels == 2) {
						spu_SetPAN(1, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
		#endif					
				}
				else
				{
	                //printk("headset plugged in!!\n");		        	
		        	outl(inl(REG_GPIOD_DOUT)&(~0x0010), REG_GPIOD_DOUT); 	// GPD4 = low        		                	                
	                earphone = 1;

		#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                
				//	spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
					if (s_PlayChannels == 1) {
						spu_SetPAN(0, 0x1F1F);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
					else if (s_PlayChannels == 2) {					
						spu_SetPAN(1, 0x001F);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
					}
		#endif					
				}										
				
	        } 
	        else 
	        {
				if(earphone == 1)
					return;

				outl(0x03, REG_SPU_CH_PAUSE);
				
		      	mdelay(30);		
	        	val = inl(REG_GPIOD_PIN);		      	
		        if (val & 0x0008) 		// GPIOD_3
		        {			
	                //printk("headset plugged out!!\n");		        								        	
		        	outl(inl(REG_GPIOD_DOUT)|(0x0010), REG_GPIOD_DOUT); 	// GPD4 = high        		                	                
	                earphone = 0;
	                
		#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                
				//	spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel	
	
					if (s_PlayChannels == 1) {
						spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
					else if (s_PlayChannels == 2) {
						spu_SetPAN(1, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
		#endif					
				}
				else
				{
	                //printk("headset plugged in!!\n");		        	
		        	outl(inl(REG_GPIOD_DOUT)&(~0x0010), REG_GPIOD_DOUT); 	// GPD4 = low        		                	                	                
	                earphone = 1;
	                
		#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                
				//	spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
					if (s_PlayChannels == 1) {
						spu_SetPAN(0, 0x1F1F);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
					}
					else if (s_PlayChannels == 2) {					
						spu_SetPAN(1, 0x001F);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
					}
		#endif					
				}										
	    	}
	
			outl(0x00, REG_SPU_CH_PAUSE);
	#endif			    	
	}


	int get_headset_status(void)
	{
	        return(earphone);
	}


	
#endif


//unplayed buffer size(data bytes in buffer)
static int wb_audio_check_dmabuf(void)

{
	unsigned int remainDmaByte;
	unsigned int curDmaPhyAddr;
	
	
//	DBG("u32Channel = 0x%x\n", u32Channel);
//	DBG("curDmaPhyAddr = 0x%x\n", curDmaPhyAddr);
	
	
	if(!(audio_dev.state & AU_STATE_PLAYING))	
		return 0;
	
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				
	curDmaPhyAddr = spu_GetCurrentAddress(0);
#endif	

#ifdef CONFIG_SOUND_W55FA93_PLAY_NAU8822
	curDmaPhyAddr = AUDIO_READ(REG_I2S_ACTL_PDSTC);
#endif

	
//	printk("----- curDmaPhyAddr = 0x%x\n", curDmaPhyAddr);
//	printk("----- dspwriteptr = 0x%x\n", dspwriteptr);		


	//buffer is underflow, set write pointer is the same with current DMA pointer plus "first_length"
	if(dspwriteptr <= 0)	
	{	

//		printk("===== fragment_size = 0x%x\n", fragment_size);			
//		printk("===== phaddrplay = 0x%x\n", phaddrplay);				
//		printk("===== dspwriteptr = 0x%x, < 0\n", dspwriteptr);							
				
		dspwriteptr = fragment_size;
		if(curDmaPhyAddr > phaddrplay + fragment_size)
			dmawrbuf = audio_dev.play_buf_length;
		else
			dmawrbuf = fragment_size;		
			
//		printk("===== dmawrbuf = 0x%x\n", dmawrbuf);							
		
		return (phaddrplay + dmawrbuf - curDmaPhyAddr);
	}
		
//	printk("----- dmawrbuf = 0x%x\n", dmawrbuf);									
//	printk("----- phaddrplay = 0x%x\n", phaddrplay);										
	
	if(curDmaPhyAddr > phaddrplay + dmawrbuf)
		remainDmaByte = (phaddrplay + audio_dev.play_buf_length - curDmaPhyAddr) + dmawrbuf;
	else
		remainDmaByte = (phaddrplay  + dmawrbuf) - curDmaPhyAddr;
	
//	printk("##### remainDmaByte = 0x%x\n", remainDmaByte);											
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				

	#ifdef OPT_ADD_PAUSE
		if (inl(REG_SPU_CH_PAUSE) & 0x03)
		{
			dspwriteptr = 0;
			dmawrbuf = curDmaPhyAddr - phaddrplay;
			remainDmaByte = 0;
		}		
	#endif
#endif	
		
	return remainDmaByte;
}


static int wb_audio_copy_pcm_from_user(char *data, int length)
{
	//mutex_lock(&buf_mutex);	
	if(dmawrbuf+length > audio_dev.play_buf_length)
	{
		int len = audio_dev.play_buf_length - dmawrbuf;

		copy_from_user((char *)(audio_dev.play_buf_addr + dmawrbuf), data, len);
		data+=len;

		len = length + dmawrbuf - audio_dev.play_buf_length;
		copy_from_user((char *)(audio_dev.play_buf_addr), data, len);		

#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			
	#ifdef OPT_ADD_PAUSE
		spu_SetPauseAddress(0,  phaddrplay + len);

		if (inl(REG_SPU_CH_PAUSE) & 0x03)
		{
	        printk("PAUSE is encountered_2 !!! \n");			
		//	outl(0x00, REG_SPU_CH_PAUSE);	        
		}	        
	#endif		
#endif		
		
		
		dmawrbuf = len;
	}
	else
	{
		copy_from_user((char *)(audio_dev.play_buf_addr + dmawrbuf), data, length);
		dmawrbuf += length;
		
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			

	#ifdef OPT_ADD_PAUSE
		if (dmawrbuf == audio_dev.play_buf_length)
			spu_SetPauseAddress(0,  phaddrplay + dmawrbuf);			
		else			
			spu_SetPauseAddress(0,  phaddrplay + dmawrbuf);

		if (inl(REG_SPU_CH_PAUSE) & 0x03)
		{
	        printk("PAUSE is encountered_3 !!! \n");			
		//	outl(0x00, REG_SPU_CH_PAUSE);	        
		}	        
	#endif		
#endif		
		
	}

	//mutex_unlock(&buf_mutex);	

}

static void wb_audio_stop_play(void)
{
	DBG("Stop Play! \n");

	if ( audio_dev.state & AU_STATE_PLAYING){
		audio_dev.play_codec->stop_play();
		audio_dev.state &= ~AU_STATE_PLAYING;
		wake_up_interruptible((wait_queue_head_t *)&audio_dev.write_wait_queue);
				
		dmawrbuf = 0;		
	}
}

static int play_callback(u32 uAddr, u32 uLen)
{
	ENTER();
	
	dspwriteptr -= fragment_size;
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			
	#ifdef OPT_ADD_PAUSE
		if (inl(REG_SPU_CH_PAUSE) & 0x03)
		{
			if (dspwriteptr)
   				printk("PASUE is encountered but dspwriteptr NOT equal to 0 !!! \n");								
   				
			dspwriteptr = 0;		
		}			
			
	#endif		
#endif
	if(dspwriteptr < 0)
	{
		dspwriteptr = 0;
   		printk("dspwriteptr < 0 !!! \n");							
	}		
	

#ifdef OPT_ADD_PAUSE
	if(0)
#else	
	if(enableFillSilence)
#endif
	{
		DBG("enable File Silence !!!\n");			
		
		if(dspwriteptr <= fragment_size)	//prepare silence data
		{		
			silence_len = (dspwriteptr < fragment_size) ? audio_dev.play_buf_length - dspwriteptr : 64;
			//mutex_lock(&buf_mutex);			
			if(dmawrbuf+silence_len > audio_dev.play_buf_length)
			{
				int len = audio_dev.play_buf_length - dmawrbuf;
				memset((char *)(audio_dev.play_buf_addr + dmawrbuf), 0x00, len);
				len = silence_len + dmawrbuf - audio_dev.play_buf_length;
				memset((char *)(audio_dev.play_buf_addr), 0x00, len);
			}
			else
			{
				memset((char *)(audio_dev.play_buf_addr + dmawrbuf), 0x00, silence_len);
			}
			
			//mutex_unlock(&buf_mutex);
			if(dspwriteptr < fragment_size)
				enableFillSilence = 0;
		}		
	}
	wake_up_interruptible(&audio_dev.write_wait_queue);	/* wake up all block write system call */
	DBG("%d\n", dspwriteptr);	
	
	LEAVE();
	return 0;
}

static int wb_audio_start_play(void)
{
	int ret = 0;
  	
  	ENTER();
	DBG("Start Playing ... \n");

	if ( audio_dev.state & AU_STATE_PLAYING)	/* playing? */
		return 0;

	audio_dev.state |= AU_STATE_PLAYING;
	
	if ( audio_dev.play_codec->start_play(play_callback, 
								audio_dev.nPlaySamplingRate,
								audio_dev.nPlayChannels)) {
		audio_dev.state &= ~AU_STATE_PLAYING;
		DBG("Play error\n");

		ret = -EIO;
	}
	s_PlayChannels = audio_dev.nPlayChannels;	// added by MHKuo

	LEAVE();

	return ret;
}

//*******************Record**********************//
static void wb_audio_stop_record(void)
{
	DBG("Stop Record!\n");
	
	if ( audio_dev.state & AU_STATE_RECORDING){
		//audio_dev.record_codec->set_record_volume(0, 0);
		audio_dev.record_codec->stop_record();
		audio_dev.state &= ~AU_STATE_RECORDING;
		wake_up_interruptible((wait_queue_head_t *)&audio_dev.read_wait_queue);
	}
}

static int record_callback(UINT32 uAddr, UINT32 uLen)
{
	int i = 1;

	if(uAddr == phaddrrecord)
		i = 0;
		
	DBG("record_callback!\n");
	audio_dev.record_half_buf[i].ptr =0;		/* indicate read from buffer[0] */

	wake_up_interruptible((wait_queue_head_t *)&audio_dev.read_wait_queue);	/* wake up all blocked read system call */

	if ( audio_dev.record_half_buf[i ^ 1].ptr == 0) {	/* last block wan't take off , user may stop record */
		wb_audio_stop_record();
		return 1;
	}
	else
		return 0;
	
}

static int wb_audio_start_record(void)
{
	int ret=0;

	DBG("Start Recording ... \n");

	if (audio_dev.state & AU_STATE_RECORDING)
		return 0;

	//if (audio_dev.state & AU_STATE_PLAYING) {
	//	if (!(audio_dev.record_codec->get_capacity() & AU_CAP_DUPLEX))
	//		wb_audio_stop_record();
	//}

#ifdef OPT_SET_RECORD_FRAGMENT
	audio_dev.record_half_buf[0].ptr = r_fragment_size;//FRAG_SIZE;
	audio_dev.record_half_buf[1].ptr = r_fragment_size;//FRAG_SIZE;
#else
	audio_dev.record_half_buf[0].ptr = fragment_size;//FRAG_SIZE;
	audio_dev.record_half_buf[1].ptr = fragment_size;//FRAG_SIZE;
#endif
	audio_dev.state |= AU_STATE_RECORDING;

	if ( audio_dev.record_codec->start_record((AU_CB_FUN_T)record_callback,
								audio_dev.nRecSamplingRate,
								audio_dev.nRecChannels)) {
		audio_dev.state &= ~AU_STATE_RECORDING;
		ret = -EIO;
	}
#ifdef CONFIG_SOUND_W55FA93_RECORD_ADC
	w55fa93_edma_trigger(edma_channel);
#endif	
	
	return ret;
}

#ifdef OPT_SET_RECORD_FRAGMENT
	static int wb_audio_set_fragment(struct inode *inode, int frag)
	{
		int bytes, count, size;
		int minor = MINOR(inode->i_rdev);		

		//printk("frag = 0x%x !!! \n", frag);		
		
		bytes = frag & 0xffff;
		count = (frag >> 16) & 0x7fff;
		
		if (bytes < 4 || bytes > 15)	/* <16 || > 32k */
			return -EINVAL;
			
		if (count != 2)
			return -EINVAL;
		
		size = 1 << bytes;	

//		printk("size = 0x%x !!! \n", size);				
//		printk("max_fragment_size = 0x%x !!! \n", max_fragment_size);				
//		printk("minor = 0x%x !!! \n", minor);				
//		printk("audio_dev.state = 0x%x !!! \n", audio_dev.state);						
//		printk("AU_STATE_RECORDING = 0x%x !!! \n", AU_STATE_RECORDING);						
//		printk("AU_STATE_PLAYING = 0x%x !!! \n", AU_STATE_PLAYING);										

						
		if (size > max_fragment_size)
			return -EINVAL;			
		
		if (minor == 3)			// for playing
		{
			if ( audio_dev.state & AU_STATE_PLAYING)	/* playing? */
				return -EINVAL;

		//	max_fragments = count;
			fragment_size = size;
			audio_dev.play_buf_length = fragment_size * max_fragments;
			audio_dev.play_codec->set_play_buffer(phaddrplay, audio_dev.play_buf_length);  //change for DMA	
			
//			printk("fragment_size = 0x%x !!! \n", fragment_size);									
//			printk("max_fragments = 0x%x !!! \n", max_fragments);												
		}
		else if (minor == 19)	// for recording
		{
			if ( audio_dev.state & AU_STATE_RECORDING)	/* recording? */
				return -EINVAL;
			
		//	r_max_fragments = count;			
			r_fragment_size = size;
			audio_dev.record_buf_length = r_fragment_size * r_max_fragments;
			audio_dev.record_codec->set_record_buffer(phaddrrecord, audio_dev.record_buf_length);  //change for DMA	
			
//			printk("r_fragment_size = 0x%x !!! \n", r_fragment_size);									
//			printk("r_max_fragments = 0x%x !!! \n", r_max_fragments);												
		}						
		
		return 0;
	}
#else
static int wb_audio_set_fragment(int frag)
{
	int bytes, count;
	
	if ( audio_dev.state & AU_STATE_PLAYING)	/* playing? */
		return -EINVAL;
	
	bytes = frag & 0xffff;
	count = (frag >> 16) & 0x7fff;
	
	if (bytes < 4 || bytes > 15)	/* <16 || > 32k */
		return -EINVAL;

	if (count < 2)
		return -EINVAL;
	
	max_fragments = count;
	fragment_size = 1 << bytes;
	audio_dev.play_buf_length = fragment_size * max_fragments;

	audio_dev.play_codec->set_play_buffer(phaddrplay, audio_dev.play_buf_length);  //change for DMA	
	return bytes | ((count - 1) << 16);	
}
#endif

static int wb_mixer_open(struct inode *inode, struct file *file)
{
	int retval;
	
	int minor = MINOR(inode->i_rdev);

	ENTER();

	if(minor != 0 && minor != 16)		// mixer0(0) device: playing; mixer1(16) device: recording
		return -ENODEV;

	if(minor == 16)
		minor = 1;

	down(&audio_dev.mixer_sem);

	retval = -EBUSY;

	if(minor == 0)	//mixer0 for playing
	{
		if(audio_dev.open_play_flags != 0){
			if(audio_dev.mixer_play_openflag != 0)
				goto quit;
			else{
				if(audio_dev.dsp_play_openflag != 0 && audio_dev.dsp_play_dev != minor)
					goto quit;
			}
		}
		
		audio_dev.open_play_flags = 1;
		audio_dev.mixer_play_openflag = 44;
		audio_dev.mixer_play_dev = minor;
		
	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU		
		audio_dev.play_codec = &nv_spu_play_codec;
	#else
		audio_dev.play_codec = &nv_i2s_play_codec;	
	#endif				
		
	}
	
#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501) || defined(CONFIG_SOUND_W55FA93_RECORD_ADC)	
	else if(minor == 1)	//mixer1 for recording
	{
		if(audio_dev.open_record_flags != 0){
			if(audio_dev.mixer_record_openflag != 0)
				goto quit;
			else{
				if(audio_dev.dsp_record_openflag != 0 && audio_dev.dsp_record_dev != minor)
					goto quit;
			}
		}
		
		audio_dev.open_record_flags = 1;
		audio_dev.mixer_record_openflag = 44;
		audio_dev.mixer_record_dev = minor;
	#if defined(CONFIG_SOUND_W55FA93_RECORD_ADC)		
		audio_dev.record_codec = &nv_adc_record_codec;
	#else
		audio_dev.record_codec = &nv_i2s_record_codec;	
	#endif
	}
#endif	
	else
		goto quit;
	
	MSG2("Mixer[%d] opened\n", minor);
		
	retval = 0;

quit:
	up(&audio_dev.mixer_sem);

	LEAVE();
	
	return retval;
}

static int wb_mixer_release(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	
	if(minor == 16)
		minor = 1;
	
	if(minor == 0)	//mixer0 for playing
	{
		audio_dev.mixer_play_openflag = 0;
		if(audio_dev.dsp_play_openflag == 0)
			audio_dev.open_play_flags = 0;
	}		
	else if(minor == 1)	//mixer1 for recording
	{
		audio_dev.mixer_record_openflag = 0;
		if(audio_dev.dsp_record_openflag == 0)
			audio_dev.open_record_flags = 0;
		
	}		

	return 0;
}

static int saveVolume=WB_AUDIO_DEFAULT_VOLUME;

static int wb_mixer_ioctl(struct inode *inode, struct file *file,
			       unsigned int cmd, unsigned long arg)
{
	int ret = 0, val=0, err = 0;
	int tmpVolumeLeft, tmpVolumeRight;
		
	ENTER();
	if (cmd == SOUND_MIXER_INFO) {
		mixer_info info;
		memset(&info,0,sizeof(info));
              strncpy(info.id,"w55fa93",sizeof(info.id)-1);
              strncpy(info.name,"Nuvoton w55fa93 Audio",sizeof(info.name)-1);
              info.modify_counter = 0;
              if (copy_to_user((void *)arg, &info, sizeof(info)))
                      return -EFAULT;
		return 0;
	}
	if (cmd == SOUND_OLD_MIXER_INFO) {
		_old_mixer_info info;
		memset(&info,0,sizeof(info));
              strncpy(info.id,"w55fa93",sizeof(info.id)-1);
              strncpy(info.name,"Nuvoton w55fa93 Audio",sizeof(info.name)-1);
              if (copy_to_user((void *)arg, &info, sizeof(info)))
                      return -EFAULT;
		return 0;
	}
	if (cmd == OSS_GETVERSION)
		return put_user(SOUND_VERSION, (int *)arg);

	/* read */
	if (_SIOC_DIR(cmd) & _SIOC_WRITE)
		if (get_user(val, (int *)arg))
			return -EFAULT;

	switch (cmd) {
		case MIXER_READ(SOUND_MIXER_CAPS):
			ret = SOUND_CAP_EXCL_INPUT;		/* only one input can be selected */
			break;
		case MIXER_READ(SOUND_MIXER_STEREODEVS):
			ret = 1;									/* check whether support stereo */
			break;
		
		case MIXER_READ(SOUND_MIXER_RECMASK):
			ret = SOUND_MASK_MIC;
		case MIXER_READ(SOUND_MIXER_DEVMASK):		/* get all channels mask */
				/* get input channels mask */
			ret |= SOUND_MASK_LINE | SOUND_MASK_CD | SOUND_MASK_LINE1 | SOUND_MASK_VOLUME | SOUND_MASK_PCM;
			break;
				
		case MIXER_WRITE(SOUND_MIXER_VOLUME): 
		case MIXER_WRITE(SOUND_MIXER_PCM): //0~100 -->n
			tmpVolumeLeft = (val & 0xff) * 63  / 100;
			tmpVolumeRight = ((val >> 8) & 0xff) * 63 / 100;
			//tmpVolumeLeft = (val & 0xff) * 31  / 100;
			//tmpVolumeRight = ((val >> 8) & 0xff) * 31 / 100;
			audio_dev.play_codec->set_play_volume(tmpVolumeLeft, tmpVolumeRight);
			//audio_dev.play_codec->set_play_volume(val, val);
			audio_dev.nPlayVolumeLeft = tmpVolumeLeft;
			audio_dev.nPlayVolumeRight = tmpVolumeRight;
			ret = (val & 0xffff);			
			saveVolume = val;
			break;
			
		case MIXER_WRITE(SOUND_MIXER_MIC): //0~100 -->n
			tmpVolumeLeft = (val & 0xff) * 31  / 100;
			tmpVolumeRight = ((val >> 8) & 0xff) * 31 / 100;
		
			DBG("tmpVolumeLeft = %d\n", tmpVolumeLeft);				
			DBG("tmpVolumeRight = %d\n", tmpVolumeRight);							
			audio_dev.record_codec->set_record_volume(tmpVolumeLeft, tmpVolumeRight);			
			s_IsDefaultVolume = FALSE;
			break;
							
		case MIXER_READ(SOUND_MIXER_VOLUME): //n-->0~100(fixed)
		case MIXER_READ(SOUND_MIXER_PCM):
		
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU	

			if (s_IsDefaultVolume == TRUE)
			{
				s_IsDefaultVolume = FALSE;
				saveVolume = spuGetPcmVolume() * 100 / 63;
			}				
#endif
			ret = saveVolume;
			
			DBG("default volume is %d\n", saveVolume);							
			break;	
		
		default:			
			return -EINVAL;
	}
		
	if (put_user(ret, (int *)arg))
		return -EFAULT;

	LEAVE();
	return err;
}


static struct file_operations wb_mixer_fops = {
	owner:	THIS_MODULE,
	ioctl:	wb_mixer_ioctl,
	open:	wb_mixer_open,
	release:	wb_mixer_release,
};

static irqreturn_t wb_play_irq(int irq, void *dev_id, struct pt_regs * regs)
{
	ENTER();

	if( audio_dev.state & AU_STATE_PLAYING)
	audio_dev.play_codec->play_interrupt();
	
	LEAVE();

	return IRQ_HANDLED;
}

//for AC97/iis
static irqreturn_t wb_dsp_irq(int irq, void *dev_id, struct pt_regs * regs)
{
	ENTER();

#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501)|| defined(CONFIG_SOUND_W55FA93_PLAY_NAU8822)
	int status = AUDIO_READ(REG_I2S_ACTL_CON);

//	printk("status=%x\n", status);
	
	if(status & P_DMA_IRQ)
		audio_dev.play_codec->play_interrupt();
	
	if(status & R_DMA_IRQ)
		audio_dev.record_codec->record_interrupt();

/*
	if( audio_dev.state & AU_STATE_PLAYING)
		audio_dev.play_codec->play_interrupt();
	if(audio_dev.state & AU_STATE_RECORDING)
		audio_dev.record_codec->record_interrupt();
*/		
#endif
#ifdef CONFIG_SOUND_W55FA93_RECORD_ADC
	audio_dev.record_codec->record_interrupt();
#endif
	LEAVE();

	return IRQ_HANDLED;
}

#ifdef CONFIG_SOUND_W55FA93_RECORD_ADC
static adc_edma_irq_handler(int IntEdmaType)
{
	//printk("EDMA type = %d", IntEdmaType);
	adc_edma_isr_type(IntEdmaType);	
	audio_dev.record_codec->record_interrupt();
	
}
int initEDMA(void)
{
	int i, ret;
	ENTER();
#if 1
	i = w55fa93_pdma_find_and_request(CLIENT_ADC_NAME); //w55fa93_edma_request
#else
	for (i = 4; i >=1; i--)
	{
		if (!w55fa93_edma_request(i, CLIENT_ADC_NAME))
			break;
	}
	printk("EDMA channel for ADC %d\n", i);
#endif

	if(i == -ENODEV)
		return -ENODEV;

	edma_channel = i;
	w55fa93_edma_setAPB(edma_channel,			//int channel, 
						eDRVEDMA_ADC,			//E_DRVEDMA_APB_DEVICE eDevice, 
						eDRVEDMA_READ_APB,		//E_DRVEDMA_APB_RW eRWAPB, 
						eDRVEDMA_WIDTH_32BITS);	//E_DRVEDMA_TRANSFER_WIDTH eTransferWidth	

	w55fa93_edma_setup_handlers(edma_channel, 		//int channel
						eDRVEDMA_WAR, 			//int interrupt,	
						adc_edma_irq_handler, 		//void (*irq_handler) (void *),
						NULL);					//void *data

	w55fa93_edma_set_wrapINTtype(edma_channel , 
								eDRVEDMA_WRAPAROUND_EMPTY | 
								eDRVEDMA_WRAPAROUND_HALF);	//int channel, WR int type

	w55fa93_edma_set_direction(edma_channel , eDRVEDMA_DIRECTION_FIXED, eDRVEDMA_DIRECTION_WRAPAROUND);


	w55fa93_edma_setup_single(edma_channel,		// int channel, 
								0xB800E020,		// unsigned int src_addr,  (ADC data port physical address) 
								phaddrrecord,		// unsigned int dest_addr,
								fragment_size*2);	// unsigned int dma_length /* Lenth equal 2 half buffer */

}
void releaseEDMA(void)
{
	if(edma_channel!=0)
				w55fa93_edma_free(edma_channel);
}

#endif
static int wb_dsp_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	int retval = -EBUSY;

	ENTER();

	if(minor != 3 && minor != 19){			// dsp0(3) device: playing; dsp1(19) device: recording
		MSG2("Minor error : %d\n", minor);
		return -ENODEV;
	}


	if(minor == 3)		//dsp0 is for playing
	{	
		minor = 0;	//dsp0
		
		if(audio_dev.open_play_flags != 0){
			if(audio_dev.dsp_play_openflag != 0)
				goto quit;
			else{
				if(audio_dev.mixer_play_openflag != 0 && audio_dev.mixer_play_dev != minor)
					goto quit;
			}
		}

		
	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU
		audio_dev.play_codec = &nv_spu_play_codec;
		audio_dev.play_buf_addr = dma_play_buf_addr;
		if((retval = request_irq(IRQ_SPU, wb_play_irq, SA_INTERRUPT, "wb audio", NULL))){
			DBG("wb_audio_init : Request IRQ error\n");
			goto quit;
		}
		
	#else
		
		#if defined(CONFIG_SOUND_W55FA93_PLAY_NAU8822)
			audio_dev.play_codec = &nv_i2s_play_codec;
			audio_dev.play_buf_addr = dma_play_buf_addr;
			if(audio_dev.ac97iis_irq == 0)
			{
				if((retval = request_irq(IRQ_I2S, wb_dsp_irq, SA_INTERRUPT, "wb audio", NULL))){
					DBG("wb_audio_init : Request IRQ error\n");
					goto quit;
				}
				audio_dev.ac97iis_irq = 1;	
			}
		#endif
	#endif
		
		audio_dev.play_buf_addr = dma_play_buf_addr;
		
		if(audio_dev.play_buf_addr == (unsigned int)NULL ){
			free_irq(IRQ_SPU, NULL);
			DBG("Not enough memory\n");
			return -ENOMEM;
		}
//		printk("=x=>%x %x %x\n", fragment_size, max_fragments, fragment_size*max_fragments);
		audio_dev.play_buf_length = fragment_size * max_fragments/*( PAGE_SIZE << AUDIO_BUFFER_ORDER )*/;

	#if defined(CONFIG_SPU_BUF_8K)
		memset(audio_dev.play_buf_addr, 0, 16*1024);
	#else
		memset(audio_dev.play_buf_addr, 0, 64*1024);
	#endif

	
		MSG2("Audio_Dev.play_buf_addr : %x, Length: %x\n", 
			audio_dev.play_buf_addr, audio_dev.play_buf_length);
		
		init_waitqueue_head((wait_queue_head_t *)&audio_dev.read_wait_queue);		
		init_waitqueue_head((wait_queue_head_t *)&audio_dev.write_wait_queue);	//
		
		//add if the buffer is big enough, clean more data for silence
		//silence_len = (fragment_size > 8192) ? fragment_size : audio_dev.play_buf_length;
		//silence_len = audio_dev.play_buf_length;
		//enable the FillSilence feature just one time per playing
		enableFillSilence = 1;
		
//		audio_dev.nPlayVolumeLeft = WB_AUDIO_DEFAULT_VOLUME;
//		audio_dev.nPlayVolumeRight = WB_AUDIO_DEFAULT_VOLUME;
				
		/* set dma buffer */
		audio_dev.play_codec->set_play_buffer(phaddrplay, audio_dev.play_buf_length);  //change for DMA
		audio_dev.play_codec->init_play();

		audio_dev.open_play_flags = 1;
		audio_dev.state = AU_STATE_NOP;
		audio_dev.dsp_play_openflag = 1;
		audio_dev.dsp_play_dev = minor;
//		audio_dev.nPlaySamplingRate = WB_AUDIO_DEFAULT_SAMPLERATE;
//		audio_dev.nPlayChannels = WB_AUDIO_DEFAULT_CHANNEL;);		

	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU
//		outl((1<<IRQ_SPU), REG_AIC_MECR);		
	#else
//		outl((1<<IRQ_I2S), REG_AIC_MECR);			
	#endif		
		
	}
	else	//dsp1 for recording
	{
		minor = 1;	//dsp1		
//		printk("dsp_record_openflag = %d\n", audio_dev.dsp_record_openflag);						
		
		if(audio_dev.open_record_flags != 0){
			if(audio_dev.dsp_record_openflag != 0)
				goto quit;
			else{
				if(audio_dev.mixer_record_openflag != 0 && audio_dev.mixer_record_dev != minor)
					goto quit;
			}
		}
		
		#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501)
			audio_dev.record_codec = &nv_i2s_record_codec;
			if(audio_dev.ac97iis_irq == 0)
			{
				if((retval = request_irq(IRQ_I2S, wb_dsp_irq, SA_INTERRUPT, "wb audio", NULL))){
					printk("wb_audio_init : Request IRQ error\n");
					goto quit;
				}
				
				audio_dev.ac97iis_irq = 1;	
			}
					
			//audio_dev.record_buf_length =  PAGE_SIZE << AUDIO_BUFFER_ORDER;				
		#endif
		#if defined(CONFIG_SOUND_W55FA93_RECORD_ADC)			
			audio_dev.record_codec = &nv_adc_record_codec;	
			retval = initEDMA();	
			if(retval == -ENODEV)
			{
				printk("Not free PDMA channel\n");
				return -ENODEV;
			}		
		#endif	
		
		audio_dev.record_buf_addr = dma_rec_buf_addr;
		
		if(audio_dev.record_buf_addr == (unsigned int)NULL ){
#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501) 			
			free_irq(IRQ_I2S, NULL);
#elif defined(CONFIG_SOUND_W55FA93_RECORD_ADC)	
			releaseEDMA();
#endif			
			DBG("Not enough memory\n");
			return -ENOMEM;
		}
#ifdef OPT_SET_RECORD_FRAGMENT
		audio_dev.record_buf_length = r_fragment_size * r_max_fragments;
#else		
		#if defined(CONFIG_SPU_BUF_8K)
			audio_dev.record_buf_length = 16*1024;
		#else
			audio_dev.record_buf_length = 64*1024;
		#endif
#endif
		
	#if defined(CONFIG_SPU_BUF_8K)
		memset(audio_dev.record_buf_addr, 0, 16*1024);
	#else
		memset(audio_dev.record_buf_addr, 0, 64*1024);
	#endif
		
		MSG2("Audio_Dev.record_buf_addr : %x, Length: %x\n", 
			audio_dev.record_buf_addr, audio_dev.record_buf_length);		
			
		init_waitqueue_head((wait_queue_head_t *)&audio_dev.read_wait_queue);
		init_waitqueue_head((wait_queue_head_t *)&audio_dev.write_wait_queue);	//
		
//		audio_dev.nRecordVolumeLeft = WB_AUDIO_DEFAULT_VOLUME;
//		audio_dev.nRecordVolumeRight = WB_AUDIO_DEFAULT_VOLUME;	
				
		/* set dma buffer */
		audio_dev.record_codec->set_record_buffer(phaddrrecord, audio_dev.record_buf_length);  //change for DMA
		audio_dev.record_codec->init_record();		

		audio_dev.state = AU_STATE_NOP;		
		audio_dev.open_record_flags = 1;
		audio_dev.dsp_record_openflag = 1;
		audio_dev.dsp_record_dev = minor;
//		audio_dev.nRecSamplingRate = WB_AUDIO_DEFAULT_SAMPLERATE;
//		audio_dev.nRecChannels = WB_AUDIO_DEFAULT_CHANNEL;
//		printk("I2S recrod is ready !!!\n");						
#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501) || defined(CONFIG_SOUND_W55FA93_PLAY_NAU8822)

		printk("enable I2S interrupt !! \n");
		AUDIO_WRITE(REG_I2S_ACTL_CON, AUDIO_READ(REG_I2S_ACTL_CON) | R_DMA_IRQ_EN); 			
		
		outl((1<<IRQ_I2S), REG_AIC_MECR);			
#endif		
	}	

	LEAVE();

	return 0;

quit:

	MSG2("Open failed : %d\n", retval);

	return retval;
}

static int wb_dsp_release(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);

	ENTER();
	
	//if(audio_dev.state & AU_STATE_PLAYING){		//[fl] should not wait here
		/* wait until stop playing */		
	//	wait_event_interruptible(audio_dev.write_wait_queue,
	//							(audio_dev.state & AU_STATE_PLAYING)  == 0 ||
	//							(audio_dev.play_buf_length - wb_audio_check_dmabuf(_u8Channel0)) == 0);
	//}
	
	if(minor == 3)
	{
		if(audio_dev.dsp_play_openflag != 1)
			return 0;
		wb_audio_stop_play();
		dspwriteptr = 0;

	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU	
		free_irq(IRQ_SPU, NULL);
	#else		
		free_irq(IRQ_I2S, NULL);	
		audio_dev.ac97iis_irq = 0;
	#endif
	
		audio_dev.dsp_play_openflag = 0;
		if(audio_dev.mixer_play_openflag == 0)
			audio_dev.open_play_flags = 0;
	}
	else
	{
		if(audio_dev.dsp_record_openflag != 1)
			return 0;
			
		wb_audio_stop_record();
	#ifdef CONFIG_SOUND_W55FA93_RECORD_ADC 
		if(edma_channel != 0)
		{
			DBG("ADC free channel %d\n", edma_channel);
			w55fa93_adc_close();			//2011-0506
			w55fa93ts_open_again();		//2011-0506
			w55fa93_edma_free(edma_channel);
			edma_channel  = 0;
		}	
	#endif

	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU	
		free_irq(IRQ_SPU, NULL);
	#else		
		free_irq(IRQ_I2S, NULL);	
		audio_dev.ac97iis_irq = 0;
	#endif

		audio_dev.dsp_record_openflag = 0;
		if(audio_dev.mixer_record_openflag == 0)
			audio_dev.open_record_flags = 0;
	}
	LEAVE();
	
	return 0;
}

static ssize_t wb_dsp_read(struct file *file, char *buffer,
				size_t swcount, loff_t *ppos)
{
	int i, tmp, length, block_len, retval, bpos;
	char *dma_buf = (char *)audio_dev.record_buf_addr;  

	ENTER();

	if(swcount == 0)
		return 0;

	if(down_interruptible((struct semaphore *)&audio_dev.dsp_read_sem))
		return -ERESTARTSYS;

//org again:

	if((audio_dev.state & AU_STATE_RECORDING) == 0) {	/* if record not start, then start it */
		retval = wb_audio_start_record();
		if ( retval )
			goto quit;
	}

#ifdef OPT_SET_RECORD_FRAGMENT

	length = swcount;
	block_len = r_fragment_size;//FRAG_SIZE;
	retval = -EFAULT;
again:
	if(audio_dev.record_half_buf[0].ptr == r_fragment_size/*FRAG_SIZE*/ && 
	   audio_dev.record_half_buf[1].ptr == r_fragment_size/*FRAG_SIZE*/){	/* buffer empty */
		if( file->f_flags & O_NONBLOCK){
			retval = -EAGAIN;
			goto quit;
		}
		else {			
			wait_event_interruptible(audio_dev.read_wait_queue, 
					(audio_dev.state & AU_STATE_RECORDING) == 0 ||
					audio_dev.record_half_buf[0].ptr == 0 || 
					audio_dev.record_half_buf[1].ptr == 0 );			
			if ( (audio_dev.state & AU_STATE_RECORDING)  == 0){
				retval = 0;
				goto quit;
			}
		}

	}

#else

	length = swcount;
	block_len = fragment_size;//FRAG_SIZE;
	retval = -EFAULT;
again:
	if(audio_dev.record_half_buf[0].ptr == fragment_size/*FRAG_SIZE*/ && 
	   audio_dev.record_half_buf[1].ptr == fragment_size/*FRAG_SIZE*/){	/* buffer empty */
		if( file->f_flags & O_NONBLOCK){
			retval = -EAGAIN;
			goto quit;
		}
		else {			
			wait_event_interruptible(audio_dev.read_wait_queue, 
					(audio_dev.state & AU_STATE_RECORDING) == 0 ||
					audio_dev.record_half_buf[0].ptr == 0 || 
					audio_dev.record_half_buf[1].ptr == 0 );			
			if ( (audio_dev.state & AU_STATE_RECORDING)  == 0){
				retval = 0;
				goto quit;
			}
		}

	}
#endif
//printk("ccccc\n");
	//org retval = 0;
	bpos = 0;
	for(i = 0; i < 2; i++){
		tmp = block_len - audio_dev.record_half_buf[i].ptr;

		if(swcount < tmp)
			tmp = swcount;

		if(tmp){
			retval = -EFAULT;
			if(copy_to_user(buffer + bpos, 
				dma_buf + i * block_len + audio_dev.record_half_buf[i].ptr , tmp))
				goto quit;
		}
		else
			continue;

		swcount -= tmp;
		audio_dev.record_half_buf[i].ptr += tmp;
		bpos += tmp;

		if(swcount == 0)
			break;
	
	}


	retval = length - swcount;

#ifdef OPT_SET_RECORD_FRAGMENT
	if(swcount != 0){
		if( file->f_flags & O_NONBLOCK 
			&& audio_dev.record_half_buf[0].ptr == r_fragment_size/*FRAG_SIZE*/ 
			&& audio_dev.record_half_buf[1].ptr == r_fragment_size/*FRAG_SIZE*/)
			goto quit;
		
		buffer += retval;
		goto again;
	}

#else
	if(swcount != 0){
		if( file->f_flags & O_NONBLOCK 
			&& audio_dev.play_half_buf[0].ptr == fragment_size/*FRAG_SIZE*/ 
			&& audio_dev.play_half_buf[1].ptr == fragment_size/*FRAG_SIZE*/)
			goto quit;
		
		buffer += retval;
		goto again;
	}
#endif	

quit:

	up((struct semaphore *)&audio_dev.dsp_read_sem);

	LEAVE();

	return retval;

}

static ssize_t wb_dsp_write(struct file *file, const char *buffer,
				 size_t count, loff_t *ppos)
{
	int retval, length, buf_size, c;
	
	ENTER();

	if(count == 0)
		return 0;

	if(down_interruptible(&audio_dev.dsp_write_sem))
		return -ERESTARTSYS;
	
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU
	#ifdef OPT_ADD_PAUSE

		if((audio_dev.state & AU_STATE_PLAYING ) )
		{
			DrvSPU_SetPauseAddress(0,  phaddrplay + audio_dev.play_buf_length*3); 	// set pause address to un-reached address
	
			if (inl(REG_SPU_CH_PAUSE) & 0x03)
			{
			    printk("PAUSE is encountered_1 !!! \n");			
			//	outl(0x00, REG_SPU_CH_PAUSE);	        
			}	        
		}
	#endif	
#endif		
	length = count;
	
	retval = -EFAULT;
	
	while(length)
	{
		
		// don't check available DMA buffer size for writing
		c = length;
		//copy pcm to DMA buffer
		wb_audio_copy_pcm_from_user(buffer, c);
		
		length -= c;
		buffer += c;
		
		if((audio_dev.state & AU_STATE_PLAYING ) == 0
		//&& audio_dev.play_half_buf[0].ptr == FRAG_SIZE 	//[fl]once we have data, just play it(
		//&& audio_dev.play_half_buf[1].ptr == FRAG_SIZE
		){
			if (wb_audio_start_play()){
				retval =  -EIO;
				goto quit;
			}

	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			
		#ifdef OPT_ADD_PAUSE	
			if (inl(REG_SPU_CH_PAUSE) & 0x03)
			{
		        printk("PAUSE is encountered_4 !!! \n");			
				outl(0x00, REG_SPU_CH_PAUSE);	        
			}	
		#endif			        
	#endif			
		}	
		
		dspwriteptr += c;
		if (inl(REG_SPU_CH_PAUSE) & 0x03)
		{
	        printk("PAUSE is encountered_6 !!! \n");			
			outl(0x00, REG_SPU_CH_PAUSE);	        
		}	        
		
	}
	
quit:	
	up(&audio_dev.dsp_write_sem);
	
	LEAVE();
	
	return count;
	
}

static int wb_dsp_ioctl(struct inode *inode, struct file *file,
			     unsigned int cmd, unsigned long arg)
{
	int val = 0, i, err = 0;
	//for battery
	unsigned int ADC_CON_STO = 0, CLKDIV3_STO = 0,AGC_CON_STO = 0,
				AGCP1_STO = 0,AUDIO_CON_STO = 0,OPOC_STO = 0;
	int *adc_pointer = NULL,adc_data;

#ifdef 	CONFIG_SOUND_W55FA93_RECORD_ADC
	int sr;
#endif
	
	audio_buf_info info;
	
	int minor = MINOR(inode->i_rdev);

	ENTER();
	
	switch (cmd) {
	case SNDCTL_DSP_GETAIN2:
		print("ADC_SNDCTL_DSP_GETAIN2\n");
		//disable recorder*****************************************************
		ADC_CON_STO = inl(REG_ADC_CON);
		CLKDIV3_STO = inl(REG_CLKDIV3);
		AGC_CON_STO = inl(REG_AGC_CON);
		AGCP1_STO = 	inl(REG_AGCP1);
		AUDIO_CON_STO = inl(REG_AUDIO_CON);
		OPOC_STO	= inl(REG_OPOC);
		//print("REG_ADC_CON:%x,REG_CLKDIV3:%x,REG_AGC_CON:%x,AGCP1:%x,AUDIO_CON:%x,OPOC:%x\n",
		//		inl(REG_ADC_CON),inl(REG_CLKDIV3),inl(REG_AGC_CON),inl(REG_AGCP1),
		//		inl(REG_AUDIO_CON),inl(REG_OPOC));
		outl(inl(REG_AUDIO_CON) & ~(1 <<27),REG_AUDIO_CON);

		//configure and get data from AIN2**************************************
		outl(0x00,REG_ADC_XDATA);//clear XDATA
		//get battery data
		adc_pointer = (int*)arg;
		for( i = 0; i < 2; i++)
		{
			SET_NORMAL_AIN2;
			//wait
			while(  (inl(REG_ADC_CON) & ADC_INT) != ADC_INT )
			{
				print("waiting for ADC_INT\n");
				msleep(1);
			}
			//check memory access
			if(access_ok(VERIFY_WRITE,adc_pointer,sizeof(int)) == 0) //access_ok() 1 ok, 0 fail
			{
				print("can't access into user space\n");
				return -1;
			}
			//get data from ADC_XDATA (10 bit)
			adc_data = (int)inl(REG_ADC_XDATA);
			//sent data to user space
			printk("data_battery_drv2:%d\n",adc_data);
		}
		put_user(adc_data,adc_pointer);

		//enable recorder*******************************************************
		outl(OPOC_STO,REG_OPOC);
		outl(AGCP1_STO,REG_AGCP1);
		outl(AGC_CON_STO,REG_AGC_CON);
		outl(ADC_CON_STO,REG_ADC_CON);
		outl(AUDIO_CON_STO,REG_AUDIO_CON);

		//print("REG_ADC_CON:%x,REG_CLKDIV3:%x,REG_AGC_CON:%x,AGCP1:%x,AUDIO_CON:%x,OPOC:%x\n",
		//		inl(REG_ADC_CON),inl(REG_CLKDIV3),inl(REG_AGC_CON),inl(REG_AGCP1),
		//		inl(REG_AUDIO_CON),inl(REG_OPOC));

		return 0;
	case SNDCTL_DSP_GETAIN3:
		print("ADC_SNDCTL_DSP_GETAIN3\n");
		//disable recorder*****************************************************
		ADC_CON_STO = inl(REG_ADC_CON);
		CLKDIV3_STO = inl(REG_CLKDIV3);
		AGC_CON_STO = inl(REG_AGC_CON);
		AGCP1_STO = 	inl(REG_AGCP1);
		AUDIO_CON_STO = inl(REG_AUDIO_CON);
		OPOC_STO	= inl(REG_OPOC);
		//printk("REG_ADC_CON:%x,REG_CLKDIV3:%x,REG_AGC_CON:%x,AGCP1:%x,AUDIO_CON:%x,OPOC:%x\n",
		//		inl(REG_ADC_CON),inl(REG_CLKDIV3),inl(REG_AGC_CON),inl(REG_AGCP1),
		//		inl(REG_AUDIO_CON),inl(REG_OPOC));
		outl(inl(REG_AUDIO_CON) & ~(1 <<27),REG_AUDIO_CON);

		//configure and get data from AIN3**************************************
		outl(0x00,REG_ADC_XDATA);//clear XDATA
		//get battery data
		adc_pointer = (int*)arg;
		for( i = 0; i < 2; i++)
		{
			SET_NORMAL_AIN3;
			//wait
			while(  (inl(REG_ADC_CON) & ADC_INT) != ADC_INT )
			{
				print("waiting for ADC_INT\n");
				msleep(1);
			}
			//check memory access
			if(access_ok(VERIFY_WRITE,adc_pointer,sizeof(int)) == 0) //access_ok() 1 ok, 0 fail
			{
				print("can't access into user space\n");
				return -1;
			}
			//get data from ADC_XDATA (10 bit)
			adc_data = (int)inl(REG_ADC_XDATA);
			//sent data to user space
			printk("data_battery_drv3:%d\n",adc_data);
		}
		put_user(adc_data,adc_pointer);

		//enable recorder*******************************************************
		outl(OPOC_STO,REG_OPOC);
		outl(AGCP1_STO,REG_AGCP1);
		outl(AGC_CON_STO,REG_AGC_CON);
		outl(ADC_CON_STO,REG_ADC_CON);
		outl(AUDIO_CON_STO,REG_AUDIO_CON);

		//printk("REG_ADC_CON:%x,REG_CLKDIV3:%x,REG_AGC_CON:%x,AGCP1:%x,AUDIO_CON:%x,OPOC:%x\n",
		//		inl(REG_ADC_CON),inl(REG_CLKDIV3),inl(REG_AGC_CON),inl(REG_AGCP1),
		//		inl(REG_AUDIO_CON),inl(REG_OPOC));

	return 0;
//****************************************************************
	       case OSS_GETVERSION:
       	       val = SOUND_VERSION;

			break;
				
	       case SNDCTL_DSP_GETCAPS:
				if (audio_dev.play_codec->get_capacity() & AU_CAP_DUPLEX)	       
					val |= DSP_CAP_DUPLEX;
			
			   	val |= DSP_CAP_TRIGGER;

			break;

	       case SNDCTL_DSP_SPEED:
			if (get_user(val, (int*)arg))
				return -EFAULT;
				
#ifdef 	CONFIG_SOUND_W55FA93_RECORD_ADC
			sr = sizeof(nSamplingRate)/sizeof(unsigned int); 	
			if(minor == 3)
			{//playing	
				for(i = 0; i < sr; i++)
					if(val == nSamplingRate[i])
						break;
			}
			else
			{//recording
				//Recording from ADC
				sr = sizeof(nAdcSamplingRate)/sizeof(unsigned int);
				for(i = 0; i < sr; i++)
					if(val == nAdcSamplingRate[i])
						break;
			}						
			if(i >= sr)	/* not supported */
#else				//Recording from external codec			

			for(i = 0; i < sizeof(nSamplingRate)/sizeof(unsigned int); i++)
				if(val == nSamplingRate[i])
					break;

			if(i >= sizeof(nSamplingRate)/sizeof(unsigned int))	/* not supported */
#endif					
			{
				if(minor == 3)	//playing
					val = audio_dev.nPlaySamplingRate;
				else
					val = audio_dev.nRecSamplingRate;
				
				err = -EPERM;
			}
			else
			{
				if(minor == 3)	//playing
					audio_dev.nPlaySamplingRate = val;
				else
					audio_dev.nRecSamplingRate = val;
			}
			break;


	       case SOUND_PCM_READ_RATE:
				if(minor == 3)	//playing
					val = audio_dev.nPlaySamplingRate;
				else
					val = audio_dev.nRecSamplingRate;
				break;
			break;

	       case SNDCTL_DSP_STEREO:
			if (get_user(val, (int*)arg))
				return -EFAULT;

			if(minor == 3)	//playing
				audio_dev.nPlayChannels = val ? 2:1;
			else
				audio_dev.nRecChannels = val ? 2:1;
			break;	
		
	       case SNDCTL_DSP_CHANNELS:
			if (get_user(val, (int*)arg))
				return -EFAULT;

			if(val != 1 && val != 2){
				//val = audio_dev.nChannels;
				err = -EPERM;
			}

			if(minor == 3)	//playing
				audio_dev.nPlayChannels = val;
			else
				audio_dev.nRecChannels = val;
			break;

	       case SOUND_PCM_READ_CHANNELS:
			if(minor == 3)	//playing
				val = audio_dev.nPlayChannels;
			else
				val = audio_dev.nRecChannels;
			break;

	       case SNDCTL_DSP_SETFMT:
		   	if (get_user(val, (int*)arg))
				return -EFAULT;

			if ( (val & (AFMT_S16_LE)) == 0)
				err = -EPERM;
			
		case SNDCTL_DSP_GETFMTS:
			val =  (AFMT_S16_LE);		//support standard 16bit little endian format
			break;

	       case SOUND_PCM_READ_BITS:
			val = 16;
			break;

	       case SNDCTL_DSP_NONBLOCK:
       	       file->f_flags |= O_NONBLOCK;
			break;

	       case SNDCTL_DSP_RESET:
			wb_audio_stop_play();
						
			return 0;
		
	       case SNDCTL_DSP_GETBLKSIZE:
	       
#ifdef OPT_SET_RECORD_FRAGMENT
			if(minor == 3)	//playing
			{
				val = fragment_size/*FRAG_SIZE*/;
	        	printk("fragment_size = 0x%x\n", fragment_size);
			}				
			else
			{
				val = r_fragment_size/*FRAG_SIZE*/;				
	        	printk("r_fragment_size = 0x%x\n", r_fragment_size);
			}				
#else	       
			val = fragment_size/*FRAG_SIZE*/;
	        printk("fragment_size = 0x%x\n", fragment_size);
#endif						
			break;
				
	    case SNDCTL_DSP_SYNC:
	    {
			/* no data */
			int bytestogo = audio_dev.play_buf_length - wb_audio_check_dmabuf();
			if(bytestogo == 0)
				return 0;
		
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU					
	#ifdef OPT_ADD_PAUSE
							
		//	wait_event_interruptible(audio_dev.write_wait_queue,
		//							(audio_dev.state & AU_STATE_PLAYING)  == 0 ||
		//							bytestogo == 0 || (inl(REG_SPU_CH_PAUSE) & 0x03) != 0);
							
			while(!(inl(REG_SPU_CH_PAUSE) & 0x03));
			wb_audio_stop_play();									
	#else			
		
		//	wait_event_interruptible(audio_dev.write_wait_queue,
		//							(audio_dev.state & AU_STATE_PLAYING)  == 0 ||
		//							(audio_dev.play_buf_length - wb_audio_check_dmabuf(_u8Channel1)) == 0);

			while(wb_audio_check_dmabuf());
			wb_audio_stop_play();
	#endif			
#endif			

#ifdef CONFIG_SOUND_W55FA93_PLAY_NAU8822

		//	wait_event_interruptible(audio_dev.write_wait_queue,
		//							(audio_dev.state & AU_STATE_PLAYING)  == 0 ||
		//							(audio_dev.play_buf_length - wb_audio_check_dmabuf()) == 0);
			
			while(wb_audio_check_dmabuf());												
			wb_audio_stop_play();
#endif			

		   	return 0;
		}   	
		case SNDCTL_DSP_GETOSPACE:
		
		
			info.fragsize = fragment_size/*FRAG_SIZE*/;
			info.fragstotal = 2;			
			val = -1;

			if(dspwriteptr < 0 )
			{
				printk("dspwriteptr = 0x%x, < 0\n", dspwriteptr);													
				info.bytes = audio_dev.play_buf_length;
				printk("info.bytes = 0x%x, < 0\n", info.bytes);																	
			}				
			else
			{
				info.bytes = audio_dev.play_buf_length - dspwriteptr;
		}	

	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			
	
		#ifdef OPT_ADD_PAUSE
			if (inl(REG_SPU_CH_PAUSE))
			{
	        	if (!info.bytes)	// can't remove this one
	        	{
			        printk("PAUSE is encountered_5 !!! \n");			
			        printk("dspwriteptr = 0x%x \n", dspwriteptr);						        
			//		outl(0x00, REG_SPU_CH_PAUSE);	        
					info.bytes = audio_dev.play_buf_length;
					dspwriteptr = 0;		
					AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) | SPU_SWRST);
					AUDIO_WRITE(REG_SPU_CTRL, AUDIO_READ(REG_SPU_CTRL) & ~SPU_SWRST);	
				}	        		
	        	
			}	        	
		#endif			        	
	#endif			
	
			if(info.bytes == 0)
				info.fragments = 0;
			else
			{
				if(info.bytes > info.fragsize)
					info.fragments = 2;
				else
					info.fragments = 1;
			}
					
			if (copy_to_user((void *)arg, &info, sizeof(info)))
				return -EFAULT;
	
			return 0;
			
		case SNDCTL_DSP_GETODELAY:		//[fl]add for calculating played time
		{	
			/* return unplayed byte in DMA buffer */			
			int remainbytes = wb_audio_check_dmabuf();									
		
			//printk("<==remainbyte=%d\n", remainbytes);
			put_user(remainbytes, (int *)arg);
			return 0;			
		}
		
		case SNDCTL_DSP_GETTRIGGER:
			val = 0;
			if ( audio_dev.state & AU_STATE_PLAYING )
				val |= PCM_ENABLE_OUTPUT;			
			break;

		case SNDCTL_DSP_SETTRIGGER:
			if (get_user(val, (int*)arg))
				return -EFAULT;
			if ( val & PCM_ENABLE_OUTPUT){
				if ( wb_audio_start_play()){
					val &= ~PCM_ENABLE_OUTPUT;
					err = -EPERM ;
				}
			}
			else{
				wb_audio_stop_play();
			}

			break;
		
		case SNDCTL_DSP_SETFRAGMENT:
			if (get_user(val, (int*)arg))
				return -EFAULT;

#ifdef OPT_SET_RECORD_FRAGMENT
			if (wb_audio_set_fragment(inode, val))
				return -EFAULT;				
#else				
			wb_audio_set_fragment(val);	
#endif			
			
			break;
		
		//fake ioctl for flashlite project when need to add silence data in ISR
		case SNDCTL_MIDI_PRETIME:
			if (get_user(val, (int*)arg))
				return -EFAULT;
				
			enableFillSilence = val;
			break;
		default:
			return -EINVAL;
	}

	LEAVE();

	return err?-EPERM:put_user(val, (int *)arg);

}

static unsigned int wb_dsp_poll(struct file *file, struct poll_table_struct *wait)
{
	int mask = 0;

	ENTER();

	if (file->f_mode & FMODE_WRITE){
		poll_wait(file, &audio_dev.write_wait_queue, wait);
				
		if(audio_dev.play_buf_length - wb_audio_check_dmabuf() > 0)
			mask |= POLLOUT | POLLWRNORM;
	}
	
	if (file->f_mode & FMODE_READ){
		poll_wait(file, &audio_dev.read_wait_queue, wait);

		/* check if can read */
		if(audio_dev.record_half_buf[0].ptr !=  FRAG_SIZE )
			mask |= POLLIN | POLLRDNORM;
	}
	LEAVE();

	return mask;
}

static struct file_operations wb_dsp_fops = {
	owner:	THIS_MODULE,
	llseek:	no_llseek,
	read:	wb_dsp_read,
	write:	wb_dsp_write,
	poll:		wb_dsp_poll,
	ioctl:	wb_dsp_ioctl,
	open:	wb_dsp_open,
	release:	wb_dsp_release,
};

#if defined(CONFIG_SOUND_W55FA93_PLAY_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822)
extern void NAU8822_Init(void);
extern void NAU8822_Exit(void);
#endif

#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8501
extern void NAU8501_Init(void);
extern void NAU8501_Exit(void);
#endif

static int __init wb_dsp_init(void)
{
	int i;

	for(i=0;i<2;i++){
		dev_dsp[i] = register_sound_dsp(&wb_dsp_fops, i);
		MSG2("Dsp Device No : %d\n", dev_dsp[i]);
		if(dev_dsp[i]< 0)
			goto quit;
	}
	
#if defined(CONFIG_SOUND_W55FA93_PLAY_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822)
	NAU8822_Init();
#endif

#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8501
	NAU8501_Init();
#endif

	return 0;

quit:
	printk("Register DSP device failed\n");
	return -1;
}


static int __init wb_mixer_init(void)
{
	int i;

	for(i=0;i<2;i++){
		dev_mixer[i] = register_sound_mixer(&wb_mixer_fops, i);
		MSG2("Mixer Device No : %d\n", dev_mixer[i]);
		if(dev_mixer[i]< 0)
			goto quit;
	}

	return 0;

quit:
	printk("Register Mixer device failed\n");
	return -1;
}

static void wb_dsp_unload(void)
{
	int i;
	for(i=0;i<2;i++)
		unregister_sound_dsp(dev_dsp[i]);

	#if defined(CONFIG_SOUND_W55FA93_PLAY_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822)
		NAU8822_Exit();
	#endif
	
	#ifdef CONFIG_SOUND_W55FA93_RECORD_NAU8501
		NAU8501_Exit();
	#endif
}

static void wb_mixer_unload(void)
{
	int i;
	for(i=0;i<2;i++)
		unregister_sound_mixer(dev_mixer[i]);
}

#ifdef USE_DAC_ON_OFF_API	

#include <linux/platform_device.h>
static struct platform_device *sys_dac;

///api for dac power down procedures///////
//	static int dac_onoff_level = 2;
	void dac_on(void)
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
		//printk("%x\n",inl(REG_SPU_DAC_VOL)); 

		msleep(700);
	}
	
void dac_off(void)
{
	outl(inl(REG_SPU_CTRL) & ~SPU_EN, REG_SPU_CTRL);
	mdelay(1);
	
	outl(inl(REG_SPU_DAC_VOL) | 0x10000, REG_SPU_DAC_VOL);	//P0
	msleep(700);

	outl(inl(REG_SPU_DAC_VOL) | 0x200000, REG_SPU_DAC_VOL);	//P5
	mdelay(1);
				
	outl(inl(REG_SPU_DAC_VOL) | 0x1e0000, REG_SPU_DAC_VOL);	//P1-4
	mdelay(1);
	outl(inl(REG_SPU_DAC_VOL) | 0x400000, REG_SPU_DAC_VOL);	//P6
	mdelay(1);
	outl(inl(REG_SPU_DAC_VOL) | 0x800000, REG_SPU_DAC_VOL);	//P7
	//printk("%x\n",inl(REG_SPU_DAC_VOL));
	mdelay(1);
}

static ssize_t write_dac(struct device *dev, struct device_attribute *attr, const char *buffer, size_t count)
{
  
    if(buffer[0] == '0') {
    	printk("dac off\n"); 
		dac_off();
		dac_auto_config=0;
    } else if(buffer[0] == '1') {
    	printk("dac on\n");
		dac_on();
        dac_auto_config=0;
    } else if(buffer[0] == '2') {   
    	printk("dac auto\n");
    	dac_auto_config=1;
    } 

    return count;
}

/* Attach the sysfs write method */
DEVICE_ATTR(dac, 0644, NULL, write_dac);

/* Attribute Descriptor */
static struct attribute *dac_attrs[] = {
  &dev_attr_dac.attr,
  NULL
};

/* Attribute group */
static struct attribute_group dac_attr_group = {
  .attrs = dac_attrs,
};
/////////////////////////////////////////////
#endif


MODULE_AUTHOR("QFu");
MODULE_DESCRIPTION("Nuvoton w55fa93 Audio Driver");
MODULE_LICENSE("GPL");

static void wb_audio_exit (void)
{
	wb_mixer_unload();
	wb_dsp_unload();

	#ifdef CONFIG_HEADSET_ENABLED		
		free_irq(HEADSET_IRQ_NUM, NULL);
	#endif		
	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU	
		free_irq(IRQ_SPU, NULL);
	#endif		
	
	#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501) 	
		free_irq(IRQ_I2S, NULL);		
	#endif		
	
	#if defined(CONFIG_SPU_BUF_8K)	
		dma_free_writecombine(NULL, 16*1024, (dma_addr_t *)audio_dev.play_buf_addr , phaddrplay); 
		
		#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || \
			defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501) || \
			defined(CONFIG_SOUND_W55FA93_RECORD_ADC)
			dma_free_writecombine(NULL, 16*1024, (dma_addr_t *)audio_dev.record_buf_addr , phaddrrecord); 		
		#endif
	#else
		dma_free_writecombine(NULL, 64*1024, (dma_addr_t *)audio_dev.play_buf_addr , phaddrplay);

		#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || \
			defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501) || \
			defined(CONFIG_SOUND_W55FA93_RECORD_ADC)
			dma_free_writecombine(NULL, 64*1024, (dma_addr_t *)audio_dev.record_buf_addr , phaddrrecord); 		
		#endif
		
	#endif
}

static int __init wb_audio_init(void)
{
	//Disable_Int(IRQ_ACTL);
	AUDIO_WRITE(REG_AIC_MDCR,(1<<IRQ_SPU));

	/* enable audio enigne clock */
	AUDIO_WRITE(REG_AHBCLK, AUDIO_READ(REG_AHBCLK) | ADO_CKE);
	
#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU			
	AUDIO_WRITE(REG_AHBCLK, AUDIO_READ(REG_AHBCLK) | SPU_CKE);
#else
	AUDIO_WRITE(REG_AHBCLK, AUDIO_READ(REG_AHBCLK) | I2S_CKE);
#endif
	

	memset(&audio_dev, 0, sizeof(audio_dev));
	sema_init(& audio_dev.dsp_read_sem, 1);
	sema_init(&audio_dev.dsp_write_sem, 1);
	sema_init(&audio_dev.mixer_sem, 1);

	if(wb_dsp_init())
		goto quit;

	if(wb_mixer_init())
		goto quit;
	
	//mutex_init(&buf_mutex);
	
#ifdef USE_DAC_ON_OFF_API	
	outl((inl(REG_SPU_DAC_PAR) & ~POP_CON) | 0x10, REG_SPU_DAC_PAR);	//delay time, p0=0.5-1s
	
	printk("register dac on-off device\n");
    sys_dac = platform_device_register_simple("w55fa93-dac", -1, NULL, 0);    
	if(sys_dac == NULL)
    	printk("register dac on-off module failed\n");
	sysfs_create_group(&sys_dac->dev.kobj, &dac_attr_group);  
	
	dac_auto_config =1; //set to audo config by default	
#endif	
#if 0 // for W55FA93_AMP_CTRL test
        outl(inl(REG_GPIOE_OMD) | 2, REG_GPIOE_OMD);     
        outl(inl(REG_GPIOE_DOUT) | 2, REG_GPIOE_DOUT);
#endif

#ifdef 	CONFIG_W55FA93_INIT_DAC

#ifdef USE_DAC_ALWAYS_ON

	outl((inl(REG_SPU_DAC_PAR) & ~POP_CON) | 0x10, REG_SPU_DAC_PAR);	//delay time, p0=0.5-1s
	
	outl(inl(REG_SPU_DAC_VOL) & ~0x0800000, REG_SPU_DAC_VOL);	//P7	
	msleep(1);
	outl(inl(REG_SPU_DAC_VOL) & ~0x0400000, REG_SPU_DAC_VOL);	//P6
	msleep(1);
	outl(inl(REG_SPU_DAC_VOL) & ~0x01e0000, REG_SPU_DAC_VOL);	//P1-4
	msleep(1);
	outl(inl(REG_SPU_DAC_VOL) & ~0x0200000, REG_SPU_DAC_VOL);	//P5	
	msleep(1);
	
	outl(inl(REG_SPU_DAC_VOL) & ~0x00010000, REG_SPU_DAC_VOL);	//P0			
	//printk("%x\n",inl(REG_SPU_DAC_VOL)); 
	msleep(700);
	
#endif	// dac always on

#endif // init dac	

	#if defined(CONFIG_SPU_BUF_8K)
			dma_play_buf_addr = (unsigned int)dma_alloc_writecombine(NULL, (unsigned int)16*1024, (dma_addr_t *)&phaddrplay, GFP_KERNEL);
		
		#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || \
			defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501) || \
			defined(CONFIG_SOUND_W55FA93_RECORD_ADC)
			dma_rec_buf_addr = (unsigned int)dma_alloc_writecombine(NULL, (unsigned int)16*1024, (dma_addr_t *)&phaddrrecord, GFP_KERNEL);
		#endif
		
	#else
			dma_play_buf_addr = (unsigned int)dma_alloc_writecombine(NULL, (unsigned int)64*1024, (dma_addr_t *)&phaddrplay, GFP_KERNEL);
		
		#if defined(CONFIG_SOUND_W55FA93_RECORD_NAU8822) || \
			defined(CONFIG_SOUND_W55FA93_RECORD_NAU8501) || \
			defined(CONFIG_SOUND_W55FA93_RECORD_ADC)
			dma_rec_buf_addr = (unsigned int)dma_alloc_writecombine(NULL, (unsigned int)64*1024, (dma_addr_t *)&phaddrrecord, GFP_KERNEL);
		#endif
	#endif	// CONFIG_SPU_BUF_8K		

        audio_dev.nPlaySamplingRate = WB_AUDIO_DEFAULT_SAMPLERATE;
        audio_dev.nRecSamplingRate = WB_AUDIO_DEFAULT_SAMPLERATE;        
        audio_dev.nPlayChannels = WB_AUDIO_DEFAULT_CHANNEL;
        audio_dev.nRecChannels = WB_AUDIO_DEFAULT_CHANNEL;        
        audio_dev.nPlayVolumeLeft = WB_AUDIO_DEFAULT_VOLUME;
        audio_dev.nPlayVolumeRight = WB_AUDIO_DEFAULT_VOLUME;
        audio_dev.nRecordVolumeLeft = WB_AUDIO_DEFAULT_VOLUME;
        audio_dev.nRecordVolumeRight = WB_AUDIO_DEFAULT_VOLUME;

	printk("W55FA93 Audio driver has been initialized successfully!\n");
	
#ifdef CONFIG_HEADSET_ENABLED		

	#if defined (CONFIG_HEADSET_GPB2_AND_SPEAKER_GPB3)
	
        outl(inl(REG_GPBFUN) & ~MF_GPB2, REG_GPBFUN);				// headset detect plug IN/OUT
        outl(inl(REG_GPBFUN) & ~MF_GPB3, REG_GPBFUN);				// speaker control signal
        outl(inl(REG_GPIOB_OMD) & ~(0x0004), REG_GPIOB_OMD); 		// port B2 input
        outl(inl(REG_GPIOB_PUEN) | (0x0004), REG_GPIOB_PUEN); 		// port B2 pull-up
        
		outl(inl(REG_GPIOB_OMD)  | (0x0008), REG_GPIOB_OMD); 		// port B3 output
        outl(inl(REG_GPIOB_PUEN) | (0x0008), REG_GPIOB_PUEN); 		// port B3 pull-up

		if ( inl(REG_GPIOB_PIN) & 0x0004)	// headset plug_in
        	outl(inl(REG_GPIOB_DOUT) &~(0x0008), REG_GPIOB_DOUT);	// port B3 = low		

		else								// headset plug_out		
        	outl(inl(REG_GPIOB_DOUT) | (0x0008), REG_GPIOB_DOUT); 	// port B3 = high        	
               

        outl(inl(REG_IRQTGSRC0) & 0x00040000, REG_IRQTGSRC0);               
               
        outl((inl(REG_IRQSRCGPB) & ~(0x30)) | 0x20, REG_IRQSRCGPB); // port B2 as nIRQ2 source
        outl(inl(REG_IRQENGPB) | 0x00040004, REG_IRQENGPB); 		// falling/rising edge trigger
        
        outl(0x10,  REG_AIC_SCCR); // force clear previous interrupt, 

//        printk("register the headset_detect_irq\n");
        
	    if (request_irq(HEADSET_IRQ_NUM, headset_detect_irq, SA_INTERRUPT, "FA93_headset_DETECT", NULL) != 0) {
	            printk("register the headset_detect_irq failed!\n");
	            return -1;
	    }
	    
        //Enable_IRQ(HEADSET_IRQ_NUM);	    
		outl((1<<4), REG_AIC_MECR);		        
	
	#elif (CONFIG_HEADSET_GPD14_AND_SPEAKER_GPA2)				

        outl(inl(REG_GPDFUN) & ~MF_GPD14, REG_GPDFUN);				// headset detect plug IN/OUT
        outl(inl(REG_GPIOD_OMD) & ~(0x4000), REG_GPIOD_OMD); 		// port D14 input
        outl(inl(REG_GPIOD_PUEN) | (0x4000), REG_GPIOD_PUEN); 		// port D14 pull-up

        outl(inl(REG_GPAFUN) & ~MF_GPA2, REG_GPAFUN);				// speaker control by GPA2
        outl(inl(REG_GPIOA_OMD) | (0x0004), REG_GPIOA_OMD); 		
        outl(inl(REG_GPIOA_PUEN) | (0x0004), REG_GPIOA_PUEN); 		
        
		if ( inl(REG_GPIOD_PIN) & 0x4000)	// headset plug_out
		{
	       	outl(inl(REG_GPIOA_DOUT)|(0x0004), REG_GPIOA_DOUT); 	// GPA2 = high
	       	
	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                
			spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel	
			spu_SetPAN(1, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
	#endif					
		}		
		else								// headset plug_in		
		{
	       	outl(inl(REG_GPIOA_DOUT)&(~0x0004), REG_GPIOA_DOUT); 	// GPA2 = low
		       	
	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                			
			spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
			spu_SetPAN(1, 0x001F);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
	#endif			
		}			

        outl(inl(REG_IRQTGSRC1) & 0x40000000, REG_IRQTGSRC1);               
               
        outl((inl(REG_IRQSRCGPD) & ~(0x30000000)) | 0x20000000, REG_IRQSRCGPD); // port D14 as nIRQ2 source
        outl(inl(REG_IRQENGPD) | 0x40004000, REG_IRQENGPD); 		// falling/rising edge trigger
        
        outl(0x10,  REG_AIC_SCCR); // force clear previous interrupt, 

//        printk("register the headset_detect_irq\n");
        
	    if (request_irq(HEADSET_IRQ_NUM, headset_detect_irq, SA_INTERRUPT, "FA93_headset_DETECT", NULL) != 0) {
	            printk("register the headset_detect_irq failed!\n");
	            return -1;
	    }
	    
        //Enable_IRQ(HEADSET_IRQ_NUM);	    
		outl((1<<4), REG_AIC_MECR);		        
        
	#elif (CONFIG_HEADSET_GPE0_AND_SPEAKER_GPE1)				

        outl(inl(REG_GPEFUN) & ~MF_GPE0, REG_GPEFUN);				// headset detect plug IN/OUT
        outl(inl(REG_GPEFUN) & ~MF_GPE1, REG_GPEFUN);				// speaker control signal
        outl(inl(REG_GPIOE_OMD) & ~(0x0001), REG_GPIOE_OMD); 		// port E0 input
        outl(inl(REG_GPIOE_PUEN) | (0x0001), REG_GPIOE_PUEN); 		// port E0 pull-up
        
		outl(inl(REG_GPIOE_OMD)  | (0x0002), REG_GPIOE_OMD); 		// port E1 output
        outl(inl(REG_GPIOE_PUEN) | (0x0002), REG_GPIOE_PUEN); 		// port E1 pull-up

		if ( inl(REG_GPIOE_PIN) & 0x0001)	// headset plug_in
        	outl(inl(REG_GPIOE_DOUT) &~(0x0002), REG_GPIOE_DOUT);	// port E1 = low		

		else								// headset plug_out		
        	outl(inl(REG_GPIOE_DOUT) | (0x0002), REG_GPIOE_DOUT); 	// port E1 = high        	
               

        outl(inl(REG_IRQTGSRC2) & 0x00000001, REG_IRQTGSRC2);               
               
        outl((inl(REG_IRQSRCGPE) & ~(0x03)) | 0x02, REG_IRQSRCGPE); // port E0 as nIRQ2 source
        outl(inl(REG_IRQENGPE) | 0x00010001, REG_IRQENGPE); 		// falling/rising edge trigger
        
//		outl((inl(REG_AIC_SCR2) & 0xFFFFFF00) | 0x000000c7, REG_AIC_SCR2);

        outl(0x10,  REG_AIC_SCCR); // force clear previous interrupt, 

//        printk("register the headset_detect_irq\n");
        
	    if (request_irq(HEADSET_IRQ_NUM, headset_detect_irq, SA_INTERRUPT, "FA93_headset_DETECT", NULL) != 0) {
	            printk("register the headset_detect_irq failed!\n");
	            return -1;
	    }
	    
        //Enable_IRQ(HEADSET_IRQ_NUM);	    
		outl((1<<4), REG_AIC_MECR);		        

	#elif (CONFIG_HEADSET_GPD3_AND_SPEAKER_GPD4)				

        outl(inl(REG_GPDFUN) & ~MF_GPD3, REG_GPDFUN);				// headset detect plug IN/OUT
        outl(inl(REG_GPIOD_OMD) & ~(0x0008), REG_GPIOD_OMD); 		// port D3 input
        outl(inl(REG_GPIOD_PUEN) | (0x0008), REG_GPIOD_PUEN); 		// port D3 pull-up

        outl(inl(REG_GPDFUN) & ~MF_GPD4, REG_GPDFUN);				// speaker control by GPD4
        outl(inl(REG_GPIOD_OMD) | (0x0010), REG_GPIOD_OMD); 		
        outl(inl(REG_GPIOD_PUEN) | (0x0010), REG_GPIOD_PUEN); 		
        
		if ( inl(REG_GPIOD_PIN) & 0x0008)	// headset plug_out
		{
	       	outl(inl(REG_GPIOD_DOUT)|(0x0010), REG_GPIOD_DOUT); 	// GPD4 = high
	       	
	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                
			spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel	
			spu_SetPAN(1, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel		
	#endif					
		}		
		else								// headset plug_in		
		{
	       	outl(inl(REG_GPIOD_DOUT)&(~0x0010), REG_GPIOD_DOUT); 	// GPD4 = low
		       	
	#ifdef CONFIG_SOUND_W55FA93_PLAY_SPU				                	                			
			spu_SetPAN(0, 0x1F00);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
			spu_SetPAN(1, 0x001F);			// MSB 8-bit = left channel; LSB 8-bit = right channel				
	#endif			
		}			

        outl(inl(REG_IRQTGSRC1) & 0x00080000, REG_IRQTGSRC1);               
               
        outl((inl(REG_IRQSRCGPD) & ~(0x000000c0)) | 0x00000080, REG_IRQSRCGPD); // port D3 as nIRQ2 source
        outl(inl(REG_IRQENGPD) | 0x00080008, REG_IRQENGPD); 		// falling/rising edge trigger
        
        outl(0x10,  REG_AIC_SCCR); // force clear previous interrupt, 

//        printk("register the headset_detect_irq\n");
        
	    if (request_irq(HEADSET_IRQ_NUM, headset_detect_irq, SA_INTERRUPT, "FA93_headset_DETECT", NULL) != 0) {
	            printk("register the headset_detect_irq failed!\n");
	            return -1;
	    }
	    
        //Enable_IRQ(HEADSET_IRQ_NUM);	    
		outl((1<<4), REG_AIC_MECR);		        
        
	#endif
	                
#endif

	return 0;

quit:
	printk("Nuvoton Audio Driver Initialization failed\n");
	wb_audio_exit();
	return -1;
}
 
module_init(wb_audio_init);
module_exit(wb_audio_exit);

