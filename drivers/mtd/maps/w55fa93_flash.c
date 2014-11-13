/*
 * drivers/mtd/maps/w55fa93_flash.c
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
 *   2007/08/23     vincen.zswan add the file to support winbond NOR from CFI_PROBE.
 */
 
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/errno.h>
#include <asm/arch/w55fa93_reg.h>


#define FLASH_BASE_ADDR 0xA0000000
#define FLASH_BANK_SIZE (4*1024*1024)

MODULE_AUTHOR("zswan, nuvoton Corporation <zswan@nuvoton.com>");
MODULE_DESCRIPTION("User-programmable flash device on the nuvoton w55fa93 ev board");
MODULE_LICENSE("GPL");

static struct map_info w55fa93nor_map = {
	.name		= "w55fa93nor",
	.bankwidth	= 2,
	.size		=  4 * 1024 * 1024
};


static struct mtd_partition w55fa93nor_partitions[] = {
	{
		name:       "W55FA93 NOR Partition 0 (2M)",
		size:       0x200000,
		offset:     0x0000000,
	},
        {
	  name:       "W55FA93 NOR Partition 1 (2M)",
	  size:       0x200000,
	  offset:     0x200000,
        }

};

static struct mtd_info *this_mtd;

static int __init init_w55fa93nor(void)
{
	struct mtd_partition *partitions;
	int num_parts = ARRAY_SIZE(w55fa93nor_partitions);

	outl(0x4006fff6, REG_ROMCON);
	partitions = w55fa93nor_partitions;

	w55fa93nor_map.virt = ioremap(FLASH_BASE_ADDR, w55fa93nor_map.size);

	if (w55fa93nor_map.virt == 0) {
		printk("Failed to ioremap FLASH memory area.\n");
		return -EIO;
	}

	simple_map_init(&w55fa93nor_map);

	this_mtd = do_map_probe("cfi_probe", &w55fa93nor_map);
	if (!this_mtd)
	{
	  printk("probe failed\n");
		iounmap((void *)w55fa93nor_map.virt);
		return -ENXIO;
	}

	printk("w55fa93 flash device: %dMiB at 0x%08x\n",
		   this_mtd->size >> 20, FLASH_BASE_ADDR);

	this_mtd->owner = THIS_MODULE;
	add_mtd_partitions(this_mtd, partitions, num_parts);

	return 0;
}

static void __exit cleanup_w55fa93nor(void)
{
	if (this_mtd)
	{
		del_mtd_partitions(this_mtd);
		map_destroy(this_mtd);
	}

	if (w55fa93nor_map.virt)
	{
		iounmap((void *)w55fa93nor_map.virt);
		w55fa93nor_map.virt = 0;
	}

	return;
}

module_init(init_w55fa93nor);
module_exit(cleanup_w55fa93nor);
