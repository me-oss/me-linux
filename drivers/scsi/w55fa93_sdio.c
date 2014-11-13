/*
 * drivers/driver/scsi/w55fa93_sdio.c
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
 *   2006/11/24     zmsong add this file for nuvoton sdio
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/highmem.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include <linux/autoconf.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>

#include <asm/uaccess.h>
#include <asm/arch/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/scatterlist.h>

#include <asm/arch/irqs.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/w55fa93_sdio.h>

/**for auto mount/umount**/
#include <linux/kmod.h>
#include <asm/uaccess.h>

#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
#include <asm/arch/w55fa93_sysmgr.h>
extern void sysmgr_report(unsigned status);
#endif

//#define SD_DEBUG
//#define SD_DEBUG_ENABLE_ENTER_LEAVE
//#define SD_DEBUG_ENABLE_MSG
//#define SD_DEBUG_ENABLE_MSG2
//#define SD_DEBUG_PRINT_LINE

#ifdef SD_DEBUG
#define PDEBUG(fmt, arg...)		printk(fmt, ##arg)
#else
#define PDEBUG(fmt, arg...)
#endif

#ifdef SD_DEBUG_PRINT_LINE
#define PRN_LINE()				PDEBUG("[%-20s] : %d\n", __FUNCTION__, __LINE__)
#else
#define PRN_LINE()
#endif

#ifdef SD_DEBUG_ENABLE_ENTER_LEAVE
#define ENTER()					PDEBUG("[%-20s] : Enter...\n", __FUNCTION__)
#define LEAVE()					PDEBUG("[%-20s] : Leave...\n", __FUNCTION__)
#else
#define ENTER()
#define LEAVE()
#endif

#ifdef SD_DEBUG_ENABLE_MSG
#define MSG(msg)				PDEBUG("[%-20s] : %s\n", __FUNCTION__, msg)
#else
#define MSG(msg)
#endif

#ifdef SD_DEBUG_ENABLE_MSG2
#define MSG2(fmt, arg...)			PDEBUG("[%-20s] : "fmt, __FUNCTION__, ##arg)
#define PRNBUF(buf, count)		{int i;MSG2("CID Data: ");for(i=0;i<count;i++)\
									PDEBUG("%02x ", buf[i]);PDEBUG("\n");}
#else
#define MSG2(fmt, arg...)
#define PRNBUF(buf, count)
#endif


#define Enable_IRQ(n)     				outl(1 << (n),REG_AIC_MECR)
#define Disable_IRQ(n)    				outl(1 << (n),REG_AIC_MDCR)
#define sd_get_resp()					    ((inl(REG_SDRSP1) & 0xff) |((inl(REG_SDRSP0) << 8) & 0xffffff00))


struct sd_hostdata sd_host;
static FLAG_SETTING flagSetting;
static unsigned int registered = 0;
static volatile int has_command = 0,sd_event = SD_EVENT_NONE;
static DECLARE_WAIT_QUEUE_HEAD(sd_wq);
static DECLARE_WAIT_QUEUE_HEAD(scsi_wq);

static DECLARE_WAIT_QUEUE_HEAD(sd_event_wq);
static DECLARE_WAIT_QUEUE_HEAD(sd_rw);
static DECLARE_MUTEX(sem);
//static DECLARE_MUTEX(sem_r);
//static DECLARE_MUTEX(sem_w);

#define FALSE             0
#define TRUE              1



volatile s8  _fmi_bIsSDDataReady = FALSE;

static int sd_add = 0;
static int alloc0 = 0;
static void sd_Long_TimeOut(unsigned long data);
static void sd_card_stop(struct sd_hostdata *);
static void sd_done(struct sd_hostdata *dev);

extern struct semaphore  dmac_sem;
extern struct semaphore fmi_sem;
static int card_detect = 0; //ya
static struct timer_list sd_timer;	// handle debunce

void sdCheckRB(void);

static unsigned int s_u32ExtClockMHz = 12;
//static unsigned int s_u32ExtClockMHz = 27;

extern unsigned int w55fa93_apll_clock;
extern unsigned int w55fa93_upll_clock;


static void sd_Short_TimeOut(unsigned long data)
{

        // set long timeout event
        sd_timer.data = 0UL;
        sd_timer.expires = jiffies +  SD_LONG_DELAY;
        sd_timer.function = sd_Long_TimeOut;
        add_timer(&sd_timer);
        outl(inl(REG_SDIER) | SDIER_CD_IEN, REG_SDIER);	// enable card0 detect interrupt
        return;
}

static void sd_Long_TimeOut(unsigned long data)
{
        struct sd_hostdata *dev = NULL;
        dev = &sd_host;

        if ((inl(REG_SDISR) & SDISR_CD_Card)) { //sd0 removed
#ifdef CONFIG_NUVOTON_W55FA93_SD_PWR
                outl(inl(REG_GPIOD_DOUT) | 1, REG_GPIOD_DOUT);
                //mdelay(500);
#endif
                sd_event = SD_EVENT_REMOVE;
                card_detect = 0;//ya
                wake_up_interruptible(&sd_event_wq);
                wake_up_interruptible(&sd_rw);
                //outl(inl(REG_PINFUN) & ~PINFUN_SDHPIN_EN, REG_PINFUN);  //ya

                printk("Card0 Removed\n");
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
                sysmgr_report(SYSMGR_STATUS_SD_REMOVE);
#endif
                if (dev->state != SD_STATE_NOP) {
                        dev->sense = SS_MEDIUM_NOT_PRESENT;
                        goto sd_inter_quit;
                }
        } else {
#ifdef CONFIG_NUVOTON_W55FA93_SD_PWR
                outl(inl(REG_GPIOD_DOUT) & ~1, REG_GPIOD_DOUT);
                //mdelay(500);
#endif
                printk("Card0 Inserted\n");

                sd_event = SD_EVENT_ADD;
                //card_detect = 1; //ya
                wake_up_interruptible(&sd_event_wq);
        }

        outl(inl(REG_SDISR),REG_SDISR);

        return;

sd_inter_quit:

        outl(inl(REG_SDISR), REG_SDISR);		/* clear all interrupt */
        sd_card_stop(dev);
        dev->state = SD_STATE_NOP;
        sd_done(dev);	/* scsi done */
        return;
}




static u16 inline get_be16(u8 *buf)
{
        return ((u16) buf[0] << 8) | ((u16) buf[1]);
}

static u32 inline get_be32(u8 *buf)
{
        return ((u32) buf[0] << 24) | ((u32) buf[1] << 16) |
               ((u32) buf[2] << 8) | ((u32) buf[3]);
}

static void inline put_be16(u16 val, u8 *buf)
{
        buf[0] = val >> 8;
        buf[1] = val;
}

static void inline put_be32(u32 val, u8 *buf)
{
        buf[0] = val >> 24;
        buf[1] = val >> 16;
        buf[2] = val >> 8;
        buf[3] = val;
}

static void sd_make_sense_buffer(struct sd_hostdata *dev,int key, int asc, int ascq)
{

        unsigned char *sense_buffer;

        sense_buffer = dev->cmd->sense_buffer;
        memset(sense_buffer, 0, 18);

        sense_buffer[0] = 0x70;		// error code
        sense_buffer[2] = key;
        sense_buffer[7] = 0x0A;		// sdditional sense length
        sense_buffer[12] = asc;
        sense_buffer[13] = ascq;

}


static unsigned int DrvSys_GetPLLOutputMHz(void)
{

	unsigned int u32Freqout;
	unsigned int u32OTDV;
	
#ifdef CONFIG_W55FA93_SDIO_CLOCK_FROM_APLL	
	
	// SPU enigine clock from APLL
	u32OTDV = (((inl(REG_APLLCON)>>14)&0x3)+1)>>1;	
		
	u32Freqout = (s_u32ExtClockMHz*((inl(REG_APLLCON)&0x1FF)+2)/(((inl(REG_APLLCON)>>9)&0x1F)+2))>>u32OTDV;	
#else
	// SPU enigine clock from UPLL 
	u32OTDV = (((inl(REG_UPLLCON)>>14)&0x3)+1)>>1;	
		
	u32Freqout = (s_u32ExtClockMHz*((inl(REG_UPLLCON)&0x1FF)+2)/(((inl(REG_UPLLCON)>>9)&0x1F)+2))>>u32OTDV;	
#endif	
	
	return u32Freqout;
}

static int sd_check_error(void)
{
        int status;

        status = sd_get_resp();

        MSG2("Status : %x\n", status);

        if ((status >> 16) & 0xffff) {
                PDEBUG("Command execuate error. Status code %08x\n", status);
                return SD_FAILED;
        }

        return SD_SUCCESS;
}

/* sd send command */
static int sd_wait_cmd(void)
{
        u32 count;

        ENTER();

        count = jiffies + SD_CMD_TIMEOUT;

        while (1) {
                if (flagSetting.bCardExist==0)
                        return SD_REMOVED;
                if ((inl(REG_SDCR) & SDCR_CO_EN) == 0)
                        break;

                if (time_after(jiffies, count))
                        return SD_TIMEOUT;
        }

        LEAVE();

        return 0;
}

static int sd_command(int cmd, int arg)
{
        int retval;

        ENTER();

        MSG2("Command = %d   Args = %x\n", cmd, arg);

        if (flagSetting.bCardExist==0)
                return SD_REMOVED;

        outl(arg,REG_SDARG);	/* set argument */

        outl((inl(REG_SDCR)&0xffffc080) | (cmd  << 8) | SDCR_CO_EN, REG_SDCR);

        /* wait command complete */
        retval = sd_wait_cmd();

        LEAVE();

        return retval;
}




