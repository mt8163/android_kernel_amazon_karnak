/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __M4U_H__
#define __M4U_H__
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/scatterlist.h>
#include "m4u_port.h"

#define M4U_PROT_READ   (1<<0)  /* buffer can be read by engine */
#define M4U_PROT_WRITE  (1<<1)  /* buffer can be write by engine */
/* buffer access will goto CCI to do cache snoop */
#define M4U_PROT_CACHE  (1<<2)
/*
 * buffer access will goto CCI, but don't do cache snoop
 * just for engines who wants to use CCI bandwidth
 */
#define M4U_PROT_SHARE  (1<<3)
/* buffer can only be accessed by secure engine. */
#define M4U_PROT_SEC    (1<<4)

/* public flags */
/* engine access this buffer in sequncial way. */
#define M4U_FLAGS_SEQ_ACCESS (1<<0)
/* fix allocation, we will use mva user specified. */
#define M4U_FLAGS_FIX_MVA   (1<<1)
#define M4U_FLAGS_SEC_SHAREABLE   (1<<2)  /* the mva will share in SWd */
/*
 * the allocator will search free mva from user specified.
 */
#define M4U_FLAGS_START_FROM   (1<<3)
/*
 * ion_alloc have allocated sg_table with
 * m4u_create_sgtable. For va2mva ion case
 */
#define M4U_FLAGS_SG_READY   (1<<4)

/* m4u internal flags (DO NOT use them for other purpers) */
#define M4U_FLAGS_MVA_IN_FREE (1<<8) /* this mva is in deallocating. */


enum M4U_RANGE_PRIORITY_ENUM {
	RT_RANGE_HIGH_PRIORITY = 0,
	SEQ_RANGE_LOW_PRIORITY = 1
};


struct port_mva_info_t {
	int eModuleID;
	unsigned long va;
	unsigned int BufSize;
	int security;
	int cache_coherent;
	unsigned int flags;
	unsigned int iova_start;
	unsigned int iova_end;
	unsigned int mva;
};

/* port related: virtuality, security, distance */
struct M4U_PORT_STRUCT {
	int ePortID;		   /* hardware port ID, defined in int */
	unsigned int Virtuality;
	unsigned int Security;
	unsigned int domain;            /* domain : 0 1 2 3 */
	unsigned int Distance;
	unsigned int Direction;         /* 0:- 1:+ */
};

struct m4u_port_array {
#define M4U_PORT_ATTR_EN		(1<<0)
#define M4U_PORT_ATTR_VIRTUAL	(1<<1)
#define M4U_PORT_ATTR_SEC		(1<<2)
	unsigned char ports[M4U_PORT_NR];
};


enum M4U_CACHE_SYNC_ENUM {
	M4U_CACHE_CLEAN_BY_RANGE,
	M4U_CACHE_INVALID_BY_RANGE,
	M4U_CACHE_FLUSH_BY_RANGE,

	M4U_CACHE_CLEAN_ALL,
	M4U_CACHE_INVALID_ALL,
	M4U_CACHE_FLUSH_ALL,
};

struct m4u_client_t {
	/* mutex to protect mvaList */
	/* should get this mutex whenever add/delete/interate mvaList */
	struct mutex dataMutex;
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head mvaList;
};




int m4u_dump_info(int m4u_index);
int m4u_power_on(int m4u_index);
int m4u_power_off(int m4u_index);

int m4u_alloc_mva(struct m4u_client_t *client, int port, unsigned long va,
		struct sg_table *sg_table, unsigned int size, unsigned int prot,
		unsigned int flags, unsigned int *pMva);

int m4u_dealloc_mva(struct m4u_client_t *client, int port, unsigned int mva);

int m4u_alloc_mva_sg(struct port_mva_info_t *port_info,
		struct sg_table *sg_table);
int m4u_dealloc_mva_sg(int eModuleID, struct sg_table *sg_table,
		const unsigned int BufSize, const unsigned int MVA);
int m4u_dealloc_mva_fix(struct m4u_client_t *client,
		int port, unsigned int mva);

struct sg_table *m4u_create_sgtable(unsigned long va,
	unsigned int size);

int m4u_config_port(struct M4U_PORT_STRUCT *pM4uPort);
int m4u_config_port_array(struct m4u_port_array *port_array);
int m4u_monitor_start(int m4u_id);
int m4u_monitor_stop(int m4u_id);


int m4u_cache_sync(struct m4u_client_t *client, int port,
			unsigned long va, unsigned int size, unsigned int mva,
			enum M4U_CACHE_SYNC_ENUM sync_type);

int m4u_mva_map_kernel(unsigned int mva, unsigned int size,
	unsigned long *map_va, unsigned int *map_size);
int m4u_mva_unmap_kernel(unsigned int mva, unsigned int size, unsigned long va);
struct m4u_client_t *m4u_create_client(void);
int m4u_destroy_client(struct m4u_client_t *client);


enum m4u_callback_ret_t {
	M4U_CALLBACK_HANDLED,
	M4U_CALLBACK_NOT_HANDLED,
};

typedef enum m4u_callback_ret_t (m4u_reclaim_mva_callback_t)(int alloc_port,
		unsigned int mva, unsigned int size, void *data);
int m4u_register_reclaim_callback(int port,
		m4u_reclaim_mva_callback_t *fn, void *data);
int m4u_unregister_reclaim_callback(int port);

typedef enum m4u_callback_ret_t (m4u_fault_callback_t)(int port,
		unsigned int mva, void *data);
int m4u_register_fault_callback(int port, m4u_fault_callback_t *fn, void *data);
int m4u_unregister_fault_callback(int port);

/* ===add for GPU==== */
int m4u_reg_backup(int m4u_id);
int m4u_reg_restore(int m4u_id);

typedef void (m4u_regwrite_callback_t)(unsigned int reg, unsigned int value);
void m4u_register_regwirte_callback(struct m4u_client_t *client,
		m4u_regwrite_callback_t *fn);

/* struct m4u_gpu_cb
 * Param:
 * fgForcePoweron : false: only get the state
 *		    true : request gpu power on if it's power off.
 *			   only return the state if it's power on.
 * Return : 0: unlocked and power off.
 *		if it is power off, m4u_invalid_tlb will return directly,
 *		it don't call unlock. so please keep it unlocked.
 *          1: locked and power on.
 *		if it is power on, "unlocked" is needed, please keep it locked.
 *	    <0: unlocked and failed while force power on.
 * Return is 0 or 1 while fgForcePoweron is false.
 */
struct m4u_gpu_cb {
	int  (*gpu_power_state_freeze)(bool fgForcePoweron);

	void (*gpu_power_state_unfreeze)(bool fgForcePoweron);
};

void m4u_register_gpu_clock_callback(struct m4u_client_t *client,
		struct m4u_gpu_cb *fn);
/*Return 0: no translation fault
 *       Others: have translation fault.
 */
unsigned int m4u_get_gpu_translationfault_state(void);

void m4u_get_pgd(struct m4u_client_t *client, int port,
	void **pgd_va, void **pgd_pa, unsigned int *size);
unsigned long m4u_mva_to_pa(struct m4u_client_t *client,
	int port, unsigned int mva);
/* =================== */
void m4u_larb0_enable(char *name);
void m4u_larb0_disable(char *name);

/* for smi driver use */
int m4u_larb_backup_sec(int larb);
int m4u_larb_restore_sec(int larb);

/* m4u driver internal use ------------------------------------------------- */
/*  */

#endif
