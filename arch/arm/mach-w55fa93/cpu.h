/* arch/arm/mach-w55fa93/cpu.h
 *
 * Based on linux/include/asm-arm/plat-s3c24xx/cpu.h by Ben Dooks
 *
 * Copyright (c) 2008 Nuvoton technology corporation
 * All rights reserved.
 *
 * Header file for W55FA93 CPU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#define IODESC_ENT(y) { (unsigned long)W55FA93_VA_##y, __phys_to_pfn(W55FA93_PA_##y), W55FA93_SZ_##y, MT_DEVICE }

#ifndef MHZ
#define MHZ (1000*1000)
#endif

#define print_mhz(m) ((m) / MHZ), ((m / 1000) % 1000)

/* forward declaration */
struct w55fa93_uartcfg;
struct map_desc;

/* core initialisation functions */

extern void w55fa93_init_irq(void);

extern void w55fa93_init_io(struct map_desc *mach_desc, int size);

extern void w55fa93_init_uarts(struct w55fa93_uartcfg *cfg, int no);

extern void w55fa93_init_clocks(int xtal);

extern  int init_w55fa93(void);

extern void map_io_w55fa93(struct map_desc *mach_desc, int size);

extern void init_uarts_w55fa93(struct w55fa93_uartcfg *cfg, int no);

extern void init_clocks_w55fa93(int xtal);

extern struct w55fa93_board w55fa93_board;

/* the board structure is used at first initialsation time
 * to get info such as the devices to register for this
 * board. This is done because platfrom_add_devices() cannot
 * be called from the map_io entry.
*/

struct w55fa93_board {
	struct platform_device  **devices;
	unsigned int              devices_count;

	struct clk		**clocks;
	unsigned int		  clocks_count;
};

struct cpu_table {
	unsigned long	idcode;
	unsigned long	idmask;
	void		(*map_io)(struct map_desc *mach_desc, int size);
	void		(*init_uarts)(struct w55fa93_uartcfg *cfg, int no);
	void		(*init_clocks)(int xtal);
	int		(*init)(void);
	const char	*name;
};


struct sys_timer;
extern struct sys_timer w55fa93_timer;