static int sd_cmd_and_resp(int cmd, int arg)
{
        int retval;
        u32 count;

        ENTER();
        MSG2("Command = %d   Args = %x\n", cmd, arg);

        if (flagSetting.bCardExist==0)
                return SD_REMOVED;


        outl(arg,REG_SDARG);	/* set argument */
        outl((inl(REG_SDCR)&0xffffc080) | (cmd  << 8) | SDCR_CO_EN|SDCR_RI_EN, REG_SDCR);

        /* wait command complete */
        retval = sd_wait_cmd();

        if (retval)
                return retval;

        count = jiffies + SD_RESP_TIMEOUT;
        while (1) {
                if ((inl(REG_SDCR) & SDCR_RI_EN) == 0)
                        break;

                if (time_after(jiffies, count)) {
                        if (flagSetting.needReset == 1) {
                                outl(inl(REG_SDCR)|SDCR_SWRST,REG_SDCR); //reset SD
                                while (inl(REG_SDCR) & SDCR_SWRST);
                        } else {
                                outl(inl(REG_SDCR)&(~(SDCR_CO_EN | SDCR_RI_EN | SDCR_R2_EN)), REG_SDCR);
                                flagSetting.needReset = 1;
                        }
                        return SD_TIMEOUT;
                }
        }
        MSG2("Response : %08x %08x\n", inl(REG_SDRSP0), inl(REG_SDRSP1));

        if (flagSetting.r7_cmd == 1) {
                if (((inl(REG_SDRSP1) & 0xff) != 0x55) && ((inl(REG_SDRSP0) & 0xf) != 0x01)) {
                        flagSetting.r7_cmd = 0;
                        return SD_FAILED;
                }

                flagSetting.r7_cmd = 0;
        }

        if (flagSetting.bCrcCheck == 1) { //check CRC7
                if (inl(REG_SDISR) & SDISR_CRC_7) // no crc error
                        return SD_SUCCESS;
                else { //crc7error
                        printk("CRC-7 error in sd_cmd_and_resp()\n");
                        return SD_STATE_ERR;
                }

        } else {//no need
                flagSetting.bCrcCheck = 1;
                outl(SDISR_CRC_7|SDISR_CRC_IF,REG_SDISR); //clear
                return SD_SUCCESS;
        }

}


static int sd_cmd_and_resp_data_in(int cmd, int arg)
{

        u32 count;

        ENTER();
        MSG2("Command = %d   Args = %x\n", cmd, arg);

        if (flagSetting.bCardExist == 0) {
                return SD_REMOVED;
        }
        outl(arg,REG_SDARG);	/* set argument */
        outl((inl(REG_SDCR)&0xffffc080) | (cmd  << 8) | (0x07), REG_SDCR);


        count = jiffies + (SD_RESP_TIMEOUT);
        while (1) {
                if ((inl(REG_SDCR) & (SDCR_RI_EN | SDCR_DI_EN)) == 0)
                        break;
                if (time_after(jiffies, count))
                        return SD_TIMEOUT;

                if (flagSetting.bCardExist == 0)
                        return SD_REMOVED;
        }


        if (flagSetting.bCrcCheck == 1) { //check CRC7 & CRC16
                if ((inl(REG_SDISR) & SDISR_CRC_7) && (inl(REG_SDISR) & SDISR_CRC_16)) // no crc error
                        return SD_SUCCESS;
                else { //crc7error
                        printk("CRC error in sd_cmd_and_resp_data_in()\n");
                        return SD_STATE_ERR;
                }

        } else {//no need
                flagSetting.bCrcCheck = 1;
                outl(SDISR_CRC_7|SDISR_CRC_IF,REG_SDISR); //clear
                return SD_SUCCESS;
        }
}


static int sd_cmd_and_resp2(int cmd, int arg, char *buf)
{
        u32 i, count;
        unsigned int tmpbuf[4];
        int retval;

        ENTER();
        MSG2("Command = %d   Args = %x\n", cmd, arg);

        if (flagSetting.bCardExist==0)
                return SD_REMOVED;

        outl(arg,REG_SDARG);	/* set argument */
        outl((inl(REG_SDCR)&0xffffc080) | (cmd  << 8) | SDCR_CO_EN | SDCR_R2_EN, REG_SDCR);

        /* wait command complete */
        retval = sd_wait_cmd();
        if (retval)
                return retval;

        count = jiffies + SD_RESP_TIMEOUT;
        while (1) {
                if ((inl(REG_SDCR) & SDCR_R2_EN) == 0)
                        break;

                if (flagSetting.bCardExist==0)
                        return SD_REMOVED;

                if (time_after(jiffies, count))
                        return SD_TIMEOUT;
        }

        MSG2("Response2 : %08x %08x\n", inl(REG_SDRSP0), inl(REG_SDRSP1));


        if (inl(REG_SDISR) & SDISR_CRC_7) {		/* r2 crc-7 check */

                memcpy(buf, (char *)(REG_FB_0 + 1), 16);

                for (i = 0; i < 4; i++) {
                        tmpbuf[3 - i] = Swap32(*((u32 *)(buf + i * 4)));
                }

                memcpy(buf, tmpbuf, 16);

                LEAVE();
                return SD_SUCCESS;
        } else
                return SD_FAILED;
}

static int sd_app_cmd(int cmd, int arg)
{
        unsigned int retval;
        struct sd_hostdata *dev=NULL;

        dev = &sd_host;


        ENTER();

        retval = sd_cmd_and_resp(55, dev->RCA);

        if (retval)
                return retval;



        retval = sd_check_error();
        if (retval)
                return retval;

        retval = sd_cmd_and_resp(cmd, arg);

        LEAVE();

        return SD_SUCCESS;
}



static int sd_sd_card_reset(void)
{
        int retval;
        unsigned int volatile tick;

        ENTER();

        //mdelay(100);

        retval = sd_command(0, 0);	/* reset card to idle state */
        if (retval)
                return retval;
#if 1
        tick = jiffies;
        while (1) {
                if (time_after(jiffies, tick+1))
                        break;
                schedule();
        }

#else
        mdelay(10);		/* wait for card to idle state */
#endif
        retval = sd_cmd_and_resp(55, 0);
        if (retval)
                return retval;

        flagSetting.bCrcCheck = 0;
        flagSetting.needReset = 0;
        retval = sd_cmd_and_resp(41, 0x803c0000);		/* 3.0 - 3.4 v */ //0x00ff8000?
        if (retval)
                return retval;

// check if card is ready
        while (!(inl(REG_SDRSP0)&0x00800000)) {

                retval = sd_cmd_and_resp(55, 0);
                if (retval)
                        return retval;

                flagSetting.bCrcCheck = 0;
                flagSetting.needReset = 0;
                retval = sd_cmd_and_resp(41, 0x803c0000);
                if (retval)
                        return retval;
        }

        LEAVE();

        return SD_SUCCESS;
}

static int sd_sdhc_card_reset(struct sd_hostdata *dev)
{
        int retval;
        unsigned int val;

        ENTER();


        retval = sd_cmd_and_resp(55, 0);
        if (retval)
                return retval;

        flagSetting.bCrcCheck = 0;
        flagSetting.needReset = 0;
        retval = sd_cmd_and_resp(41, 0x40ff8000);		/* 3.0 - 3.4 v */
        if (retval)
                return retval;

// check if card is ready
        while (!((val = sd_get_resp()) & 0x80000000)) {

                retval = sd_cmd_and_resp(55, 0);
                if (retval)
                        return retval;

                flagSetting.bCrcCheck = 0;
                flagSetting.needReset = 0;
                retval = sd_cmd_and_resp(41, 0x40ff8000);
                if (retval)
                        return retval;
        }

        if ((val & 0x40000000) == 0) {
                dev->cardType = CARD_TYPE_SD;
                printk("SD card found\n");
        } else
                printk("SDHC card found\n");

        LEAVE();

        return SD_SUCCESS;
}

static int sd_mmc_card_reset(void)
{
        int retval;

        ENTER();

        retval = sd_command(0, 0);	/* reset card to idle state */
        if (retval)
                return retval;

        mdelay(100);		/* wait for card to idle state */


        do {
                flagSetting.bCrcCheck = 0;
                flagSetting.needReset = 0;
                retval = sd_cmd_and_resp(1, 0x80ff8000);
                if (retval)
                        return retval;
        } while (!(sd_get_resp() & 0x80000000));

        LEAVE();

        return SD_SUCCESS;

}




