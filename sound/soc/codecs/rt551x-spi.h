/*
 * rt551x-spi.h  --  ALC5514/ALC5518 driver
 *
 * Copyright 2015 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RT551X_SPI_H__
#define __RT551X_SPI_H__

#define RT551X_SPI_BUF_LEN		240
#define RT551X_RW_ADDR_START	0x4ff60000
//#define RT551X_RW_ADDR_END		0x4ffb8000
#define RT551X_RW_ADDR_END		0x4ffb0000

/* SPI Command */
enum {
	RT551X_SPI_CMD_16_READ = 0,
	RT551X_SPI_CMD_16_WRITE,
	RT551X_SPI_CMD_32_READ,
	RT551X_SPI_CMD_32_WRITE,
	RT551X_SPI_CMD_BURST_READ,
	RT551X_SPI_CMD_BURST_WRITE,
};

int rt551x_spi_read_addr(unsigned int addr, unsigned int *val);
int rt551x_spi_write_addr(unsigned int addr, unsigned int val);
int rt551x_spi_burst_read(unsigned int addr, u8 *rxbuf, size_t len);
int rt551x_spi_burst_write(u32 addr, const u8 *txbuf, size_t len);
void reset_pcm_read_pointer(void);
void set_pcm_is_readable(int readable);
void rt551x_SetRP_onIdle(void);
void wait_for_rt551x_spi_resumed(void);
#endif /* __RT551X_SPI_H__ */
