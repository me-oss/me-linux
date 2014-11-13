/* linux/driver/input/w55fa93_keypad_2x4_input.c
 *
 * Copyright (c) 2010 Nuvoton technology corporation
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Changelog:
 *
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <asm/errno.h>
#include <asm/delay.h>
#include <linux/mm.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <asm/arch/irqs.h>
#include <linux/interrupt.h>
#include  <linux/completion.h>
#include <asm/arch/w55fa93_keypad.h>
#include <asm/arch/w55fa93_reg.h>
#include <asm/arch/regs-clock.h>
#undef BIT
#include <linux/input.h>

#define DEF_KPD_DELAY           HZ/20 

#define w55fa93_DEBUG //printk

static struct input_dev *w55fa93_keypad_input_dev;

static u32 open_cnt = 0;


const int key_map[8] [8]= {     
		{0, 1, 2, 3, 4, 5, 6, 7},
		{10, 11, 12, 13, 14, 15, 16, 17},	
		{20, 21, 22, 23, 24, 25, 26, 27},
		{30, 31, 32, 33, 34, 35, 36, 37}, 
		{40, 41, 42, 43, 44, 45, 46, 47}, 
		{50, 51, 52, 53, 54, 55, 56, 57}, 
		{60, 61, 62, 63, 64, 65, 66, 67}, 
		{70, 71, 72, 73, 74, 75, 76, 77}, 	
		
};


static void read_key(UINT32 u32KPIStatus)
{
	UINT32 u32KeyEvent, u32Tmp;
	UINT8 i, j, u8TmpRow;
	
	u8TmpRow = (((CONFIG_INPUT_KEYPAD_ROW) < (4)) ? (CONFIG_INPUT_KEYPAD_ROW) : (4));
		
	if((u32KPIStatus & PKEY_INT) == PKEY_INT)
	{	
		u32KeyEvent = inp32(REG_KPIKPE0);			
		
		if (u32KeyEvent != 0)
		{
			u32Tmp = u32KeyEvent;
			for (i=0; i<u8TmpRow; i++)
			{
				for (j=0; j<CONFIG_INPUT_KEYPAD_COLUMN; j++)
				{
					if(u32Tmp & 1<<j)
					{					
						w55fa93_DEBUG("p%d,%d\n", i, j);					
						input_report_key(w55fa93_keypad_input_dev, key_map[i][j], 1);	//key down
					}					
				}
				u32Tmp = u32Tmp>>8; 
			}
			outp32(REG_KPIKPE0 , u32KeyEvent);
		}

		u32KeyEvent = inp32(REG_KPIKPE1);			
		
		if (u32KeyEvent != 0)
		{
			i = 4;	
			u32Tmp = u32KeyEvent;
			do
			{
				for (j=0; j<CONFIG_INPUT_KEYPAD_COLUMN; j++)
				{
					if(u32Tmp & 1<<j)
					{					
						w55fa93_DEBUG("p%d,%d\n", i, j);					
						input_report_key(w55fa93_keypad_input_dev, key_map[i][j], 1);	//key down
					}					
				}
				u32Tmp = u32Tmp>>8; 
			}while(i++<CONFIG_INPUT_KEYPAD_ROW);
			
			outp32(REG_KPIKPE1 , u32KeyEvent);
		}	

		input_sync(w55fa93_keypad_input_dev);
	
	}

	if((u32KPIStatus & RKEY_INT) == RKEY_INT)
	{	
		u32KeyEvent = inp32(REG_KPIKRE0);			

		if (u32KeyEvent != 0)
		{			
			u32Tmp = u32KeyEvent;
			for (i=0; i<u8TmpRow; i++)
			{
				for (j=0; j<CONFIG_INPUT_KEYPAD_COLUMN; j++)
				{
					if(u32Tmp & 1<<j)
					{					
						w55fa93_DEBUG("r%d,%d\n", i, j);					
						input_report_key(w55fa93_keypad_input_dev, key_map[i][j], 0);	//key up
					}					
				}
				u32Tmp = u32Tmp>>8; 
			}	
			outp32(REG_KPIKRE0 , u32KeyEvent);
		}

		u32KeyEvent = inp32(REG_KPIKRE1);			

		if (u32KeyEvent != 0)
		{			
			i = 4;			
			u32Tmp = u32KeyEvent;
			do
			{
				for (j=0; j<CONFIG_INPUT_KEYPAD_COLUMN; j++)
				{
					if(u32Tmp & 1<<j)
					{					
						w55fa93_DEBUG("r%d,%d\n", i, j);					
						input_report_key(w55fa93_keypad_input_dev, key_map[i][j], 0);	//key up
					}					
				}
				u32Tmp = u32Tmp>>8; 
			}while(i++ < CONFIG_INPUT_KEYPAD_ROW);
			
			outp32(REG_KPIKRE1 , u32KeyEvent);
		}

		input_sync(w55fa93_keypad_input_dev);
	
	}			

}


static irqreturn_t w55fa93_kpd_irq(int irq, void *dev_id, struct pt_regs *regs)
{

	volatile UINT32 u32IntStatus;	

    	u32IntStatus = inp32(REG_KPISTATUS);	

	read_key(u32IntStatus);
        
       return IRQ_HANDLED;
}


int w55fa93_kpd_open(struct input_dev *dev)
{

        if (open_cnt > 0) {
                goto exit;
        }

        if (request_irq(IRQ_KPI, w55fa93_kpd_irq, SA_INTERRUPT, "Keypad",NULL) != 0) {
                printk("register the keypad_irq failed!\n");
                return -1;
        }

	outp32(REG_KPICONF , inp32(REG_KPICONF)  | ENKP );		// Keypad Scan Enable

exit:
        open_cnt++;
        return 0;
}



void w55fa93_kpd_close(struct input_dev *dev)
{
        open_cnt--;
        if (open_cnt == 0) {
                outp32(REG_KPICONF , inp32(REG_KPICONF) &~ ENKP );		// Keypad Scan disable
                free_irq(IRQ_KPI,NULL);
        }
        return;
}

static void Set_Kpd_CLKSrc(E_DEBOUNCECLK_SRC eCLKSrc,  UINT8 CLK_Div)
{
	outp32(REG_CLKDIV0 , (inp32(REG_CLKDIV0) & ~ KPI_S) | (eCLKSrc<<5));	
	outp32(REG_CLKDIV0 , (inp32(REG_CLKDIV0) & ~ KPI_N0) | ((CLK_Div & 0x0F)<<12));
	outp32(REG_CLKDIV0 , (inp32(REG_CLKDIV0) & ~ KPI_N1) | ((CLK_Div >> 4)<<21));
}

static void Set_Kpd_Pin(UINT8 u8Row, UINT8 u8Col)
{
	UINT8 u8count;
	UINT32 u32RowTmp = 0, u32ColTmp = 0;	

	for (u8count =0; u8count<=u8Row; u8count++)
		u32RowTmp = u32RowTmp | (0x03 <<(u8count*2 + 4));
	outp32(REG_GPAFUN , inp32(REG_GPAFUN) &~ u32RowTmp );

	u32RowTmp = 0;

	for (u8count =0; u8count<=u8Row; u8count++)
		u32RowTmp = u32RowTmp | (0x01 <<(u8count*2 + 4));
	outp32(REG_GPAFUN , inp32(REG_GPAFUN) | u32RowTmp );

	for (u8count =0; u8count<=u8Col; u8count++)
		u32ColTmp = u32ColTmp | (0x03 <<(u8count*2 + 16));
	outp32(REG_GPCFUN , inp32(REG_GPCFUN) &~ u32ColTmp );

	u32ColTmp = 0;

	for (u8count =0; u8count<=u8Col; u8count++)
		u32ColTmp = u32ColTmp | (0x02 <<(u8count*2 + 16));
	outp32(REG_GPCFUN , inp32(REG_GPCFUN) | u32ColTmp );
	
}

static void Set_Kpd_DeBounce(E_DEBOUNCE_ENABLE eDBEable,  E_DEBOUNCE_CLK eDBCLK_Select)
{
	outp32(REG_KPICONF , (inp32(REG_KPICONF) & ~ DB_EN) | (eDBEable<<21));	
	outp32(REG_KPICONF , (inp32(REG_KPICONF) & ~ DB_CLKSEL) | (eDBCLK_Select<<16));
}


static int __init w55fa93_kpd_reg(void)
{

	int i, err;
	
	outp32(REG_APBCLK , inp32(REG_APBCLK)  | KPI_CKE );

	Set_Kpd_CLKSrc(eDBCLK_XIN, 26);

	Set_Kpd_Pin(KPD_ROWNUM, KPD_COLNUM);

	Set_Kpd_DeBounce(eDB_ENABLE, eDEBOUNCE_2048CLK);

	outp32(REG_KPICONF , (inp32(REG_KPICONF) & ~ PRESCALE) | (0x80<<8));	
	outp32(REG_KPIPRESCALDIV , (inp32(REG_KPIPRESCALDIV) & ~ PRESCALDIV) | 0x1F);
	outp32(REG_KPICONF , (inp32(REG_KPICONF) & ~(KCOL | KROW)) |(KPD_COLNUM<<24) | (KPD_ROWNUM<<28) );
	outp32(REG_KPICONF , (inp32(REG_KPICONF) | (PKINTEN | RKINTEN )));	
	outp32(REG_KPICONF , inp32(REG_KPICONF)  | INTEN );
	outp32(REG_KPICONF , inp32(REG_KPICONF)  | ODEN );
	//outp32(REG_KPICONF , inp32(REG_KPICONF)  | ENKP );

	if (!(w55fa93_keypad_input_dev = input_allocate_device())) {
		printk("w55fa93 Keypad Drvier Allocate Memory Failed!\n");
		err = -ENOMEM;
		goto fail;
	}

	w55fa93_keypad_input_dev->name = "w55fa93 Keypad";
	w55fa93_keypad_input_dev->phys = "input/event1";
	w55fa93_keypad_input_dev->id.bustype = BUS_HOST;
	w55fa93_keypad_input_dev->id.vendor  = 0x0005;
	w55fa93_keypad_input_dev->id.product = 0x0001;
	w55fa93_keypad_input_dev->id.version = 0x0100;
	
	w55fa93_keypad_input_dev->open    = w55fa93_kpd_open;
	w55fa93_keypad_input_dev->close   = w55fa93_kpd_close;

	w55fa93_keypad_input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_SYN) |  BIT(EV_REP);
	
	for(i = 0; i < 240; i++)
		//set_bit(i+1, w55fa93_keypad_input_dev->keybit);	
		set_bit(i, w55fa93_keypad_input_dev->keybit);	

	err = input_register_device(w55fa93_keypad_input_dev);
	if (err) {

		input_free_device(w55fa93_keypad_input_dev);
		return err;
	}

	w55fa93_keypad_input_dev->rep[REP_DELAY] = 200; //250ms
	w55fa93_keypad_input_dev->rep[REP_PERIOD] = 100; //ms

	printk("w55fa93 keypad driver has been initialized successfully!\n");

	return 0;

fail:	
	input_free_device(w55fa93_keypad_input_dev);
	return err;	
}

static void __exit w55fa93_kpd_exit(void)
{
	free_irq(IRQ_KPI, NULL);	
	input_unregister_device(w55fa93_keypad_input_dev);
}

module_init(w55fa93_kpd_reg);
module_exit(w55fa93_kpd_exit);

MODULE_DESCRIPTION("w55fa93 keypad driver");
MODULE_LICENSE("GPL");