static void sd_host_reset(void)
{
	struct sd_hostdata *dev=NULL;
    int rate;
//	unsigned int PllFreq = DrvSys_GetPLLOutputMHz();  
#ifdef CONFIG_W55FA93_SDIO_CLOCK_FROM_APLL	
	unsigned int PllFreq = w55fa93_apll_clock;
#else
	unsigned int PllFreq = w55fa93_upll_clock;
#endif

    dev = &sd_host;


        ENTER();

	//Enable IP I/O pins
	outl((inl(REG_GPEFUN)&(~0x0000FFF0)) | 0x0000aaa0, REG_GPEFUN);	// SD0_CLK/CMD/DAT0_3 pins selected
	
	// Enable SD Card Host Controller operation and driving clock.
	outl(inl(REG_AHBCLK) | SIC_CKE | SD_CKE, REG_AHBCLK);	// enable SD engine clock 		
	outl(inl(REG_SDCR) & (~SDCR_SDPORT), REG_SDCR);	

        if (down_interruptible(&dmac_sem))
                return;


//Reset
        while (inl(REG_DMACCSR)&FMI_BUSY); //Wait IP finished... for safe
        outl(FMI_SWRST, REG_FMICR);						// FMI software reset
        while (inl(REG_FMICR)&FMI_SWRST);
        outl(FMI_SD_EN, REG_FMICR); //enable SD

        outl(inl(REG_DMACCSR) | DMAC_EN, REG_DMACCSR);		// enable DMAC
        outl(inl(REG_DMACCSR) | DMAC_SWRST, REG_DMACCSR);	// DMAC software rest
        outl(0, REG_DMACIER);

        outl(inl(REG_SDCR) | SDCR_SWRST, REG_SDCR);        	// SD software reset
        while (inl(REG_SDCR) & SDCR_SWRST);

// chp  outl(inl(REG_SDCR) & (~SDCR_DBW), REG_SDCR);		        // SD 1-bit data bus
// chp  outl(inl(REG_SDCR) & (~SDCR_SDNWR), REG_SDCR);
// chp  outl(inl(REG_SDCR) | 0x09000000, REG_SDCR);             // SDNWR = 10 clock
//  outl(inl(REG_SDCR) | SDCR_CLK_KEEP, REG_SDCR);


// chp  outl(inl(REG_SDIER) & ~SDIER_BLKD_IEN, REG_SDIER);
//  outl(inl(REG_SDIER) | SDIER_DIT_IEN, REG_SDIER);     	// data input timeout
//	outl(inl(REG_SDIER) | SDIER_RIT_IEN, REG_SDIER);      // response timeout
//	outl(inl(REG_SDIER) | SDIER_CRC_IEN, REG_SDIER);      // CRC interrupt enable
//	outl(inl(REG_SDIER) | SDIER_CD_IEN, REG_SDIER);       // CD0 card detect interrupt enable
//	outl(inl(REG_SDIER) | SDIER_BLKD_IEN, REG_SDIER);     // BLKD interrupt enable

//  outl(inl(REG_SDISR)|SDISR_BLKD_IF|SDISR_CD_IF, REG_SDISR);

        if ( inl(REG_GPAFUN) & MF_GPA1 )		// MF_GPA1[1:0]=10 or 11, GPA1 is switch to SD_CD
                outl(inl(REG_SDIER) | SDIER_CDSRC,REG_SDIER);	// SD card detection from GPIO

        outl(0, REG_SDTMOUT);  //disable hw timout

        // for hp test
        outl(0x01010000, REG_SDCR);

	//clock	setting     
//	PllFreq *= 1000;
	
	printk("PllFreq = 0x%x \n", PllFreq);	
	
	rate = PllFreq / 300;
	if ((PllFreq % 300) == 0)
		rate = rate - 1;
		
	printk("rate = 0x%x \n", rate);		
		
#ifdef CONFIG_W55FA93_SDIO_CLOCK_FROM_APLL			
	outl((inl(REG_CLKDIV2) & (~SD_S)) | (0x02 << 19), REG_CLKDIV2);	// SD clock from APLL
#else
	outl((inl(REG_CLKDIV2) & (~SD_S)) | (0x03 << 19), REG_CLKDIV2);	// SD clock from UPLL
#endif	
	
	outl((inl(REG_CLKDIV2) & (~SD_N0)) | (0x07 << 16), REG_CLKDIV2);	// SD clock divided by 8
	rate /= 8;

	outl(inl(REG_CLKDIV2) & (~SD_N1), REG_CLKDIV2);
	rate &= 0xFF;
	rate --;
	outl(inl(REG_CLKDIV2) | (rate<<24), REG_CLKDIV2);	

	printk("REG_CLKDIV2 = 0x%x \n", inl(REG_CLKDIV2));		

        sd_host.nSectorSize = 512;
        dev->state = SD_STATE_NOP;
        dev->sense = SS_NO_SENSE;

        flagSetting.bCrcCheck = 1;
        flagSetting.needReset = 1;
        flagSetting.r7_cmd    = 0;

        up(&dmac_sem);
        LEAVE();
}

static int sd_get_card_type(void)
{
        int retval=0;
        u32 count;

        ENTER();

        // power ON 74 clock
        outl(inl(REG_SDCR) | SDCR_74CLK_OE, REG_SDCR);
        count = jiffies + SD_RESP_TIMEOUT;
        while (1) {
                if ((inl(REG_SDCR) & SDCR_74CLK_OE) == 0)
                        break;

                if (flagSetting.bCardExist==0)
                        return SD_REMOVED;

                if (time_after(jiffies, count))
                        return SD_TIMEOUT;
        }

        retval = sd_command(0,0); // reset all cards
        udelay(100);

        if (retval)
                return retval;


        flagSetting.r7_cmd = 1;
        retval = sd_cmd_and_resp(8,0x155);
        flagSetting.r7_cmd = 0;
        if (retval ==SD_SUCCESS) {
                printk("SDHC memory card detected\n");
                return (CARD_TYPE_SDHC); // should be. will check again in init
        }
        retval = sd_cmd_and_resp(55,0);
        if (retval == SD_TIMEOUT) {		/* MMC memory card */
                printk("MMC memory card detected\n");
                return CARD_TYPE_MMC;
        } else {
                printk("SD memory card detected\n");
                LEAVE();
                return CARD_TYPE_SD;
        }
}

static int sd_get_rca(void)
{
        int retval;
        struct sd_hostdata *dev=NULL;
        dev = &sd_host;


        ENTER();

        /* SD / SDIO Card */
        if ((dev->cardType & CARD_TYPE_SD) || (dev->cardType & CARD_TYPE_SDHC)) {
                retval = sd_cmd_and_resp(3, 0x00);	/* get RCA */

                if (retval)
                        return retval;

                dev->RCA = (inl(REG_SDRSP0)<<8) & 0xffff0000;

        } else {		/* MMC Card */
                retval = sd_cmd_and_resp(3, 0x10000);	/* get RCA */

                if (retval)
                        return retval;

                dev->RCA = 0x10000;
        }


        MSG2("R6 is %08x\n", dev->RCA);



        LEAVE();

        return SD_SUCCESS;
}

static int sd_get_cid(void)
{
        int retval;
        struct sd_hostdata *dev=NULL;
        dev = &sd_host;


        ENTER();

//	retval = sd_command(2, 0);	/* get CID */
//	if(retval)
//		return retval;

        retval = sd_cmd_and_resp2(2, 0, dev->CID);
        if (retval)
                return retval;

        LEAVE();

        return SD_SUCCESS;

}

static int sd_get_csd(void)
{
        int retval;
        unsigned int c_size, mult;
        struct sd_hostdata *dev=NULL;
        dev = &sd_host;


        ENTER();

        retval = sd_cmd_and_resp2(9, dev->RCA, dev->CSD);
        if (retval) {
                MSG("Get CSD error\n");
        } else {

                if ((dev->cardType != CARD_TYPE_MMC) && (dev->CSD[15] & 0xc0)) {
                        // CSD version 2.0
                        c_size = dev->CSD[6] | (dev->CSD[7] << 8) | (dev->CSD[8] << 16) | ((dev->CSD[9] & 0x3) << 24);
                        dev->nSectorSize = 512;
                        dev->nCapacityInByte = 0;//(c_size + 1) * 512 * 1024;
                        dev->nTotalSectors = (c_size + 1) * 1024;
                        MSG2 ("Capacity : %dMB  SectorSize : %d  TotalSector : %d\n",
                              dev->nTotalSectors >> 1,
                              dev->nSectorSize, dev->nTotalSectors);
                } else {

                        mult = (1 << (((*((unsigned int *)&dev->CSD[5]) >> 7) & 0x07) + 2));
                        c_size = ((dev->CSD[7] >> 6) & 0x3) |
                                 ((dev->CSD[8] << 2) & 0x3fc) |
                                 ((dev->CSD[9]<<10) & 0xc00);

                        dev->nTotalSectors = (c_size + 1) * mult;
                        dev->nSectorSize = (1 << (dev->CSD[10] & 0xf));
                        dev->nCapacityInByte = dev->nTotalSectors * dev->nSectorSize;
                        MSG2("Capacity : %dMB  SectorSize : %d  TotalSector : %d\n",
                             dev->nCapacityInByte >> 20,
                             dev->nSectorSize, dev->nTotalSectors);
                }


        }

        LEAVE();

        return retval;
}

static int SwitchToHighSpeed(struct sd_hostdata *dev)
{
        unsigned short current_comsumption, busy_status;
        unsigned char *c = (unsigned char *)dev->DMAvaddr;


        if (down_interruptible(&dmac_sem)) {
                return SD_FAILED;
        }
        while (inl(REG_DMACCSR)&FMI_BUSY); //Wait IP finished
        outl(dev->DMApaddr, REG_DMACSAR);
        outl(7, REG_SDBLEN);

        if (sd_cmd_and_resp(55, dev->RCA) != SD_SUCCESS)
                goto hspeed_failed;

        if (sd_cmd_and_resp_data_in(51, 0x00) != SD_SUCCESS) {
                outl(3, REG_DMACCSR); // reset DMA
                //bus_width = inl(REG_SDCR) & (1 << 15);  // 1-bit or 4-bit?
                outl((1 << 14), REG_SDCR);  //reset FMI
                //udelay(10);
                //outl(inl(REG_SDCR) | bus_width, REG_SDCR);
                goto hspeed_failed;
        }
        if ((c[0] & 0xf)	!= 0x2)
                goto hspeed_failed;

        outl(dev->DMApaddr, REG_DMACSAR);
        outl(63, REG_SDBLEN);

        if (sd_cmd_and_resp_data_in(6, 0x00ffff01) != SD_SUCCESS)
                goto hspeed_failed;

        current_comsumption = c[0] << 8 | c[1];
        if (!current_comsumption)
                goto hspeed_failed;

        busy_status = c[28]<<8 | c[29];

        if (!busy_status) {	// function ready
                outl(dev->DMApaddr, REG_DMACSAR);
                outl(63, REG_SDBLEN);

                if (sd_cmd_and_resp_data_in(6, 0x80ffff01) != SD_SUCCESS)
                        goto hspeed_failed;
                // function change timing: 8 clocks
                outl(inl(REG_SDCR)|0x40, REG_SDCR);
                while (inl(REG_SDCR) & 0x40);

                current_comsumption = c[0]<<8 | c[1];
                if (!current_comsumption)
                        goto hspeed_failed;

                MSG2("Switch to high speed success\n");
                up(&dmac_sem);
                return SD_SUCCESS;
        }

hspeed_failed:
        up(&dmac_sem);
        return SD_FAILED;

}

