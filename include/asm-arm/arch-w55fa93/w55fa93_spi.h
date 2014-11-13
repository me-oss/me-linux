/* linux/include/asm-arm/arch-nuc900/nuc900_spi.h
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
 *   2006/08/26     vincen.zswan add this file for nuvoton nuc900 evb.
 */

#ifndef _NUC900_USI_H_
#define _NUC900_USI_H_

#include <linux/types.h>
#include <linux/ioctl.h>



struct spi_parameter
{
	unsigned int active_level:1;
	unsigned int lsb:1, tx_neg:1, rx_neg:1, divider:16;
	unsigned int sleep:4;
};

struct spi_data
{
	unsigned int write_data;
	unsigned int read_data;
	unsigned int bit_len;
};

#define SPI_MAJOR		231

#define SPI_IOC_MAGIC			'u'
#define SPI_IOC_MAXNR			3

#define SPI_IOC_GETPARAMETER	_IOR(SPI_IOC_MAGIC, 0, struct usi_parameter *)
#define SPI_IOC_SETPARAMETER	_IOW(SPI_IOC_MAGIC, 1, struct usi_parameter *)
#define SPI_IOC_SELECTSLAVE	_IOW(SPI_IOC_MAGIC, 2, int)
#define SPI_IOC_TRANSIT			_IOW(SPI_IOC_MAGIC, 3, struct usi_data *)

#endif /* _NUC900_USI_H_ */

