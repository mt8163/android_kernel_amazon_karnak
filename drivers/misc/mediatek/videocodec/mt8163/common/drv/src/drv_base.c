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

#include "drv_api.h"
#include "val_types_private.h"
#include <linux/dma-mapping.h>
#include <linux/sched.h>

/* ============================================================== */
/* For Hybrid HW */
struct VAL_VCODEC_OAL_HW_CONTEXT_T oal_hw_ctx[VCODEC_MULTIPLE_INSTANCE_NUM];
/* mutex : NonCacheMemoryListLock */
struct VAL_NON_CACHE_MEMORY_LIST_T
	grNCMemList[VCODEC_MULTIPLE_INSTANCE_NUM_x_10];

/* For both hybrid and pure HW */
struct VAL_VCODEC_HW_LOCK_T grVDecHWLock;	/* mutex : VdecHWLock */
struct VAL_VCODEC_HW_LOCK_T grVEncHWLock;	/* mutex : VencHWLock */

unsigned int gu4LockDecHWCount;	/* spinlock : LockDecHWCountLock */
unsigned int gu4LockEncHWCount;	/* spinlock : LockEncHWCountLock */
unsigned int gu4DecISRCount;	/* spinlock : DecISRCountLock */
unsigned int gu4EncISRCount;	/* spinlock : EncISRCountLock */


signed int search_HWLockSlot_ByTID(unsigned long ulpa, unsigned int curr_tid)
{
	int i;
	int j;

	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_ctx[i].u4VCodecThreadNum !=
		VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_ctx[i].u4VCodecThreadNum;
			j++) {
				if (oal_hw_ctx[i].u4VCodecThreadID[j]
					== curr_tid) {
					pr_debug("[%s] Lookup curr HW Locker is ObjId %d in index%d\n",
					    __func__, curr_tid, i);
					return i;
				}
			}
		}
	}

	return -1;
}

signed int search_HWLockSlot_ByHandle(unsigned long ulpa, unsigned long handle)
{
	signed int i;

	if (handle == (unsigned long)NULL) {
		pr_err("[VCODEC] Get NULL Handle\n");
		return -1;
	}
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_ctx[i].pvHandle == handle)
			return i;
	}

	return -1;
}

struct VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot
	(unsigned long ulpa, unsigned int tid)
{

	int i, j;

	/* Dump current ObjId */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++)
		pr_debug("Dump curr slot %d ObjId %lu\n",
			i, oal_hw_ctx[i].ObjId);

	/* check if current ObjId exist in oal_hw_ctx[i].ObjId */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_ctx[i].ObjId == ulpa) {
			pr_debug("[VCODEC] Curr Already exist in %d Slot\n", i);
			return &oal_hw_ctx[i];
		}
	}

	/* if not exist in table,  find a new free slot and put it */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_ctx[i].u4VCodecThreadNum !=
			VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_ctx[i].u4VCodecThreadNum; j++) {
				if (oal_hw_ctx[i].u4VCodecThreadID[j]
					== current->pid) {
					oal_hw_ctx[i].ObjId = ulpa;
					pr_debug("[VCODEC][%s] setCurr %d Slot\n",
						 __func__, i);
					return &oal_hw_ctx[i];
				}
			}
		}
	}

	pr_err("[VCODEC][ERROR] %s All %d Slots unavaliable\n",
		 __func__, VCODEC_MULTIPLE_INSTANCE_NUM);
	oal_hw_ctx[0].u4VCodecThreadNum = VCODEC_THREAD_MAX_NUM - 1;
	for (i = 0; i < oal_hw_ctx[0].u4VCodecThreadNum; i++)
		oal_hw_ctx[0].u4VCodecThreadID[i] = current->pid;

	return &oal_hw_ctx[0];
}