static void sd_change_clock(int type)
{

        int rate;
        int clock;
        struct sd_hostdata *dev=NULL;
//	  	unsigned int PllFreq = DrvSys_GetPLLOutputMHz();  
#ifdef CONFIG_W55FA93_SDIO_CLOCK_FROM_APLL	
		unsigned int PllFreq = w55fa93_apll_clock;
#else
		unsigned int PllFreq = w55fa93_upll_clock;	  	
#endif		
	  	
        dev = &sd_host;

        ENTER();

#if 0
        if (dev->cardType & CARD_TYPE_MMC)
                clock = 20000;
        else if (dev->cardType & CARD_TYPE_SD)
                clock = 25000;
        else {  // SDHC
                if (SwitchToHighSpeed(dev) == SD_SUCCESS)
                        clock = 48000;//40000; //50000
                else
                        clock = 25000;
        }
#else
        if (dev->cardType & CARD_TYPE_MMC)
                clock = 20000;
        else  // SD & SDHC
              //  clock = 25000;
				clock = 24000;                


#endif

#if 0	// mh removes it on 20101206

        if (type == 0) 
        {
            if (dev->cardType & CARD_TYPE_SDHC) {
                if (SwitchToHighSpeed(dev) == SD_SUCCESS)
                    clock = 48000;
            }
        }
#endif        

 //       rate = FMI_INPUT_CLOCK / clock;
 //       if ((FMI_INPUT_CLOCK % clock) == 0)
 //               rate = rate - 1;

//	PllFreq *= 1000;
	rate = PllFreq / clock;	
//	printk(" (change clock) PllFreq = 0x%x \n", PllFreq);	
//	printk(" (change clock) clock = 0x%x \n", clock);		
	
	outl((inl(REG_CLKDIV2) & (~SD_N0)) | (0x01 << 16), REG_CLKDIV2);	// SD clock divided by 2
	rate /= 2;	

	outl(inl(REG_CLKDIV2) & (~SD_N1), REG_CLKDIV2);
	rate &= 0xFF;

	if (!(PllFreq % clock))
		rate --;  
		
//	printk("rate = 0x%x \n", rate);				
		
	outl(inl(REG_CLKDIV2) | (rate<<24), REG_CLKDIV2);	
	
//	printk("REG_CLKDIV2 = 0x%x \n", inl(REG_CLKDIV2));			
//	MSG2("rate=%d;Set Speed : %d\n", rate,clock);

        LEAVE();

        return;
}

static int sd_enter_transmode(void)
{
        int retval;
        struct sd_hostdata *dev=NULL;
        dev = &sd_host;


        ENTER();
        flagSetting.needReset = 0;
        retval = sd_cmd_and_resp(7, dev->RCA);
        if (retval)
                return retval;

        retval = sd_check_error();	/* trans mode state */

        LEAVE();

        return retval;
}

static int sd_set_bus_width(void)
{
        int retval;
        struct sd_hostdata *dev=NULL;
        dev = &sd_host;


        ENTER();

        if ((dev->cardType) == CARD_TYPE_MMC) {
                outl(inl(REG_SDCR) & 0xffff7fff,REG_SDCR);
                retval = sd_cmd_and_resp(16, 512);
                if (retval)
                        return retval;
                //sd_command(7, 0x0000);
                outl(inl(REG_SDIER)|SDIER_BLKD_IEN, REG_SDIER);	/* enable read/write interrupt */
                MSG("MMC card's bus width = 1\n");
                return SD_SUCCESS;
        }

        outl(7,REG_SDBLEN);	// 64 bit
        retval = sd_app_cmd(6, 0x02);
        if (retval)
                return retval;

        outl(inl(REG_SDCR) | SDCR_DBW, REG_SDCR);		/* set for 4 bit width */

        if ( dev->nSectorSize != 512) {
                retval = sd_cmd_and_resp(16, 512);
                if (retval)
                        return retval;

                dev->nTotalSectors = dev->nCapacityInByte / 512;
                dev->nSectorSize = 512;

        }

//	sd_command(7, 0x0000);
        outl(inl(REG_SDIER)|SDIER_BLKD_IEN, REG_SDIER);	/* enable read/write interrupt */

        MSG("Set BUS width = 4 \n");

        LEAVE();

        return retval;

}

static int sd_card_init(void)
{
        int retval = SD_FAILED;
        struct sd_hostdata *dev=NULL;
        dev = &sd_host;


        ENTER();

        // if (down_interruptible(&fmi_sem))
        //        return SD_FAILED;

        sd_host_reset();

        if ( flagSetting.bCardExist == 0) {
                //    up(&fmi_sem);
                return SD_REMOVED;
        }

        outl(FMI_SD_EN, REG_FMICR);	/* Enable SD functionality of FMI */
        dev->cardType = sd_get_card_type();

        /* sdio should be not reseted here, it will take when use */

        if (dev->cardType & CARD_TYPE_MMC)
                retval = sd_mmc_card_reset();
        else if (dev->cardType & CARD_TYPE_SD)
                retval = sd_sd_card_reset();
        else if (dev->cardType & CARD_TYPE_SDHC)
                retval = sd_sdhc_card_reset(dev);
        else
                MSG2("Error occured %08x\n", dev->cardType);

        PRN_LINE();
        if (retval)
                goto init_exit;
        retval = sd_get_cid();
        if (retval)
                goto init_exit;
        PRN_LINE();

        retval = sd_get_rca();
        if (retval)
                goto init_exit;
        PRN_LINE();

        sd_change_clock(1);   // for hp test

        retval = sd_get_csd();
        if (retval)
                goto init_exit;


        PRN_LINE();
        retval = sd_enter_transmode();
        if (retval)
                goto init_exit;

        PRN_LINE();
#if 0
        retval = sd_set_bus_width();
        if (retval)
                goto init_exit;

        PRN_LINE();
        sd_change_clock();
#else
        sd_change_clock(0);   // ya remove for testing...

        retval = sd_set_bus_width();
        if (retval)
                goto init_exit;

#endif
        flagSetting.bWriteProtect = 0;
        /* check for write protect */
        if (((dev->CSD[1]>>4) & 0x3) != 0) {
                flagSetting.bWriteProtect = 1;
        }
#ifdef CONFIG_NUVOTON_W55FA93_SD_WP
        if (inl(REG_GPIOA_PIN) & 1) {		// check SD_WP (GPIOA_0) pin status
                flagSetting.bWriteProtect = 1;
        }
#endif
        MSG2("Init SD Card Result = %d\n", retval);

init_exit:
        if (retval)
                printk("Init Card error [%d]\n",retval);

        LEAVE();
        //up(&fmi_sem);
        return retval;
}



static void sd_card_stop(struct sd_hostdata *dev)
{
        ENTER();

        if (card_detect == 0)
                return;
        flagSetting.needReset = 0;
        sd_cmd_and_resp(12, 0);

        LEAVE();
}



static void sd_done(struct sd_hostdata *dev)
{
        ENTER();

        if ( dev->sense != SS_NO_SENSE) {
                sd_make_sense_buffer(dev,dev->sense >> 16,
                                     dev->sense >> 8,
                                     dev->sense);

                if ( flagSetting.bCardExist != 0 && card_detect != 0)
                        dev->cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_CHECK_CONDITION);
                else
                        dev->cmd->result = SD_CMD_RESULT(DID_NO_CONNECT, DISCONNECT, SAM_STAT_CHECK_CONDITION);

        } else {
                dev->cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );

        }

        MSG( "calling SCSI_done() in sd_done!\n" );

        wake_up_interruptible(&sd_wq);

        LEAVE();
}



void sdCheckRB()
{
        while (!(inl(REG_SDISR) & SDISR_SD_DATA0)) {
                outl(inl(REG_SDCR)|0x40,REG_SDCR); //clock 8
                while ((inl(REG_SDCR) & 0x40) && (card_detect != 0) && (flagSetting.bCardExist!=0)) {
                        schedule();
                }
                if (flagSetting.bCardExist==0 || card_detect == 0)
                        break;

        }
}



