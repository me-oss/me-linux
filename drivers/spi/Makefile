#
# Makefile for kernel SPI drivers.
#

ifeq ($(CONFIG_SPI_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif

# small core, mostly translating board-specific
# config declarations into driver model code
obj-$(CONFIG_SPI_MASTER)		+= spi.o

# SPI master controller drivers (bus)
obj-$(CONFIG_SPI_BITBANG)		+= spi_bitbang.o
obj-$(CONFIG_SPI_BUTTERFLY)		+= spi_butterfly.o
obj-$(CONFIG_SPI_PXA2XX)		+= pxa2xx_spi.o
obj-$(CONFIG_SPI_MPC83xx)		+= spi_mpc83xx.o
obj-$(CONFIG_SPI_S3C24XX_GPIO)		+= spi_s3c24xx_gpio.o
obj-$(CONFIG_SPI_S3C24XX)		+= spi_s3c24xx.o
obj-$(CONFIG_SPI_W55FA93)		+= spi_w55fa93.o
# 	... add above this line ...

# SPI protocol drivers (device/link on bus)
# 	... add above this line ...

# SPI slave controller drivers (upstream link)
# 	... add above this line ...

# SPI slave drivers (protocol for that link)
# 	... add above this line ...
