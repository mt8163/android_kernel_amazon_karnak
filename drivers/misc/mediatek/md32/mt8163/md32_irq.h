/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MD32_IRQ_H__
#define __MD32_IRQ_H__
#include <linux/interrupt.h>

#define MD32_MAX_USER	20
/* #define MD2HOST_IPCR  0x1005001C */

/*Define MD32 IRQ Type*/
#define MD32_IPC_INT	0x100
#define WDT_INT		0x200
#define PMEM_DISP_INT	0x400
#define DMEM_DISP_INT	0x800

#define MD32_PTCM_SIZE		0x18000 /* 96KB */
#define MD32_DTCM_SIZE		0x20000 /* 128KB */
#define MD32_CFGREG_SIZE	0x100

#define MD32_BASE		(md32reg.cfg)
#define MD32_TO_HOST_ADDR	(md32reg.cfg + 0x001C)
#define MD32_TO_HOST_REG	MD32_TO_HOST_ADDR
#define HOST_TO_MD32_REG	(md32reg.cfg + 0x0024)
#define MD32_DEBUG_PC_REG	(md32reg.cfg + 0x0060)
#define MD32_DEBUG_R14_REG	(md32reg.cfg + 0x0064)
#define MD32_DEBUG_R15_REG	(md32reg.cfg + 0x0068)
#define MD32_WDT_REG		(md32reg.cfg + 0x0084)
#define MD32_PTCM		(md32reg.sram)
#define MD32_DTCM		(md32reg.sram + MD32_PTCM_SIZE)

#define IPC_MD2HOST		(1 << 0)

struct md32_regs {
	void __iomem *sram;
	void __iomem *cfg;
	void __iomem *clkctrl;
	int irq;
};

struct md32_wdt_func {
	void (*wdt_func[MD32_MAX_USER]) (void *);
	void (*reset_func[MD32_MAX_USER]) (void *);
	char MODULE_NAME[MD32_MAX_USER][100];
	void *private_data[MD32_MAX_USER];
	int in_use[MD32_MAX_USER];
};

struct md32_assert_func {
	void (*assert_func[MD32_MAX_USER]) (void *);
	void (*reset_func[MD32_MAX_USER]) (void *);
	char MODULE_NAME[MD32_MAX_USER][100];
	void *private_data[MD32_MAX_USER];
	int in_use[MD32_MAX_USER];
};

void irq_ast_ipi_handler(int id, void *data, unsigned int len);
void memcpy_to_md32(void __iomem *trg, const void *src, int size);
void memcpy_from_md32(void *trg, const void __iomem *src, int size);
void md32_ipi_handler(void);
void md32_ipi_init(void);
void md32_irq_init(void);
irqreturn_t md32_irq_handler(int irq, void *dev_id);
void md32_ocd_init(void);
int reboot_load_md32(void);

extern struct device_attribute dev_attr_md32_ocd;
extern unsigned char *md32_send_buff;
extern unsigned char *md32_recv_buff;
extern unsigned int md32_log_buf_addr;
extern unsigned int md32_log_start_idx_addr;
extern unsigned int md32_log_end_idx_addr;
extern unsigned int md32_log_lock_addr;
extern unsigned int md32_log_buf_len_addr;
extern struct md32_regs md32reg;
#endif /* __MD32_IRQ_H__ */