static int sd_start_read(struct sd_hostdata *dev)
{
        volatile int sdcr,loop;
        volatile int curCount, i;
        s8 volatile bIsSendCmd = FALSE;

        volatile int curDMAAddr,listLenth;
        volatile int copySize = 0;
        volatile int DMAoffset = 0;
        unsigned long time; //ya
        int ret = 0;
        struct scatterlist *curList;


        curList = &dev->firstList[dev->curList];
        curDMAAddr = (unsigned int )kmap_atomic(curList->page, KM_USER0) +	curList->offset;

        curCount = dev->curCount;
        loop = curCount / 255;

        if (down_interruptible(&dmac_sem)) {
                return(-2);
        }
        for (i=0; i<loop; i++) {

                _fmi_bIsSDDataReady = FALSE;
                sdcr = (inl(REG_SDCR)& 0xff00c080) | (255 << 16);

                while (inl(REG_DMACCSR)&FMI_BUSY); //Wait IP finished
                outl(dev->DMApaddr, REG_DMACSAR);

                if (!bIsSendCmd) {
                        time = jiffies;  //ya
                        outl(sdcr|(18<<8)|0x07, REG_SDCR);
                        bIsSendCmd = TRUE;
                        while ((inl(REG_SDCR) & 0x3) && (time_after(jiffies, time+HZ))); //ya
                } else
                        outl((sdcr | 0x4),REG_SDCR);
#if 0
                while (!_fmi_bIsSDDataReady) {
                        if (flagSetting.bCardExist == 0)
                                goto read_exit;

                }
#else
                wait_event_interruptible(sd_rw, _fmi_bIsSDDataReady || (flagSetting.bCardExist == 0) || card_detect == 0);
                if (flagSetting.bCardExist == 0 || card_detect == 0) {
                        dev->sense = SS_MEDIUM_NOT_PRESENT;  //ya
                        outl(3, REG_DMACCSR); // reset DMA
                        outl((1 << 14), REG_SDCR);  //reset FMI
                        //outl(1, REG_SMCSR);  //reset FMI
                        ret = -1;
                        goto read_exit;
                }
#endif

                curDMAAddr = curDMAAddr + copySize;
                listLenth = (curList->length) - copySize;
                DMAoffset=0;
                copySize = 255 * DMA_BLOCK_SIZE;

                while (copySize > listLenth) {
                        memcpy((char*)curDMAAddr,(char*)dev->DMAvaddr+DMAoffset,listLenth);
                        dev->curList ++;
                        curList = &dev->firstList[dev->curList];
                        curDMAAddr = (unsigned int )kmap_atomic(curList->page, KM_USER0) + curList->offset;
                        DMAoffset += listLenth;
                        copySize = copySize-listLenth;
                        listLenth = curList->length;

                }

                if ( copySize < listLenth) {

                        memcpy((char*)curDMAAddr,(char*)dev->DMAvaddr+DMAoffset,copySize);
                }

                if ( copySize == listLenth) {

                        memcpy((char*)curDMAAddr,(char*)dev->DMAvaddr+DMAoffset,copySize);
                        if ((i!=(loop -1 )) || ((curCount % 255)!=0)) {
                                dev->curList ++;
                                curList = &dev->firstList[dev->curList];
                                curDMAAddr = (unsigned int )kmap_atomic(curList->page, KM_USER0) +	curList->offset;
                                copySize=0;
                        }
                }

                if (!(inl(REG_SDISR) & 0x08)) {	// check CRC16
                        printk("SD_Read: CRC16_1 error! when i=%d\n",i);
                }

        }

        loop = curCount % 255;
        if (loop !=0) {
                _fmi_bIsSDDataReady = FALSE;
                sdcr = (inl(REG_SDCR)& 0xff00c080)| (loop << 16);
                while (inl(REG_DMACCSR)&FMI_BUSY); //Wait IP finished
                outl(dev->DMApaddr, REG_DMACSAR);

                if (!bIsSendCmd) {
                        time = jiffies;  //ya
                        outl(sdcr|(18<<8)|0x07, REG_SDCR);
                        bIsSendCmd = TRUE;
                        //while(inl(REG_SDCR) & 0x3);
                        while ((inl(REG_SDCR) & 0x3) && (time_after(jiffies, time+HZ))); //ya
                } else
                        outl(sdcr | 0x04,REG_SDCR);
#if 0
                while (!_fmi_bIsSDDataReady) {
                        if (flagSetting.bCardExist == 0)
                                goto read_exit;

                }
#else
                wait_event_interruptible(sd_rw, _fmi_bIsSDDataReady ||(flagSetting.bCardExist == 0) || card_detect == 0);
                if (flagSetting.bCardExist == 0 || card_detect == 0) {
                        dev->sense = SS_MEDIUM_NOT_PRESENT;   //ya
                        outl(3, REG_DMACCSR); // reset DMA
                        outl((1 << 14), REG_SDCR);  //reset FMI
                        //outl(1, REG_SMCSR);  //reset FMI
                        ret = -1;
                        goto read_exit;
                }

#endif
                curDMAAddr = curDMAAddr + copySize;
                listLenth = (curList->length) - copySize;
                DMAoffset=0;
                copySize = loop * DMA_BLOCK_SIZE;


                while (copySize > listLenth) {
                        memcpy((char*)curDMAAddr,(char*)dev->DMAvaddr+DMAoffset,listLenth);
                        dev->curList ++;
                        curList = &dev->firstList[dev->curList];
                        curDMAAddr = (unsigned int )kmap_atomic(curList->page, KM_USER0) +	curList->offset;
                        DMAoffset += listLenth;
                        copySize = copySize-listLenth;
                        listLenth = curList->length;

                }

                if ( copySize < listLenth) { //may be never to do this fuction

                        memcpy((char*)curDMAAddr,(char*)dev->DMAvaddr+DMAoffset,copySize);
                }
                if ( copySize == listLenth) {
                        memcpy((char*)curDMAAddr,(char*)dev->DMAvaddr+DMAoffset,copySize);
                }


                if (!(inl(REG_SDISR) & 0x08)) {	// check CRC16
                        printk("SD_Read: CRC16_2 error!\n");
                }

        }
read_exit:
        up(&dmac_sem);

        return(ret);

}


static int sd_start_write(struct sd_hostdata *dev)
{
        volatile int sdcr,loop;
        volatile int curCount, i;
        s8 volatile bIsSendCmd = FALSE;

        volatile int curDMAAddr,listLenth;
        volatile int copySize = 0;
        volatile int DMAoffset = 0;
        unsigned long time; //ya
        int ret = 0; //ya
        struct scatterlist *curList;

        curList = &dev->firstList[dev->curList];
        curDMAAddr = (unsigned int )kmap_atomic(curList->page, KM_USER0) + curList->offset;

        curCount = dev->curCount;
        loop = curCount / 255;

        if (down_interruptible(&dmac_sem)) {
                return(-2);
        }
        for (i=0; i<loop; i++) {
                _fmi_bIsSDDataReady = FALSE;
                sdcr = (inl(REG_SDCR)& 0xff00c080) | (255 << 16);

                curDMAAddr = curDMAAddr + copySize;
                listLenth = (curList->length) - copySize;
                DMAoffset=0;
                copySize = 255 * DMA_BLOCK_SIZE;


                while (copySize > listLenth) {
                        memcpy((char*)dev->DMAvaddr+DMAoffset,(char*)curDMAAddr,listLenth);
                        dev->curList ++;
                        curList = &dev->firstList[dev->curList];
                        curDMAAddr = (unsigned int )kmap_atomic(curList->page, KM_USER0) +	curList->offset;

                        DMAoffset += listLenth;
                        copySize = copySize-listLenth;
                        listLenth = curList->length;

                }

                if ( copySize < listLenth) {
                        memcpy((char*)dev->DMAvaddr+DMAoffset,(char*)curDMAAddr,copySize);
                }
                if ( copySize == listLenth) {
                        memcpy((char*)dev->DMAvaddr+DMAoffset,(char*)curDMAAddr,copySize);

                        if ((i!=(loop -1 )) || ((curCount % 255)!=0)) {
                                dev->curList ++;
                                curList = &dev->firstList[dev->curList];
                                curDMAAddr = (unsigned int )kmap_atomic(curList->page, KM_USER0) +	curList->offset;
                                copySize=0;
                        }
                }

                while (inl(REG_DMACCSR)&FMI_BUSY); //Wait IP finished
                outl(dev->DMApaddr, REG_DMACSAR);

                if (!bIsSendCmd) {
                        time = jiffies;  //ya
                        outl(sdcr|(25<<8)|0xb, REG_SDCR);
                        bIsSendCmd = TRUE;
                        //while(inl(REG_SDCR) & 0x3);
                        while ((inl(REG_SDCR) & 0x3) && (time_after(jiffies, time+HZ))); //ya
                } else
                        outl(sdcr | 0x08,REG_SDCR);
#if 0
                while (!_fmi_bIsSDDataReady) {
                        if (flagSetting.bCardExist == 0)
                                goto write_exit;

                }
#else
                outl(0x02, REG_SDISR);	// chp
                wait_event_interruptible(sd_rw, _fmi_bIsSDDataReady || (flagSetting.bCardExist == 0) || (card_detect == 0));
                if (flagSetting.bCardExist == 0 || card_detect == 0) {
                        dev->sense = SS_MEDIUM_NOT_PRESENT;   //ya
                        outl(3, REG_DMACCSR); // reset DMA
                        outl((1 << 14), REG_SDCR);  //reset FMI
                        //outl(1, REG_SMCSR);  //reset FMI
                        ret = -1;
                        goto write_exit;
                }

#endif
                if ((inl(REG_SDISR) & 0x02)!= 0) { // chp
                        printk("SD_Write: CRC16_1 error![0x%x]\n", inl(REG_SDISR));
                        outl(0x02, REG_SDISR);	// chp
                }

        }


        loop = curCount % 255;
        if (loop !=0) {
                _fmi_bIsSDDataReady = FALSE;
                sdcr = (inl(REG_SDCR)& 0xff00c080)| (loop << 16);

                curDMAAddr = curDMAAddr + copySize;
                listLenth = (curList->length) - copySize;
                DMAoffset=0;
                copySize = loop * DMA_BLOCK_SIZE;

                while (copySize > listLenth) {
                        memcpy((char*)dev->DMAvaddr+DMAoffset,(char*)curDMAAddr,listLenth);
                        dev->curList ++;
                        curList = &dev->firstList[dev->curList];
                        curDMAAddr = (unsigned int )kmap_atomic(curList->page, KM_USER0) +	curList->offset;
                        DMAoffset += listLenth;
                        copySize = copySize-listLenth;
                        listLenth = curList->length;

                }

                if ( copySize < listLenth) {
                        memcpy((char*)dev->DMAvaddr+DMAoffset,(char*)curDMAAddr,copySize);
                }

                if ( copySize == listLenth) {
                        memcpy((char*)dev->DMAvaddr+DMAoffset,(char*)curDMAAddr,copySize);
                }


                while (inl(REG_DMACCSR)&FMI_BUSY); //Wait IP finished
                outl(dev->DMApaddr, REG_DMACSAR);

                if (!bIsSendCmd) {
                        time = jiffies;
                        outl(sdcr|(25<<8)|0xb, REG_SDCR);
                        bIsSendCmd = TRUE;
                        //while(inl(REG_SDCR) & 0x3);
                        while ((inl(REG_SDCR) & 0x3) && (time_after(jiffies, time+HZ))); //ya
                } else
                        outl(sdcr | 0x08,REG_SDCR);
#if 0
                while (!_fmi_bIsSDDataReady) {
                        if (flagSetting.bCardExist == 0)
                                goto write_exit;
                }
#else
                outl(0x02, REG_SDISR);	// chp
                wait_event_interruptible(sd_rw, _fmi_bIsSDDataReady || (flagSetting.bCardExist == 0) || card_detect == 0);
                if (flagSetting.bCardExist == 0 || card_detect == 0) {
                        dev->sense = SS_MEDIUM_NOT_PRESENT;   //ya
                        outl(3, REG_DMACCSR); // reset DMA
                        outl((1 << 14), REG_SDCR);  //reset FMI
                        //outl(1, REG_SMCSR);  //reset FMI
                        ret = -1;

                        goto write_exit;
                }

#endif
                if ((inl(REG_SDISR) & 0x02) != 0) {
                        printk("SD_Write: CRC16_2 error![0x%x]\n", inl(REG_SDISR));
                        outl(0x02, REG_SDISR);	// chp
                }
        }

write_exit:
        up(&dmac_sem);
        return(ret);
}