struct VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot_Thread_ID
	(struct VAL_VCODEC_THREAD_ID_T a_prVThrdID,
	unsigned int *a_prIndex)
{
	int i;
	int j;
	int k;

	/* Dump current tids */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_ctx[i].u4VCodecThreadNum !=
			VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_ctx[i].u4VCodecThreadNum; j++) {
				pr_debug("[%s] Dump curr slot %d, ThreadID[%d] = %d\n",
				     __func__, i, j,
				     oal_hw_ctx[i].u4VCodecThreadID[j]);
			}
		}
	}

	for (i = 0; i < a_prVThrdID.u4VCodecThreadNum; i++) {
		pr_debug("[%s] VCodecThreadNum = %d, VCodecThreadID = %d\n",
			__func__, a_prVThrdID.u4VCodecThreadNum,
			a_prVThrdID.u4VCodecThreadID[i]);
	}

	/* check if current tids exist in oal_hw_ctx[i].ObjId */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_ctx[i].u4VCodecThreadNum !=
			VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_ctx[i].u4VCodecThreadNum; j++) {
				for (k = 0; k < a_prVThrdID.u4VCodecThreadNum;
				k++) {
					if (oal_hw_ctx[i].u4VCodecThreadID[j] ==
					    a_prVThrdID.u4VCodecThreadID[k]) {
						pr_err("[%s] Curr Already exist in %d Slot\n",
							__func__, i);
						*a_prIndex = i;
						return &oal_hw_ctx[i];
					}
				}
			}
		}
	}

	/* if not exist in table,  find a new free slot and put it */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_ctx[i].u4VCodecThreadNum
			== VCODEC_THREAD_MAX_NUM) {
			oal_hw_ctx[i].u4VCodecThreadNum =
				a_prVThrdID.u4VCodecThreadNum;
			for (j = 0; j < a_prVThrdID.u4VCodecThreadNum; j++) {
				oal_hw_ctx[i].u4VCodecThreadID[j] =
				    a_prVThrdID.u4VCodecThreadID[j];
				pr_debug("[%s] setCurr %d Slot, %d\n", __func__,
					i, oal_hw_ctx[i].u4VCodecThreadID[j]);
			}
			*a_prIndex = i;
			return &oal_hw_ctx[i];
		}
	}

	{
		pr_err("[VCodec Error][ERROR] %s All %d Slots unavaliable\n",
			__func__, VCODEC_MULTIPLE_INSTANCE_NUM);
		oal_hw_ctx[0].u4VCodecThreadNum =
			a_prVThrdID.u4VCodecThreadNum;
		for (i = 0; i < oal_hw_ctx[0].u4VCodecThreadNum; i++) {
			oal_hw_ctx[0].u4VCodecThreadID[i] =
			    a_prVThrdID.u4VCodecThreadID[i];
		}
		*a_prIndex = 0;
		return &oal_hw_ctx[0];
	}
}


struct VAL_VCODEC_OAL_HW_CONTEXT_T *freeCurr_HWLockSlot(unsigned long ulpa)
{
	int i;
	int j;

	/* check if current ObjId exist in oal_hw_ctx[i].ObjId */

	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_ctx[i].ObjId == ulpa) {
			oal_hw_ctx[i].ObjId = -1;
			for (j = 0; j < oal_hw_ctx[i].u4VCodecThreadNum; j++)
				oal_hw_ctx[i].u4VCodecThreadID[j] = -1;

			oal_hw_ctx[i].u4VCodecThreadNum =
				VCODEC_THREAD_MAX_NUM;
			oal_hw_ctx[i].Oal_HW_reg =
				(struct VAL_VCODEC_OAL_HW_REGISTER_T *) 0;
			pr_debug("%s %d Slot\n", __func__, i);
			return &oal_hw_ctx[i];
		}
	}

	pr_err("[VCodec Error][ERROR] %s can't find pid in HWLockSlot\n",
	__func__);
	return 0;
}


void Add_NonCacheMemoryList(unsigned long a_ulKVA,
	unsigned long a_ulKPA, unsigned long a_ulSize,
	unsigned int a_u4VCodecThreadNum,
	unsigned int *a_pu4VCodecThreadID)
{
	unsigned int u4I = 0;
	unsigned int u4J = 0;

	pr_debug("%s +, KVA = 0x%lx, KPA = 0x%lx, Size = 0x%lx\n",
		 __func__, a_ulKVA, a_ulKPA, a_ulSize);

	for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
		if ((grNCMemList[u4I].ulKVA == 0xffffffff)
		    && (grNCMemList[u4I].ulKPA == 0xffffffff)) {
			pr_debug("ADD %s index = %d, VCodecThreadNum = %d, curr_tid = %d\n",
				__func__, u4I, a_u4VCodecThreadNum,
				current->pid);

			grNCMemList[u4I].u4VThrdNum = a_u4VCodecThreadNum;
			for (u4J = 0; u4J < grNCMemList[u4I].u4VThrdNum;
			u4J++) {
				grNCMemList[u4I].u4VThrdID[u4J] =
					*(a_pu4VCodecThreadID + u4J);
				pr_debug("[%s] VCodecThreadNum = %d, VCodecThreadID = %d\n",
					__func__, grNCMemList[u4I].u4VThrdNum,
					grNCMemList[u4I].u4VThrdID[u4J]);
			}

			grNCMemList[u4I].ulKVA = a_ulKVA;
			grNCMemList[u4I].ulKPA = a_ulKPA;
			grNCMemList[u4I].ulSize = a_ulSize;
			break;
		}
	}

	if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10)
		pr_err("[VCODEC][ERROR] CAN'T ADD %s, List is FULL!\n",
			__func__);

	pr_debug("[VCODEC] %s -\n", __func__);
}