static int sd_read_sectors(struct sd_hostdata *dev, int lba, int count)
{
        unsigned int addr;

        ENTER();

        if (!(dev->cardType & CARD_TYPE_SDHC))
                addr = lba * SD_BLOCK_SIZE;
        else
                addr = lba;

        dev->curCount = count;
        dev->sense = SS_NO_SENSE;
        if (card_detect == 0) { //ya
                dev->sense = SS_MEDIUM_NOT_PRESENT;
                return(1);
        }

        MSG2("--Read from = %x(lba) %x(addr), length = %d\n", lba, addr, count);
        if ( count == 0)
                return 0;

        //if (down_interruptible(&fmi_sem))
        //        return SD_FAILED;
        dev->state = SD_STATE_READ;
        sdCheckRB();
        outl(0x1ff,REG_SDBLEN);	// 512 bytes
        outl(addr,REG_SDARG);	/* set argument */
        sd_start_read(dev);
        sd_card_stop(dev);
        sdCheckRB();
        //up(&fmi_sem);
        sd_done(dev);	/* scsi done */
        dev->state = SD_STATE_NOP;

        LEAVE();

        return 0;
}


static int sd_write_sectors(struct sd_hostdata *dev, int lba, int count)
{
        unsigned int addr;

        ENTER();
        if (!(dev->cardType & CARD_TYPE_SDHC))
                addr = lba * SD_BLOCK_SIZE;
        else
                addr = lba;
        dev->curCount = count;
        dev->sense = SS_NO_SENSE;
        if (card_detect == 0) { //ya
                dev->sense = SS_MEDIUM_NOT_PRESENT;
                return(1);
        }
        MSG2("--Write to = %x(lba) %x(addr), length = %d\n", lba, addr, count);

        if ( count == 0) {
                return 0;
        }
        dev->state = SD_STATE_WRITE;

        //if (down_interruptible(&fmi_sem))
        //        return SD_FAILED;
        sdCheckRB();
        outl(0x1ff,REG_SDBLEN);	// 512 bytes
        outl(addr,REG_SDARG);	/* set argument */
        sd_start_write(dev);
        sd_card_stop(dev);
        sdCheckRB();
        //up(&fmi_sem);
        sd_done(dev);	/* scsi done */
        dev->state = SD_STATE_NOP;

        LEAVE();

        return 0;
}


static irqreturn_t sd_interrupt( int irq, void *dev_id, struct pt_regs *regs )
{
        volatile int sdisr;
        //unsigned int aaa;
        //ENTER();


        sdisr=inl(REG_SDISR);

        if ((sdisr & /*0x3F0F*/ 0x3F03) == 0) {
                //printk("sd %x\n", sdisr);
                return IRQ_NONE;
        }
        //printk("11 sd %x\n", sdisr);
        if (sdisr & SDISR_BLKD_IF) {	// block down
                _fmi_bIsSDDataReady = TRUE;
                outl(SDISR_BLKD_IF,REG_SDISR);//clear interrupt
                wake_up_interruptible(&sd_rw);
                return IRQ_HANDLED;

        }


        if (sdisr & SDISR_CD_IF) { //sd0 card inserted or removed
                //printk("XXX\n");
                del_timer(&sd_timer);
                outl(inl(REG_SDIER) & (~SDIER_CD_IEN), REG_SDIER);	// disable card detect interrupt
                sd_timer.data = 0UL;
                sd_timer.expires = jiffies +  SD_SHORT_DELAY;
                sd_timer.function = sd_Short_TimeOut;
                add_timer(&sd_timer);

                outl(SDISR_CD_IF,REG_SDISR);
        }



        //LEAVE();

        return IRQ_HANDLED;

}

static void sd_check_valid_medium(struct sd_hostdata *dev)
{
        ENTER();

        if (flagSetting.bCardExist == 0 || card_detect == 0) {
                MSG( "card not exist\n" );
                return;
        }

        if (flagSetting.bMediaChanged != 0 ) {
                flagSetting.bMediaChanged = 0;
                if (!sd_card_init())
                        flagSetting.bInitSuccess = 1;
                else
                        flagSetting.bInitSuccess = 0;
        }

        LEAVE();

}


static unsigned char *sd_get_buffer(struct scsi_cmnd *cmd, int * length)
{
        unsigned char * buf;
        struct scatterlist *p;

        if ( cmd->use_sg  == 0) {
                buf = cmd->request_buffer;
                *length = cmd->request_bufflen;
        } else {
                p = (struct scatterlist *)cmd->request_buffer;
                buf =(unsigned char *)kmap_atomic(p->page, KM_USER0) +	p->offset;
                *length = p->length;
        }

        return buf;
}


static int sd_test_unit_ready(struct sd_hostdata *dev)
{
//	ENTER();
        int retval;
        struct scsi_cmnd  * cmd;
        cmd = dev->cmd;


        sd_check_valid_medium(dev);
        retval = 0;

        if (flagSetting.bCardExist != 0 && card_detect != 0) {
                if (flagSetting.bMediaChanged == 0) {
                        if (flagSetting.bInitSuccess != 0) {

                                // SenseKey: SCSI_SENSE_NO_SENSE
                                // AdditionalSenseCode: SCSI_ADSENSE_NO_SENSE
                                sd_make_sense_buffer(dev,0x00, 0x00, 0x00 );

                                retval = 0;
                        } else {
                                // SenseKey: SCSI_SENSE_MEDIUM_ERROR
                                // AdditionalSenseCode: SCSI_ADSENSE_INVALID_MEDIA
                                sd_make_sense_buffer(dev,0x03, 0x30, 0x00 );
                                retval = 1;
                        }
                } else {
                        flagSetting.bMediaChanged = 0;
                        // SenseKey: SCSI_SENSE_UNIT_ATTENTION
                        // AdditionalSenseCode: SCSI_ADSENSE_MEDIUM_CHANGED
                        sd_make_sense_buffer(dev,0x06, 0x28, 0x00 );
                        cmd->result = SD_CMD_RESULT(DID_NO_CONNECT, DISCONNECT, SAM_STAT_CONDITION_MET);
                        retval = 1;
                }
        } else {
                // SenseKey: SCSI_SENSE_NOT_READY
                // AdditionalSenseCode: SCSI_ADSENSE_NO_MEDIA_IN_DEVICE
                sd_make_sense_buffer(dev,0x02, 0x3a, 0x00 );

                // NOT_READY
                //sd_make_sense_buffer(dev,0x02, 0x00, 0x00 );
                //cmd->result = SD_CMD_RESULT(DID_NO_CONNECT, DISCONNECT, CHECK_CONDITION);
                retval = 1;
        }



        LEAVE();
        return retval;

}

static int sd_scsi_read(struct scsi_cmnd  *cmd , struct sd_hostdata *dev)
{
        unsigned int count, lba;

        ENTER();

        if ( sd_test_unit_ready(dev) )
                goto quit;



        if ( cmd->cmnd[0] == READ_6) {
                lba = ((cmd->cmnd[1] & 0x1f) << 16) + get_be16(&cmd->cmnd[2]);
                count = (cmd->cmnd[4] & 0xff);
        } else {
                lba = get_be32(&cmd->cmnd[2]);
                count = get_be16(&cmd->cmnd[7]);
        }

        if ( lba >/*=*/ dev->nTotalSectors || (lba + count) >/*=*/ dev->nTotalSectors) {
                dev->sense = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
                goto quit_with_make_sense;
        }

        dev->firstList = (struct scatterlist *)cmd->request_buffer;
        dev->nTotalLists = cmd->use_sg;

        dev->curList = 0;
        dev->sense = SS_NO_SENSE;

        wait_event_interruptible(sd_wq, dev->state == SD_STATE_NOP);

        if (sd_read_sectors(dev, lba, count))
                goto quit_with_make_sense;

        //	wait_event_interruptible(sd_wq, dev->state == SD_STATE_NOP);

        LEAVE();

        return 0;

quit_with_make_sense:
        sd_make_sense_buffer(dev,dev->sense >> 16,
                             dev->sense >> 8,
                             dev->sense);
quit:
        return -1;
}


static int sd_scsi_write(struct scsi_cmnd  *cmd, struct sd_hostdata *dev)
{
        unsigned int count, lba;

        ENTER();

        if ( sd_test_unit_ready(dev)) {
                goto quit;
        }

        if (flagSetting.bWriteProtect == 1) {
                dev->sense = SS_WRITE_PROTECTED;
                goto quit_with_make_sense;
        }

        if ( cmd->cmnd[0] == WRITE_6) {
                lba = ((cmd->cmnd[1] & 0x1f) << 16) + get_be16(&cmd->cmnd[2]);
                count = (cmd->cmnd[4] & 0xff);
        } else {
                lba = get_be32(&cmd->cmnd[2]);
                count = get_be16(&cmd->cmnd[7]);
        }

        if ( lba >/*=*/ dev->nTotalSectors || (lba + count) >/*=*/ dev->nTotalSectors) {
                dev->sense = SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE;
                goto quit_with_make_sense;
        }

        dev->firstList = (struct scatterlist *)cmd->request_buffer;
        dev->nTotalLists = cmd->use_sg;

        dev->curList = 0;
        dev->sense = SS_NO_SENSE;

        wait_event_interruptible(sd_wq, dev->state == SD_STATE_NOP);

        if (sd_write_sectors(dev, lba, count))
                goto quit_with_make_sense;

        wait_event_interruptible(sd_wq, dev->state == SD_STATE_NOP);

        LEAVE();

        return 0;

quit_with_make_sense:
        sd_make_sense_buffer(dev,dev->sense >> 16,
                             dev->sense >> 8,
                             dev->sense);
quit:

        return -1;
}



static void sd_scsi_start_stop(struct sd_hostdata *dev)
{

        struct scsi_cmnd  *cmd = dev->cmd;
        ENTER();

        if (cmd->cmnd[4] & 0x01) {		/* start */
                if (! sd_test_unit_ready(dev)) {
                        cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );
                }
        } else
                cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );

        LEAVE();
}


static void sd_scsi_request_sense(struct scsi_cmnd  *cmd)
{
        int len;
        unsigned char *buffer = sd_get_buffer(cmd, &len);

        ENTER();

        memcpy(buffer, cmd->sense_buffer, 18);

        cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );

        LEAVE();
}