void Free_NonCacheMemoryList(unsigned long a_ulKVA, unsigned long a_ulKPA)
{
	unsigned int u4I = 0;
	unsigned int u4J = 0;

	pr_debug("[VCODEC] %s +, KVA = 0x%lx, KPA = 0x%lx\n",
		__func__, a_ulKVA, a_ulKPA);

	for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
		if ((grNCMemList[u4I].ulKVA == a_ulKVA)
		    && (grNCMemList[u4I].ulKPA == a_ulKPA)) {
			pr_debug("[VCODEC] Free %s index = %d\n",
				__func__, u4I);
			grNCMemList[u4I].u4VThrdNum =
				VCODEC_THREAD_MAX_NUM;
			for (u4J = 0; u4J < VCODEC_THREAD_MAX_NUM; u4J++)
				grNCMemList[u4I].u4VThrdID[u4J]
				= 0xffffffff;

			grNCMemList[u4I].ulKVA = -1L;
			grNCMemList[u4I].ulKPA = -1L;
			grNCMemList[u4I].ulSize = -1L;
			break;
		}
	}

	if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10)
		pr_err("[VCODEC][ERROR] CAN'T Free %s, Address is not find!!\n",
			__func__);


	pr_debug("[VCODEC]%s -\n", __func__);
}


void Force_Free_NonCacheMemoryList(unsigned int a_u4Tid)
{
	unsigned int u4I = 0;
	unsigned int u4J = 0;
	unsigned int u4K = 0;

	pr_debug("[VCODEC] %s +, curr_id = %d",
		__func__, a_u4Tid);

	for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
		if (grNCMemList[u4I].u4VThrdNum
			!= VCODEC_THREAD_MAX_NUM) {
			for (u4J = 0;
			u4J < grNCMemList[u4I].u4VThrdNum;
			u4J++) {
				if (grNCMemList[u4I].u4VThrdID[u4J]
				== a_u4Tid) {
					pr_debug("[VCODEC][WARNING] %s index = %d, tid = %d\n",
						__func__, u4I, a_u4Tid);
					pr_debug("KVA = 0x%lx, KPA = 0x%lx, Size = %lu\n",
					     grNCMemList[u4I].ulKVA,
					     grNCMemList[u4I].ulKPA,
					     grNCMemList[u4I].ulSize);

					dma_free_coherent
						(0, grNCMemList[u4I].ulSize,
						(void *)grNCMemList[u4I].ulKVA,
						(dma_addr_t) grNCMemList[u4I].
						ulKPA);

					grNCMemList[u4I].u4VThrdNum =
					    VCODEC_THREAD_MAX_NUM;
					for (u4K = 0;
					u4K < VCODEC_THREAD_MAX_NUM;
					u4K++) {
						grNCMemList[u4I].u4VThrdID[u4K]
							= 0xffffffff;
					}
					grNCMemList[u4I].ulKVA = -1L;
					grNCMemList[u4I].ulKPA = -1L;
					grNCMemList[u4I].ulSize = -1L;
					break;
				}
			}
		}
	}

	pr_debug("%s -, curr_id = %d", __func__, a_u4Tid);
}

unsigned int Search_NonCacheMemoryList_By_KPA(unsigned long a_ulKPA)
{
	unsigned int u4I = 0;
	unsigned long ulVA_Offset = 0;

	ulVA_Offset = a_ulKPA & 0x00000fff;

	pr_debug("%s +, KPA = 0x%lx, ulVA_Offset = 0x%lx\n",
		__func__, a_ulKPA, ulVA_Offset);

	for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
		if (grNCMemList[u4I].ulKPA == (a_ulKPA - ulVA_Offset)) {
			pr_debug("Find %s index = %d\n",
				__func__, u4I);
			break;
		}
	}

	if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10) {
		pr_err("[ERROR] CAN'T Find %s, Address is not find!!\n",
			__func__);
		return (grNCMemList[0].ulKVA + ulVA_Offset);
	}

	pr_debug("%s, ulVA = 0x%lx -\n", __func__,
		 (grNCMemList[u4I].ulKVA + ulVA_Offset));

	return (grNCMemList[u4I].ulKVA + ulVA_Offset);
}