static void sd_scsi_media_removal(struct sd_hostdata *dev)
{

        struct scsi_cmnd  *cmd = dev->cmd;
        ENTER();
        //prevent removal cmnd is illegal since SD card can be removable
        if ( ( cmd->cmnd[4] & 0x01 ) )  {
                // SenseKey: SCSI_SENSE_ILLEGAL_REQUEST
                // AdditionalSenseCode: SCSI_ADSENSE_ILLEGAL_COMMAND
                sd_make_sense_buffer(dev,0x05, 0x20, 0x00 );
        } else {
                cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );
        }

        LEAVE();

}

static void sd_scsi_test_unit_ready(struct sd_hostdata *dev)
{

        struct scsi_cmnd  *cmd = dev->cmd;
        ENTER();
        if (!sd_test_unit_ready(dev) )
                cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );

        LEAVE();
}

static void sd_scsi_inquiry(struct sd_hostdata *dev)
{
        int len;
        struct scsi_cmnd  *cmd = dev->cmd;
        unsigned char *buf = sd_get_buffer(cmd, &len);


        static char vendor_id[] = "NUVOTON";
        static char product_id[] = "SD/MMC Reader";
        static char release_id[]="2.00";

        ENTER();

        if (registered == 0) {

                registered = 1;
                // stuff necessary inquiry data

                memset(buf, 0, 36);
                buf[1] = 0x80;	/* removable */
                buf[2] = 0;		// ANSI SCSI level 2
                buf[3] = 2;		// SCSI-2 INQUIRY data format //2
                buf[4] = 0x1f;		// Additional length
                // No special options

                sprintf(buf + 8, "%-8s%-16s%-4s", vendor_id, product_id,
                        release_id);

                cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );
        } else
                cmd->result = SD_CMD_RESULT(DID_NO_CONNECT, 0x00, 0x00);




        LEAVE();
}

static void sd_scsi_mode_sense(struct sd_hostdata *dev)
{
        int bProtectFlag, len;
        struct scsi_cmnd  *cmd = dev->cmd;
        unsigned char *buf = sd_get_buffer(cmd, &len);
        ENTER();

        if ( sd_test_unit_ready(dev) )
                return;

        if ( flagSetting.bCardExist ) {

                memset(buf, 0, 8);

                bProtectFlag = 0;
                if (flagSetting.bWriteProtect)
                        bProtectFlag = 0x80;

                if ( cmd->cmnd[0] == MODE_SENSE ) {
                        buf[0] = 0x03;
                        buf[2] = bProtectFlag;
                } else {
                        buf[1] = 0x06;
                        buf[3] = bProtectFlag;

                }

                cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );
        } else { // card is not in
                sd_make_sense_buffer(dev, 0x02, 0x3a, 0x00 );
                MSG( "SD 0 card may not be inserted!\n" );
        }


        LEAVE();

}

static void sd_scsi_read_capacity(struct sd_hostdata *dev)
{
        struct scsi_cmnd  *cmd = dev->cmd;

        int len;
        unsigned char *buf = sd_get_buffer(cmd, &len);

        ENTER();

        if (sd_test_unit_ready(dev)) {
                MSG( "SCSI_READ_CAPACITY - The unit not ready\r\n" );
                return;
        }

        memset(buf, 0, 8);

        put_be32(dev->nTotalSectors - 1, &buf[0]);	// Max logical block
        put_be32(512, &buf[4]);				// Block length

        cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_GOOD );

        LEAVE();

        return;
}

static void sd_scsi_process_cmd(struct sd_hostdata *dev)
{
        struct scsi_cmnd  * cmd;

        //	ret = down_interruptible(&sem_r);

        if (down_interruptible(&fmi_sem)) {
                cmd = dev->cmd;
                cmd->result = SD_CMD_RESULT( DID_BUS_BUSY, 0, 0 );
                goto done;
        }

        outl(FMI_SD_EN, REG_FMICR);/* Enable SD functionality of FMI */


        cmd = dev->cmd;
        cmd->result = SD_CMD_RESULT( DID_OK, COMMAND_COMPLETE, SAM_STAT_CHECK_CONDITION );
        sd_check_valid_medium(dev);

        switch (cmd->cmnd[0]) {

        case START_STOP:
                PDEBUG("SC_START_STOP_UNIT\n");
                sd_scsi_start_stop(dev);
                break;

        case REQUEST_SENSE:

                PDEBUG("SC_PREVENT_ALLOW_MEDIUM_REMOVAL\n");
                sd_scsi_request_sense(cmd);
                break;

        case ALLOW_MEDIUM_REMOVAL:
                PDEBUG("SC_PREVENT_ALLOW_MEDIUM_REMOVAL\n" );
                sd_scsi_media_removal(dev);
                break;

        case TEST_UNIT_READY:
                PDEBUG( "SC_TEST_UNIT_READY\n" );
                sd_scsi_test_unit_ready(dev );
                break;

        case INQUIRY:
                PDEBUG( "SC_INQUIRY\n" );
                sd_scsi_inquiry( dev );
                break;

        case READ_6:
                PDEBUG( "R\n" );
                sd_scsi_read(cmd,dev);
                break;

        case READ_10:
                PDEBUG( "R\n" );
                sd_scsi_read(cmd,dev);
                break;

        case WRITE_6:
                PDEBUG( "W\n" );
                sd_scsi_write(cmd,dev);
                break;

        case WRITE_10:
                PDEBUG( "W\n" );
                sd_scsi_write(cmd,dev);
                break;

        case MODE_SENSE:
                PDEBUG( "SC_MODE_SENSE_6\n" );
                sd_scsi_mode_sense( dev );
                break;

        case MODE_SENSE_10:
                PDEBUG( "SC_MODE_SENSE_6\n" );
                sd_scsi_mode_sense( dev );
                break;


        case READ_CAPACITY:
                PDEBUG( "SC_READ_CAPACITY\n" );
                sd_scsi_read_capacity( dev );
                break;

        default:
                PDEBUG("UNKNOWN command : %02x\n", cmd->cmnd[0] );
                sd_make_sense_buffer(dev, ILLEGAL_REQUEST, 0x20, 0x00 );
                cmd->result = SD_CMD_RESULT( DID_OK, 0, 2);
                break;

        }

done:
        MSG2("Result : %08x\n", cmd->result);
        MSG2("Sense : [%02x%02x%02x]\n", cmd->sense_buffer[2],
             cmd->sense_buffer[12], cmd->sense_buffer[13]);

        if (cmd->result == 0) {
                MSG("Command Finished OK.\n");

                memset(cmd->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);
        }
        cmd->scsi_done(cmd);
        up(&fmi_sem);
        //  up(&sem_r);

        LEAVE();

}

static int sd_queue_cmd( struct scsi_cmnd  *cmd, void (* done )( struct scsi_cmnd  * ) )
{
        int ret;
        ENTER();
        if (done == NULL)
                return 0;

        if (cmd->device->lun > 0) {
                cmd->result = SD_CMD_RESULT(DID_NO_CONNECT, 0, 0);
                done(cmd);
                return 0;
        }

        ret = down_interruptible(&sem);

        sd_host.cmd = cmd;
        cmd->scsi_done = done;
        cmd->result = 0;
        has_command = 1;

        wake_up_interruptible(&scsi_wq);

        up(&sem);

        LEAVE();

        return 0;
}


static int sd_kernel_thread(void *param)
{
        ENTER();

        daemonize("sdiod0");

        for (;;) {

                wait_event_interruptible(scsi_wq, (has_command != 0));
                if (has_command == SD_EVENT_QUIT)
                        break;

                has_command = 0;
                schedule();
                sd_scsi_process_cmd(&sd_host);
                schedule();
        }

        MSG("BUG : SD0 Kernel Thread Quit\n");
        printk("event quit 0 \n");

        return 0;
}




static const char* sd_info( struct Scsi_Host * psh)
{
        ENTER();

        LEAVE();

        return "Nuvoton SD Reader 2.0 for W55FA93";
}


static int sd_abort(struct scsi_cmnd * cmd)
{
        struct sd_hostdata *dev = &sd_host;

        ENTER();

        if (flagSetting.bCardExist && dev->state != SD_STATE_NOP) {

                sd_card_stop(dev);
                dev->sense = SS_COMMUNICATION_FAILURE;
                cmd->scsi_done(cmd);
                wake_up_interruptible(&sd_wq);

                return 0;
        }

        LEAVE();

        return 1;
}





static int sd_reset( struct scsi_cmnd *cmd)
{
        ENTER();

        outl(inl(REG_DMACCSR) | DMAC_SWRST, REG_DMACCSR);
        if (down_interruptible(&fmi_sem))
                return SD_FAILED;
        sd_host_reset();
        up(&fmi_sem);

        LEAVE();

        return 0;
}



static int sd_bios_param(struct scsi_device *sdev,
                         struct block_device *bdev, sector_t capacity, int *info)
{

        ENTER();

        info[0] = 2;		// heads

        info[1] = 61;		// sectors

        info[2] = capacity >> 7;

        LEAVE();

        return 0;
}



static int sd_ioctl(struct scsi_device *scsi_dev, int cmd, void  *arg)
{


        return 0;
}

static struct scsi_host_template driver_template = {
        .name 					          = "SD0",
        .info					            = sd_info,
        .queuecommand			        = sd_queue_cmd,
        .eh_abort_handler 	    	= sd_abort,
        .eh_host_reset_handler		= sd_reset,
        .bios_param				        = sd_bios_param,
        .ioctl					          = sd_ioctl,
        .can_queue	     			    = 1,
        .this_id        				  = -1,
        .sg_tablesize   			    = 128,
        .cmd_per_lun    			    = 1,
        .unchecked_isa_dma		    = 0,
        .use_clustering  			    = ENABLE_CLUSTERING,
        .module					          = THIS_MODULE,
};

MODULE_LICENSE( "GPL" );



static void sd_device_release(struct device * dev)
{
        ENTER();

        LEAVE();
}

static struct device sd_device = {
        .bus_id = "sd_bus",
        .release = sd_device_release,
};

static int sd_bus_match(struct device *dev, struct device_driver *dev_driver)
{
        ENTER();

        LEAVE();

        return 1;
}

static struct bus_type sd_bus = {
        .name = "sd_bus",
        .match = sd_bus_match,
};

static int sd_driver_probe(struct device *dev)
{
        struct Scsi_Host *shp;

        ENTER();

        //sd_check_valid_medium(dev);

        shp = scsi_host_alloc(&driver_template, 0);
        if ( shp == NULL) {
                printk(KERN_ERR "%s: scsi_register failed\n", __FUNCTION__);
                return -ENODEV;
        }

        if ( scsi_add_host(shp, &sd_host.dev) ) {
                printk(KERN_ERR "%s: scsi_add_host 0 failed\n", __FUNCTION__);
                scsi_host_put(shp);
                return -ENODEV;
        }

        scsi_scan_host(shp);

        sd_host.shost = shp;

        LEAVE();

        return 0;
}

static int sd_driver_remove(struct device *dev)
{
        ENTER();

        scsi_remove_host(sd_host.shost);
        scsi_host_put(sd_host.shost);


        LEAVE();

        return 0;
}

static struct device_driver sd_driver = {
        .name 		= "sd_scsi",
        .bus			= &sd_bus,
        .probe          = sd_driver_probe,
        .remove         = sd_driver_remove,
};

static void sd_release_host(struct device *dev)
{
        ENTER();

        LEAVE();
}



static int sd_add_card(void)
{

        int err;
        ENTER();


        if (sd_add == 0 ) { //add card 0

                sd_add = 1;
                memset(&sd_host.dev, 0, sizeof(struct device ));
                if (alloc0 == 0) {
                        sd_host.DMAvaddr = (int )dma_alloc_writecombine(NULL, (256 * DMA_BLOCK_SIZE), &sd_host.DMApaddr, GFP_KERNEL);
                        alloc0 = 1;
                }
                sd_host.dev.bus = &sd_bus;
                sd_host.dev.parent = &sd_device;
                sd_host.dev.release = sd_release_host;
                sprintf(sd_host.dev.bus_id, "card0");

                err = device_register(&sd_host.dev);
                if (err)
                        return err;

                LEAVE();

                return 0;
        }

        if (flagSetting.update == 1)
                flagSetting.update = 0;
        return 0;

}

static void sd_del_card(void)
{
        ENTER();

        if (sd_add ) {
                device_unregister(&sd_host.dev);
                sd_add = 0;
                registered = 0;

//		dma_free_writecombine(NULL, (256 * DMA_BLOCK_SIZE), (void *)sd_host.DMAvaddr, sd_host.DMApaddr);

        }


        LEAVE();
}

static int sd_event_thread(void *unused)
{
        int event;

        ENTER();

        daemonize("sdioeventd");

        for (;;) {

                wait_event_interruptible(sd_event_wq, sd_event != SD_EVENT_NONE);
                event = sd_event;
                sd_event = 0;
                //	  ret = down_interruptible(&sem_w);

                switch (event) {
                case SD_EVENT_ADD:
                        if (flagSetting.bCardExist == 1) { // too busy to handle previous remove event
                                printk("orz!!!!\n");
                                while (sd_host.state != SD_STATE_NOP) schedule();
                                //printk("call del\n");
                                flagSetting.bCardExist = 0;
                                flagSetting.bMediaChanged = 0;
                                sd_del_card();

                        }
                        flagSetting.bCardExist = 1;
                        flagSetting.bMediaChanged = 1;
                        //flagSetting.bWriteProtect = 0;
                        flagSetting.update = 1;
                        card_detect = 1; //ya
                        sd_add_card();
#if defined(CONFIG_W55FA93_SYSMGR) || defined(CONFIG_W55FA93_SYSMGR_MODULE)
                        sysmgr_report(SYSMGR_STATUS_SD_INSERT);
#endif

                        break;

                case SD_EVENT_REMOVE:

                        flagSetting.bCardExist = 0;
                        wake_up_interruptible(&sd_wq);
                        flagSetting.bMediaChanged = 0;
                        //outl(inl(REG_PINFUN) & ~PINFUN_SDHPIN_EN, REG_PINFUN);  //ya
                        sd_del_card();
                        //outl(inl(REG_PINFUN) & ~PINFUN_SDHPIN_EN, REG_PINFUN);  //ya
                        msleep(1000);  //ya, force SD pin stay low for a while
                        break;

                case SD_EVENT_QUIT:
                        goto quit;

                default:
                        MSG("NO THIS EVENT !!\n");
                        break;
                }

                //up(&sem_w);
        }

quit:
        MSG("Quit Event Thread\n");

        return 0;

}




static int __init sd_init(void)
{
        int ret;

        ENTER();

        // check SPI pin used?
//        if (inl(REG_PINFUN) & PINFUN_SPI1PIN_EN) {
//                printk("SPI pin used\n");
//                return -1;
//        }
        
#ifdef CONFIG_NUVOTON_W55FA93_SD_PWR
        // enable GPD, use GPD0 as card power
	        
        outl(inl(REG_GPDFUN) & ~MF_GPD0, REG_GPDFUN);	
        outl(inl(REG_GPIOD_DOUT) | 1, REG_GPIOD_DOUT);
        outl(inl(REG_GPIOD_OMD) | 1, REG_GPIOD_OMD);

#else	// POR procedure
//		outl((inl(REG_GPEFUN)&(~0x0000FFF0)) | 0x0000aaa0, REG_GPEFUN);	// SD0_CLK/CMD/DAT0_3 pins selected
		outl(inl(REG_GPEFUN)&(~0x0000FFF0), REG_GPEFUN);	// switch SD0_CLK/CMD/DAT0_3 pins to be GPIO mode
        outl(inl(REG_GPIOE_DOUT) & ~0xFC, REG_GPIOE_DOUT);	// GPIO pins are pulled low	
        outl(inl(REG_GPIOE_OMD) | 0xFC, REG_GPIOE_OMD);		// GPIO pins are output mode

        udelay(1000);
        
		outl((inl(REG_GPEFUN)&(~0x0000FFF0)) | 0x0000aaa0, REG_GPEFUN);	// SD0_CLK/CMD/DAT0_3 pins selected        
        // ### switch back SD function as while 1st cmd sd_host_reset
#endif

#ifdef CONFIG_NUVOTON_W55FA93_SD_WP
        outl(inl(REG_GPAFUN) & ~MF_GPA0, REG_GPAFUN);	
        outl(inl(REG_GPIOA_OMD) & ~(1), REG_GPIOA_OMD);
#endif

        //Enable IP I/O pins
		outl((inl(REG_GPEFUN)&(~0x0000FFF0)) | 0x0000aaa0, REG_GPEFUN);	// SD0_CLK/CMD/DAT0_3 pins selected        
		outl(inl(REG_SDCR) & (~SDCR_SDPORT), REG_SDCR);	        		// SD-0 port selected

        // Enable SD Card Host Controller operation and driving clock.
		outl(inl(REG_AHBCLK) | SIC_CKE | SD_CKE, REG_AHBCLK);	// enable SD engine clock 		

		// Enable SD card detect pin 
		outl(inl(REG_GPAFUN) | MF_GPA1, REG_GPAFUN);			// enable SD card detect pin (GPA1)
        outl(inl(REG_SDIER) | SDIER_CDSRC,REG_SDIER);			// SD card detection from GPIO

        //Reset FMI
        outl(FMI_SWRST, REG_FMICR);		// Start reset FMI controller.
        outl(FMI_SD_EN, REG_FMICR);		// Stop reset FMI controller

        // Disable FMI/SD host interrupt
        outl(0, REG_FMIIER);

#if 0
        //Set GPIO output
        outl((GPIO_DOUT0 & 0x0000FFFF) | (inl(REG_GPIOA_OMD) & 0x0000FFFF), REG_GPIOA_OMD);
        outl((inl(REG_GPIOA_OMD) & (~GPIO_DOUT0))& 0x0000FFFF, REG_GPIOA_DOUT);
#endif
        ret = device_register(&sd_device);
        ret = bus_register(&sd_bus);
        ret = driver_register(&sd_driver);

        driver_template.proc_name = "sd_scsi_0";

        if (request_irq(IRQ_SIC, sd_interrupt, SA_SHIRQ|SA_INTERRUPT, "W55FA93_SD", (void*)1 /* dummy value*/)) {
                printk("SD : Request IRQ error.\n");
                return -1;
        }

        Enable_IRQ(IRQ_SIC);
        //Enable_IRQ(IRQ_DMAC);

        outl(inl(REG_SDIER) | SDIER_CD_IEN, REG_SDIER); //Card detect interrupt


        kernel_thread(sd_event_thread, NULL, 0);
        kernel_thread(sd_kernel_thread, NULL, 0);



        init_timer(&sd_timer);
        sd_host.state = 0;
        sd_host.sense = 0;
        flagSetting.bCardExist=0;


        if (!(inl(REG_SDISR) & SDISR_CD_Card)) {
#ifdef CONFIG_NUVOTON_W55FA93_SD_PWR // need to enable power eariler if card detet during power up.
                outl(inl(REG_GPIOD_DOUT) & ~1, REG_GPIOD_DOUT);
#endif
//		sd_init_Detect =1;
                del_timer(&sd_timer);
                sd_timer.data = 0UL;
                sd_timer.expires = jiffies +  SD_SHORT_DELAY;
                sd_timer.function = sd_Short_TimeOut;
                add_timer(&sd_timer);
                //outl(inl(REG_GPIOA_DOUT) & ~0x7E, REG_GPIOA_DOUT);  //ya
                //outl(inl(REG_GPIOA_OMD) | 0x7E, REG_GPIOA_OMD);		//ya
        }


        printk("W55FA93 SD Card driver has been initialized successfully!\n");



        LEAVE();

        return 0;
}

static void __exit sd_exit(void)
{
        ENTER();

        free_irq(IRQ_SIC, NULL);

        driver_unregister(&sd_driver);
        bus_unregister(&sd_bus);
        device_unregister(&sd_device);

        LEAVE();
}

module_init(sd_init);
module_exit(sd_exit);

