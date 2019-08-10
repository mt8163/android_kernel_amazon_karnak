#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#include "mach/emi_mpu.h"
#include <mt-plat/dma.h>
#include <mt-plat/mt_io.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mt_device_apc.h>


#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include "trustzone/kree/system.h"
#include "trustzone/tz_cross/trustzone.h"
#include "trustzone/tz_cross/ta_emi.h"
#endif


#define ENABLE_EMI_CHKER
#define ENABLE_EMI_WATCH_POINT
#define NR_REGION_ABORT 15
#define MAX_EMI_MPU_STORE_CMD_LEN 128
#define TIMEOUT 100
#define AXI_VIO_MONITOR_TIME (1 * HZ)

static struct work_struct emi_mpu_work;
static struct workqueue_struct *emi_mpu_workqueue;

static unsigned int vio_addr;
static unsigned int emi_physical_offset;

struct mst_tbl_entry {
	u32 master;
	u32 port;
	u32 id_mask;
	u32 id_val;
	char *name;
};

struct emi_mpu_notifier_block {
	struct list_head list;
	emi_mpu_notifier notifier;
};

static const struct mst_tbl_entry mst_tbl[] = {
    /* apmcu */
	{	.master = MST_ID_APMCU_0, .port = 0x0, .id_mask = 0x0007,
		.id_val = 0x0004, .name = "CA53: Cluster0" },
	{	.master = MST_ID_APMCU_1, .port = 0x0, .id_mask = 0x0007,
		.id_val = 0x0003, .name = "CA53: Cluster1" },
	{	.master = MST_ID_APMCU_2, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0022, .name = "PWM" },
	{	.master = MST_ID_APMCU_3, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0122, .name = "MSDC1" },
	{	.master = MST_ID_APMCU_4, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0222, .name = "MSDC2" },
	{	.master = MST_ID_APMCU_5, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0322, .name = "SPI0" },
	{	.master = MST_ID_APMCU_6, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0002, .name = "IC USB" },
	{	.master = MST_ID_APMCU_7, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0102, .name = "USB0" },
	{	.master = MST_ID_APMCU_8, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0202, .name = "MSDC3" },
	{	.master = MST_ID_APMCU_9, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0302, .name = "Audio" },
	{	.master = MST_ID_APMCU_10, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0042, .name = "DBG_I2C" },
	{	.master = MST_ID_APMCU_11, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0142, .name = "SPM" },
	{	.master = MST_ID_APMCU_12, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0242, .name = "MD32" },
	{	.master = MST_ID_APMCU_13, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0342, .name = "THERM" },
	{	.master = MST_ID_APMCU_14, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0162, .name = "MSCD0" },
	{	.master = MST_ID_APMCU_15, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x0082, .name = "APDMA" },
	{	.master = MST_ID_APMCU_16, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x000a, .name = "GCPU" },
	{	.master = MST_ID_APMCU_18, .port = 0x0, .id_mask = 0x1f9f,
		.id_val = 0x0012, .name = "CQ_DMA" },
	{	.master = MST_ID_APMCU_19, .port = 0x0, .id_mask = 0x1fbf,
		.id_val = 0x003a, .name = "DebugTop" },
	{	.master = MST_ID_APMCU_20, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x03e2, .name = "Perisys IOMMU" },
	{	.master = MST_ID_APMCU_21, .port = 0x0, .id_mask = 0x1fff,
		.id_val = 0x3ea, .name = "Perisys IOMMU" },
	{	.master = MST_ID_APMCU_22, .port = 0x0, .id_mask = 0x1f07,
		.id_val = 0x0001, .name = "GPU" },

    /* Periperal */
	{
		.master = MST_ID_PERI_0, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0004, .name = "PWM" },
	{	.master = MST_ID_PERI_1, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0024, .name = "MSDC1" },
	{	.master = MST_ID_PERI_2, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0044, .name = "MSDC2" },
	{	.master = MST_ID_PERI_3, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0064, .name = "SPI0" },
	{	.master = MST_ID_PERI_4, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0000, .name = "IC USB" },
	{	.master = MST_ID_PERI_5, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0020, .name = "USB0" },
	{	.master = MST_ID_PERI_6, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0040, .name = "MSDC3" },
	{	.master = MST_ID_PERI_7, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0060, .name = "Audio" },
	{	.master = MST_ID_PERI_8, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0008, .name = "DBG_I2C" },
	{	.master = MST_ID_PERI_9, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0028, .name = "SPM" },
	{	.master = MST_ID_PERI_10, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0048, .name = "MD32" },
	{	.master = MST_ID_PERI_11, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0068, .name = "THERM" },
	{	.master = MST_ID_PERI_12, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x002c, .name = "MSCD0" },
	{	.master = MST_ID_PERI_13, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0010, .name = "APDMA" },
	{	.master = MST_ID_PERI_14, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x0001, .name = "GCPU" },
	{	.master = MST_ID_PERI_15, .port = 0x1, .id_mask = 0x1ff3,
		.id_val = 0x0002, .name = "CQ_DMA" },
	{	.master = MST_ID_PERI_16, .port = 0x1, .id_mask = 0x1ff7,
		.id_val = 0x0007, .name = "DebugTop" },
	{	.master = MST_ID_PERI_17, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x007c, .name = "Perisys IOMMU" },
	{	.master = MST_ID_PERI_18, .port = 0x1, .id_mask = 0x1fff,
		.id_val = 0x007d, .name = "Perisys IOMMU" },

    /* Conn  */
	{	.master = MST_ID_CONN_0, .port = 0x2, .id_mask = 0x1fff,
		.id_val = 0x0000, .name = "Conn" },

    /* Modem */
	{	.master = MST_ID_MDMCU_0, .port = 0x3, .id_mask = 0x0000,
		.id_val = 0x0000, .name = "MDMCU" },

    /* Modem HW (2G/3G) */
	{	.master = MST_ID_MDHW_0, .port = 0x4, .id_mask = 0x0000,
		.id_val = 0x0000, .name = "MDHW" },

    /* MM */
	{	.master = MST_ID_MM_0, .port = 0x5, .id_mask = 0x1f80,
		.id_val = 0x0000, .name = "Larb0" },
	{	.master = MST_ID_MM_1, .port = 0x5, .id_mask = 0x1f80,
		.id_val = 0x0080, .name = "Larb1" },
	{	.master = MST_ID_MM_2, .port = 0x5, .id_mask = 0x1f80,
		.id_val = 0x0100, .name = "Larb2" },
	{	.master = MST_ID_MM_3, .port = 0x5, .id_mask = 0x1f80,
		.id_val = 0x0180, .name = "Larb3" },
	{	.master = MST_ID_MM_4, .port = 0x5, .id_mask = 0x1f80,
		.id_val = 0x0200, .name = "Larb4" },
	{	.master = MST_ID_MM_5, .port = 0x5, .id_mask = 0x1ffe,
		.id_val = 0x03fc, .name = "M4U" },

    /* MFG  */
	{	.master = MST_ID_GPU_0, .port = 0x6, .id_mask = 0x1f01,
		.id_val = 0x0000, .name = "MFG" },

    /* Modem */
	{	.master = MST_ID_MD_LITE, .port = 0x7, .id_mask = 0x0000,
		.id_val = 0x0000, .name = "MDMCU1" },

};

/* struct list_head emi_mpu_notifier_list[NR_MST]; */
static const char *UNKNOWN_MASTER = "unknown";
static spinlock_t emi_mpu_lock;

#ifdef ENABLE_EMI_CHKER
struct timer_list emi_axi_vio_timer;
#endif

char *smi_larb0_port[17] = {"disp_ovl_0", "disp_rdma_0", "disp_wdma_0", "disp_ovl_1",
							"disp_rdma_1", "disp_wdma_1", "ufod_rdma0", "ufod_rdma1",
							"ufod_rdma2", "ufod_rdma3", "ufod_rdma4", "ufod_rdma5",
							"ufod_rdma6", "ufod_rdma7", "mdp_rdma", "mdp_wdma", "mdp_wrot"};

char *smi_larb1_port[9] =  {"hw_vdec_mc_ext", "hw_vdec_pp_ext", "hw_vdec_vld_ext",
							"hw_vdec_avc_mv_ext" , "hw_vdec_pred_rd_ext",
							"hw_vdec_pred_wr_ext", "hw_vdec_ppwarp_ext" };

char *smi_larb2_port[21] = {"cam_imgo", "cam_img2o", "cam_isci", "cam_imgi",
							"cam_esfko", "cam_aao"};

char *smi_larb3_port[19] = {"venc_rcpu", "venc_rec", "venc_bsdma", "venc_sv_comv",
							"vend_rd_comv", "jpgenc_rdma", "jpgenc_bsdma", "jpgdec_wdma",
							"jpgdec_bsdma", "venc_cur_luma", "venc_cur_chroma",
							"venc_ref_luma", "vend_ref_chroma"};

char *smi_larb4_port[4] = {"mjc_mv_rd", "mjc_mv_wr", "mjc_dma_rd", "mjc_dma_wr"};

static int __match_id(u32 axi_id, int tbl_idx, u32 port_ID)
{
	u32 mm_larb;
	u32 smi_port;

	if (((axi_id & mst_tbl[tbl_idx].id_mask) == mst_tbl[tbl_idx].id_val) && (port_ID == mst_tbl[tbl_idx].port)) {
		switch (port_ID) {
		case 0: /* ARM */
		case 1: /* Peripheral */
		case 2: /* Conn */
		case 3: /* MD */
		case 4: /* MD HW (2G/3G) */
		case 6: /* MFG */
		case 7: /* MD */
			pr_err("Violation master name is %s.\n", mst_tbl[tbl_idx].name);
			break;
		case 5: /* MM */
			mm_larb = axi_id>>7;
			smi_port = (axi_id & 0x7F) >> 2;
			if (mm_larb == 0x0) {
				if (smi_port >= ARRAY_SIZE(smi_larb0_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_err("Violation master name is %s (%s).\n", mst_tbl[tbl_idx].name,
						smi_larb0_port[smi_port]);
			} else if (mm_larb == 0x1) {
				if (smi_port >= ARRAY_SIZE(smi_larb1_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
					pr_err("Violation master name is %s (%s).\n", mst_tbl[tbl_idx].name,
							smi_larb1_port[smi_port]);
			} else if (mm_larb == 0x2) {
				if (smi_port >= ARRAY_SIZE(smi_larb2_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_err("Violation master name is %s (%s).\n", mst_tbl[tbl_idx].name,
						smi_larb2_port[smi_port]);
			} else if (mm_larb == 0x3) {
				if (smi_port >= ARRAY_SIZE(smi_larb3_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
					pr_err("Violation master name is %s (%s).\n", mst_tbl[tbl_idx].name,
							smi_larb3_port[smi_port]);
			} else if (mm_larb == 0x4) {
				if (smi_port >= ARRAY_SIZE(smi_larb4_port)) {
					pr_err("[EMI MPU ERROR] Invalidate master ID! lookup smi table failed!\n");
					return 0;
				}
				pr_err("Violation master name is %s (%s).\n", mst_tbl[tbl_idx].name,
						smi_larb4_port[smi_port]);
			} else /*M4U*/ {
				pr_err("Violation master name is %s.\n", mst_tbl[tbl_idx].name);
			}
			break;
		default:
				pr_err("[EMI MPU ERROR] Invalidate port ID! lookup bus ID table failed!\n");
				break;
		}
		return 1;
	}
	return 0;
}

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#ifdef CONFIG_MTK_AEE_FEATURE
static u32 __id2mst(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x000001FFF;
	port_ID = id & 0x00000007;

	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID))
			return mst_tbl[i].master;
	}
	return MST_INVALID;
}
#endif
#endif

static char *__id2name(u32 id)
{
	int i;
	u32 axi_ID;
	u32 port_ID;

	axi_ID = (id >> 3) & 0x00001FFF;
	port_ID = id & 0x00000007;

    /*MDHW disable
	if (port_ID == 4)
		disable_irq_nosync(APARM_DOMAIN_IRQ_BIT_ID);
	*/
	for (i = 0; i < ARRAY_SIZE(mst_tbl); i++) {
		if (__match_id(axi_ID, i, port_ID))
			return mst_tbl[i].name;
	}

	return (char *)UNKNOWN_MASTER;
}

static unsigned int emi_reg_read(void __iomem *addr)
{
	unsigned int reg_val;

#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	reg_val = readl(IOMEM(addr));
#else
	KREE_SESSION_HANDLE emi_session;
	MTEEC_PARAM param[4];
	TZ_RESULT ret;

	ret = KREE_CreateSession(TZ_TA_EMI_UUID, &emi_session);
	if (ret != TZ_RESULT_SUCCESS)
		return -1;

	param[0].value.a = (uint32_t)((unsigned long)(addr) & 0xFFF);
	ret = KREE_TeeServiceCall(emi_session, TZCMD_EMI_RD,
			TZ_ParamTypes4(TZPT_VALUE_INOUT, TZPT_VALUE_INOUT,
			TZPT_VALUE_INOUT, TZPT_VALUE_INOUT),
			param);

	reg_val = param[0].value.b;
	ret = KREE_CloseSession(emi_session);
	if (ret != TZ_RESULT_SUCCESS)
		return -1;

#endif
	return reg_val;
}

static void emi_reg_write(unsigned int val, void __iomem *addr)
{
#ifndef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	mt_reg_sync_writel(val, addr);
#else
	KREE_SESSION_HANDLE emi_session;
	MTEEC_PARAM param[4];
	TZ_RESULT ret;

	ret = KREE_CreateSession(TZ_TA_EMI_UUID, &emi_session);
	if (ret != TZ_RESULT_SUCCESS)
		return;

	param[0].value.a = (uint32_t)((unsigned long)(addr) & 0xFFF);
	param[0].value.b = (uint32_t)val;
	ret = KREE_TeeServiceCall(emi_session, TZCMD_EMI_WR,
		TZ_ParamTypes4(TZPT_VALUE_INOUT, TZPT_VALUE_INOUT, TZPT_VALUE_INOUT, TZPT_VALUE_INOUT),
		param);

	ret = KREE_CloseSession(emi_session);
	if (ret != TZ_RESULT_SUCCESS)
		return;
#endif
}

/*
static void __clear_emi_mpu_vio(void)
{


}
*/

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT

/*EMI MPU violation handler*/
static irqreturn_t mpu_violation_irq(int irq, void *dev_id)
{
	u32 dbg_s, dbg_t, dbg_pqry;
	u32 master_ID, domain_ID, wr_vio;
	s32 region;
	char *master_name;

	KREE_SESSION_HANDLE emi_session;
	MTEEC_PARAM param[4];
	TZ_RESULT ret;


	pr_err("[EMI MPU] Violation information from TA.\n");

	ret = KREE_CreateSession(TZ_TA_EMI_UUID, &emi_session);
	if (ret != TZ_RESULT_SUCCESS)
		return -1;

	ret = KREE_TeeServiceCall(emi_session, TZCMD_EMI_REG,
			TZ_ParamTypes4(TZPT_VALUE_OUTPUT, TZPT_VALUE_OUTPUT,
			TZPT_VALUE_OUTPUT, TZPT_VALUE_OUTPUT), param);

	dbg_s     = (uint32_t)(param[0].value.a);
	dbg_t     = (uint32_t)(param[0].value.b);
	master_ID = (uint32_t)(param[1].value.a);
	domain_ID = (uint32_t)(param[1].value.b);
	region    = (uint32_t)(param[2].value.a);
	dbg_pqry  = (uint32_t)(param[2].value.b);
	wr_vio = (dbg_s >> 28) & 0x00000003;

	ret = KREE_CloseSession(emi_session);
	if (ret != TZ_RESULT_SUCCESS)
		return -1;

	if (dbg_s == 0) {
		pr_err("It's not a MPU violation.\n");
		return IRQ_NONE;
	}



	/*TBD: print the abort region*/
	pr_err("[EMI MPU] Debug info start ----------------------------------------\n");
	pr_err("EMI_MPUS = %x, EMI_MPUT = %x.\n", dbg_s, dbg_t);
	pr_err("Current process is \"%s \" (pid: %i).\n", current->comm, current->pid);
	pr_err("Violation address is 0x%x.\n", dbg_t + emi_physical_offset);
	pr_err("Violation master ID is 0x%x.\n", master_ID);
	/*print out the murderer name*/
	master_name = __id2name(master_ID);
	pr_err("Violation domain ID is 0x%x.\n", domain_ID);
	pr_err("%s violation.\n", (wr_vio == 1) ?  "Write" : "Read");
	pr_err("Corrupted region is %d\n\r", region);
	if (dbg_pqry & OOR_VIO)
		pr_err("Out of range violation.\n");

	pr_err("[EMI MPU] Debug info end------------------------------------------\n");

#ifdef CONFIG_MTK_AEE_FEATURE
	/*FIXME: skip ca53 violation to trigger root-cause KE*/
	if ((0 != dbg_s) && (__id2mst(master_ID) != MST_ID_APMCU_0) && (__id2mst(master_ID) != MST_ID_APMCU_1)) {
		/*aee_kernel_exception("EMI MPU", "EMI MPU violation.\nEMP_MPUS = 0x%x,
		EMI_MPUT = 0x%x, EMI_MPU(PQR).\n", dbg_s, dbg_t+emi_physical_offset, dbg_pqry);*/
		aee_kernel_exception("EMI MPU", "EMI MPU violation.\nEMI_MPUS = 0x%x, EMI_MPUT = 0x%x, module is %s.\n",
		dbg_s, dbg_t+emi_physical_offset, master_name);
	}
#endif

	vio_addr = dbg_t + emi_physical_offset;

	return IRQ_HANDLED;
}
#endif

/* Acquire DRAM Setting for PASR/DPD */
void acquire_dram_setting(struct basic_dram_setting *pasrdpd)
{
	int ch_nr = MAX_CHANNELS;
	unsigned int emi_cona, emi_conh, col_bit, row_bit;
	unsigned int ch0_rank0_size, ch0_rank1_size, ch1_rank0_size, ch1_rank1_size;

	pasrdpd->channel_nr = ch_nr;

	emi_cona = readl(IOMEM(EMI_CONA));
	emi_conh = readl(IOMEM(EMI_CONH));

	/* unit 2Gb */
	ch0_rank0_size = (emi_conh >> 16) & 0xf;
	ch0_rank1_size = (emi_conh >> 20) & 0xf;
	ch1_rank0_size = (emi_conh >> 24) & 0xf;
	ch1_rank1_size = (emi_conh >> 28) & 0xf;

	/* channel 0 */
	{
		/* rank 0, 32 bits * 8 banks, unit Gb */
		pasrdpd->channel[0].rank[0].valid_rank = true;
		pasrdpd->channel[0].rank[0].segment_nr = 8;

		if (ch0_rank0_size == 0) {
			col_bit = ((emi_cona >> 4) & 0x03) + 9;
			row_bit = ((emi_cona >> 12) & 0x03) + 13;
			pasrdpd->channel[0].rank[0].rank_size = (1 << (row_bit + col_bit)) >> 22;
		} else {
			pasrdpd->channel[0].rank[0].rank_size = (ch0_rank0_size * 2);
		}

		/* rank 1 , 32 bits * 8 banks, unit Gb */
		if (0 != (emi_cona &  (1 << 17))) {
			pasrdpd->channel[0].rank[1].valid_rank = true;
			pasrdpd->channel[0].rank[1].segment_nr = 8;

			if (ch0_rank1_size == 0) {
				col_bit = ((emi_cona >> 6) & 0x03) + 9;
				row_bit = ((emi_cona >> 14) & 0x03) + 13;
				pasrdpd->channel[0].rank[1].rank_size = (1 << (row_bit + col_bit)) >> 22;
			} else {
				pasrdpd->channel[0].rank[1].rank_size = (ch0_rank1_size * 2);
			}
		} else {
			pasrdpd->channel[0].rank[1].valid_rank = false;
			pasrdpd->channel[0].rank[1].segment_nr = 0;
			pasrdpd->channel[0].rank[1].rank_size = 0;
		}
	}

	/* channel 1 exist */
	if (0 != (emi_cona & 0x01)) {
		/* rank0 setting, 32 bits * 8 banks, unit Gb */
		pasrdpd->channel[1].rank[0].valid_rank = true;
		pasrdpd->channel[1].rank[0].segment_nr = 8;

		if (ch1_rank0_size == 0) {
			col_bit = ((emi_cona >> 20) & 0x03) + 9;
			row_bit = ((emi_cona >> 28) & 0x03) + 13;
			pasrdpd->channel[1].rank[0].rank_size = (1 << (row_bit + col_bit)) >> 22;
		} else {
			pasrdpd->channel[1].rank[0].rank_size = (ch1_rank0_size * 2);
		}
		/* rank 1 exist, 32 bits * 8 banks, unit Gb */
		if (0 != (emi_cona &  (1 << 16))) {
			pasrdpd->channel[1].rank[1].valid_rank = true;
			pasrdpd->channel[1].rank[1].segment_nr = 8;
			if (ch1_rank1_size == 0) {
				col_bit = ((emi_cona >> 22) & 0x03) + 9;
				row_bit = ((emi_cona >> 30) & 0x03) + 13;
				pasrdpd->channel[1].rank[1].rank_size = (1 << (row_bit + col_bit)) >> 22;
			} else {
				pasrdpd->channel[1].rank[1].rank_size = (ch1_rank1_size * 2);
			}
		} else {
			pasrdpd->channel[1].rank[1].valid_rank = false;
			pasrdpd->channel[1].rank[1].segment_nr = 0;
			pasrdpd->channel[1].rank[1].rank_size = 0;
		}
	}
	/* channel 2 does not exist */
	else {
		pasrdpd->channel[1].rank[0].valid_rank = false;
		pasrdpd->channel[1].rank[0].segment_nr = 0;
		pasrdpd->channel[1].rank[0].rank_size = 0;

		pasrdpd->channel[1].rank[1].valid_rank = false;
		pasrdpd->channel[1].rank[1].segment_nr = 0;
		pasrdpd->channel[1].rank[1].rank_size = 0;
	}
}


/*
 * emi_mpu_set_region_protection: protect a region.
 * @start: start address of the region
 * @end: end address of the region
 * @region: EMI MPU region id
 * @access_permission: EMI MPU access permission
 * Return 0 for success, otherwise negative status code.
 */
int emi_mpu_set_region_protection(unsigned int start, unsigned int end, int region, unsigned int access_permission)
{
	int ret = 0;
	unsigned int tmp, tmp2;
	unsigned int ax_pm, ax_pm2;
	unsigned long flags;

	if ((end != 0) || (start != 0)) {
		/*Address 64KB alignment*/
		start -= emi_physical_offset;
		end -= emi_physical_offset;
		start = start >> 16;
		end = end >> 16;
		if (end < start)
			return -EINVAL;
	}

	ax_pm = (access_permission << 16) >> 16;
	ax_pm2 = (access_permission >> 16);

	spin_lock_irqsave(&emi_mpu_lock, flags);

	switch (region) {
	case 0:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUI) & 0xFFFF0000;
		tmp2 = emi_reg_read(EMI_MPUI_2ND) & 0xFFFF0000;
		emi_reg_write(0, EMI_MPUI);
		emi_reg_write(0, EMI_MPUI_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUA);
		emi_reg_write(tmp | ax_pm, EMI_MPUI);
		emi_reg_write(tmp2 | ax_pm2, EMI_MPUI_2ND);
		break;

	case 1:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUI) & 0x0000FFFF;
		tmp2 = emi_reg_read(EMI_MPUI_2ND) & 0x0000FFFF;
		emi_reg_write(0, EMI_MPUI);
		emi_reg_write(0, EMI_MPUI_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUB);
		emi_reg_write(tmp | (ax_pm << 16), EMI_MPUI);
		emi_reg_write(tmp2 | (ax_pm2 << 16), EMI_MPUI_2ND);
		break;

	case 2:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUJ) & 0xFFFF0000;
		tmp2 = emi_reg_read(EMI_MPUJ_2ND) & 0xFFFF0000;
		emi_reg_write(0, EMI_MPUJ);
		emi_reg_write(0, EMI_MPUJ_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUC);
		emi_reg_write(tmp | ax_pm, EMI_MPUJ);
		emi_reg_write(tmp2 | ax_pm2, EMI_MPUJ_2ND);
		break;

	case 3:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUJ) & 0x0000FFFF;
		tmp2 = emi_reg_read(EMI_MPUJ_2ND) & 0x0000FFFF;
		emi_reg_write(0, EMI_MPUJ);
		emi_reg_write(0, EMI_MPUJ_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUD);
		emi_reg_write(tmp | (ax_pm << 16), EMI_MPUJ);
		emi_reg_write(tmp2 | (ax_pm2 << 16), EMI_MPUJ_2ND);
		break;

	case 4:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUK) & 0xFFFF0000;
		tmp2 = emi_reg_read(EMI_MPUK_2ND) & 0xFFFF0000;
		emi_reg_write(0, EMI_MPUK);
		emi_reg_write(0, EMI_MPUK_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUE);
		emi_reg_write(tmp | ax_pm, EMI_MPUK);
		emi_reg_write(tmp2 | ax_pm2, EMI_MPUK_2ND);
		break;

	case 5:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUK) & 0x0000FFFF;
		tmp2 = emi_reg_read(EMI_MPUK_2ND) & 0x0000FFFF;
		emi_reg_write(0, EMI_MPUK);
		emi_reg_write(0, EMI_MPUK_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUF);
		emi_reg_write(tmp | (ax_pm << 16), EMI_MPUK);
		emi_reg_write(tmp2 | (ax_pm2 << 16), EMI_MPUK_2ND);
		break;

	case 6:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUL) & 0xFFFF0000;
		tmp2 = emi_reg_read(EMI_MPUL_2ND) & 0xFFFF0000;
		emi_reg_write(0, EMI_MPUL);
		emi_reg_write(0, EMI_MPUL_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUG);
		emi_reg_write(tmp | ax_pm, EMI_MPUL);
		emi_reg_write(tmp2 | ax_pm2, EMI_MPUL_2ND);
		break;

	case 7:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUL) & 0x0000FFFF;
		tmp2 = emi_reg_read(EMI_MPUL_2ND) & 0x0000FFFF;
		emi_reg_write(0, EMI_MPUL);
		emi_reg_write(0, EMI_MPUL_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUH);
		emi_reg_write(tmp | (ax_pm << 16), EMI_MPUL);
		emi_reg_write(tmp2 | (ax_pm2 << 16), EMI_MPUL_2ND);
		break;

	case 8:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUI2) & 0xFFFF0000;
		tmp2 = emi_reg_read(EMI_MPUI2_2ND) & 0xFFFF0000;
		emi_reg_write(0, EMI_MPUI2);
		emi_reg_write(0, EMI_MPUI2_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUA2);
		emi_reg_write(tmp | ax_pm, EMI_MPUI2);
		emi_reg_write(tmp2 | ax_pm2, EMI_MPUI2_2ND);
		break;

	case 9:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUI2) & 0x0000FFFF;
		tmp2 = emi_reg_read(EMI_MPUI2_2ND) & 0x0000FFFF;
		emi_reg_write(0, EMI_MPUI2);
		emi_reg_write(0, EMI_MPUI2_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUB2);
		emi_reg_write(tmp | (ax_pm << 16), EMI_MPUI2);
		emi_reg_write(tmp2 | (ax_pm2 << 16), EMI_MPUI2_2ND);
		break;

	case 10:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUJ2) & 0xFFFF0000;
		tmp2 = emi_reg_read(EMI_MPUJ2_2ND) & 0xFFFF0000;
		emi_reg_write(0, EMI_MPUJ2);
		emi_reg_write(0, EMI_MPUJ2_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUC2);
		emi_reg_write(tmp | ax_pm, EMI_MPUJ2);
		emi_reg_write(tmp2 | ax_pm2, EMI_MPUJ2_2ND);
		break;

	case 11:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUJ2) & 0x0000FFFF;
		tmp2 = emi_reg_read(EMI_MPUJ2_2ND) & 0x0000FFFF;
		emi_reg_write(0, EMI_MPUJ2);
		emi_reg_write(0, EMI_MPUJ2_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUD2);
		emi_reg_write(tmp | (ax_pm << 16), EMI_MPUJ2);
		emi_reg_write(tmp2 | (ax_pm2 << 16), EMI_MPUJ2_2ND);
		break;

	case 12:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUK2) & 0xFFFF0000;
		tmp2 = emi_reg_read(EMI_MPUK2_2ND) & 0xFFFF0000;
		emi_reg_write(0, EMI_MPUK2);
		emi_reg_write(0, EMI_MPUK2_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUE2);
		emi_reg_write(tmp | ax_pm, EMI_MPUK2);
		emi_reg_write(tmp2 | ax_pm2, EMI_MPUK2_2ND);
		break;

	case 13:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUK2) & 0x0000FFFF;
		tmp2 = emi_reg_read(EMI_MPUK2_2ND) & 0x0000FFFF;
		emi_reg_write(0, EMI_MPUK2);
		emi_reg_write(0, EMI_MPUK2_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUF2);
		emi_reg_write(tmp | (ax_pm << 16), EMI_MPUK2);
		emi_reg_write(tmp2 | (ax_pm2 << 16), EMI_MPUK2_2ND);
		break;

	case 14:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUL2) & 0xFFFF0000;
		tmp2 = emi_reg_read(EMI_MPUL2_2ND) & 0xFFFF0000;
		emi_reg_write(0, EMI_MPUL2);
		emi_reg_write(0, EMI_MPUL2_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUG2);
		emi_reg_write(tmp | ax_pm, EMI_MPUL2);
		emi_reg_write(tmp2 | ax_pm2, EMI_MPUL2_2ND);
		break;

	case 15:
		/* Marcos: Clear access right before setting MPU address (Mt6582 design) */
		tmp = emi_reg_read(EMI_MPUL2) & 0x0000FFFF;
		tmp2 = emi_reg_read(EMI_MPUL2_2ND) & 0x0000FFFF;
		emi_reg_write(0, EMI_MPUL2);
		emi_reg_write(0, EMI_MPUL2_2ND);
		emi_reg_write((start << 16) | end, EMI_MPUH2);
		emi_reg_write(tmp | (ax_pm << 16), EMI_MPUL2);
		emi_reg_write(tmp2 | (ax_pm2 << 16), EMI_MPUL2_2ND);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	spin_unlock_irqrestore(&emi_mpu_lock, flags);

	return ret;
}
EXPORT_SYMBOL(emi_mpu_set_region_protection);



#if 0
/*
 * emi_mpu_notifier_register: register a notifier.
 * master: MST_ID_xxx
 * notifier: the callback function
 * Return 0 for success, otherwise negative error code.
 */

int emi_mpu_notifier_register(int master, emi_mpu_notifier notifier)
{
	struct emi_mpu_notifier_block *block;
	static int emi_mpu_notifier_init;
	int i;

	if (master >= MST_INVALID)
		return -EINVAL;

	block = kmalloc(sizeof(struct emi_mpu_notifier_block), GFP_KERNEL);
	if (!block)
		return -ENOMEM;

	if (!emi_mpu_notifier_init) {
		for (i = 0; i < NR_MST; i++)
			INIT_LIST_HEAD(&(emi_mpu_notifier_list[i]));
		emi_mpu_notifier_init = 1;
	}

	block->notifier = notifier;
	list_add(&(block->list), &(emi_mpu_notifier_list[master]));

	return 0;
}
#endif

static ssize_t emi_mpu_show(struct device_driver *driver, char *buf)
{
	char *ptr = buf;
	unsigned int start, end;
	unsigned int reg_value, reg_value2;
	unsigned int d0, d1, d2, d3, d4, d5, d6, d7;
	static const char *permission[7] = {
		"No protect",
		"Only R/W for secure access",
		"Only R/W for secure access, and non-secure read access",
		"Only R/W for secure access, and non-secure write access",
		"Only R for secure/non-secure",
		"Both R/W are forbidden",
		"Only secure W is forbidden"
	};

	reg_value = emi_reg_read(EMI_MPUA);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 0 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUB);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 1 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUC);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 2 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUD);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 3 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUE);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 4 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUF);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 5 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUG);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 6 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUH);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 7 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUA2);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 8 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUB2);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 9 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUC2);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 10 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUD2);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 11 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUE2);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 12 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUF2);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 13 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUG2);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 14 --> 0x%x to 0x%x\n", start, end);

	reg_value = emi_reg_read(EMI_MPUH2);
	start = ((reg_value >> 16) << 16) + emi_physical_offset;
	end = ((reg_value & 0xFFFF) << 16) + emi_physical_offset + 0xFFFF;
	ptr += sprintf(ptr, "Region 15 --> 0x%x to 0x%x\n", start, end);

	ptr += sprintf(ptr, "\n");

	reg_value = emi_reg_read(EMI_MPUI);
	reg_value2 = emi_reg_read(EMI_MPUI_2ND);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	d4 = (reg_value2 & 0x7);
	d5 = (reg_value2 >> 3) & 0x7;
	d6 = (reg_value2 >> 6) & 0x7;
	d7 = (reg_value2 >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 0 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 0 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	d0 = ((reg_value>>16) & 0x7);
	d1 = ((reg_value>>16) >> 3) & 0x7;
	d2 = ((reg_value>>16) >> 6) & 0x7;
	d3 = ((reg_value>>16) >> 9) & 0x7;
	d4 = ((reg_value2>>16) & 0x7);
	d5 = ((reg_value2>>16) >> 3) & 0x7;
	d6 = ((reg_value2>>16) >> 6) & 0x7;
	d7 = ((reg_value2>>16) >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 1 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 1 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	reg_value = emi_reg_read(EMI_MPUJ);
	reg_value2 = emi_reg_read(EMI_MPUJ_2ND);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	d4 = (reg_value2 & 0x7);
	d5 = (reg_value2 >> 3) & 0x7;
	d6 = (reg_value2 >> 6) & 0x7;
	d7 = (reg_value2 >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 2 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 2 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	d0 = ((reg_value>>16) & 0x7);
	d1 = ((reg_value>>16) >> 3) & 0x7;
	d2 = ((reg_value>>16) >> 6) & 0x7;
	d3 = ((reg_value>>16) >> 9) & 0x7;
	d4 = ((reg_value2>>16) & 0x7);
	d5 = ((reg_value2>>16) >> 3) & 0x7;
	d6 = ((reg_value2>>16) >> 6) & 0x7;
	d7 = ((reg_value2>>16) >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 3 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 3 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	reg_value = emi_reg_read(EMI_MPUK);
	reg_value2 = emi_reg_read(EMI_MPUK_2ND);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	d4 = (reg_value2 & 0x7);
	d5 = (reg_value2 >> 3) & 0x7;
	d6 = (reg_value2 >> 6) & 0x7;
	d7 = (reg_value2 >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 4 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 4 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	d0 = ((reg_value>>16) & 0x7);
	d1 = ((reg_value>>16) >> 3) & 0x7;
	d2 = ((reg_value>>16) >> 6) & 0x7;
	d3 = ((reg_value>>16) >> 9) & 0x7;
	d4 = ((reg_value2>>16) & 0x7);
	d5 = ((reg_value2>>16) >> 3) & 0x7;
	d6 = ((reg_value2>>16) >> 6) & 0x7;
	d7 = ((reg_value2>>16) >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 5 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 5 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	reg_value = emi_reg_read(EMI_MPUL);
	reg_value2 = emi_reg_read(EMI_MPUL_2ND);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	d4 = (reg_value2 & 0x7);
	d5 = (reg_value2 >> 3) & 0x7;
	d6 = (reg_value2 >> 6) & 0x7;
	d7 = (reg_value2 >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 6 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 6 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	d0 = ((reg_value>>16) & 0x7);
	d1 = ((reg_value>>16) >> 3) & 0x7;
	d2 = ((reg_value>>16) >> 6) & 0x7;
	d3 = ((reg_value>>16) >> 9) & 0x7;
	d4 = ((reg_value2>>16) & 0x7);
	d5 = ((reg_value2>>16) >> 3) & 0x7;
	d6 = ((reg_value2>>16) >> 6) & 0x7;
	d7 = ((reg_value2>>16) >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 7 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 7 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	reg_value = emi_reg_read(EMI_MPUI2);
	reg_value2 = emi_reg_read(EMI_MPUI2_2ND);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	d4 = (reg_value2 & 0x7);
	d5 = (reg_value2 >> 3) & 0x7;
	d6 = (reg_value2 >> 6) & 0x7;
	d7 = (reg_value2 >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 8 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 8 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	d0 = ((reg_value>>16) & 0x7);
	d1 = ((reg_value>>16) >> 3) & 0x7;
	d2 = ((reg_value>>16) >> 6) & 0x7;
	d3 = ((reg_value>>16) >> 9) & 0x7;
	d4 = ((reg_value2>>16) & 0x7);
	d5 = ((reg_value2>>16) >> 3) & 0x7;
	d6 = ((reg_value2>>16) >> 6) & 0x7;
	d7 = ((reg_value2>>16) >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 9 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 9 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	reg_value = emi_reg_read(EMI_MPUJ2);
	reg_value2 = emi_reg_read(EMI_MPUJ2_2ND);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	d4 = (reg_value2 & 0x7);
	d5 = (reg_value2 >> 3) & 0x7;
	d6 = (reg_value2 >> 6) & 0x7;
	d7 = (reg_value2 >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 10 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 10 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	d0 = ((reg_value>>16) & 0x7);
	d1 = ((reg_value>>16) >> 3) & 0x7;
	d2 = ((reg_value>>16) >> 6) & 0x7;
	d3 = ((reg_value>>16) >> 9) & 0x7;
	d4 = ((reg_value2>>16) & 0x7);
	d5 = ((reg_value2>>16) >> 3) & 0x7;
	d6 = ((reg_value2>>16) >> 6) & 0x7;
	d7 = ((reg_value2>>16) >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 11 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 11 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);


	reg_value = emi_reg_read(EMI_MPUK2);
	reg_value2 = emi_reg_read(EMI_MPUK2_2ND);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	d4 = (reg_value2 & 0x7);
	d5 = (reg_value2 >> 3) & 0x7;
	d6 = (reg_value2 >> 6) & 0x7;
	d7 = (reg_value2 >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 12 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 12 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	d0 = ((reg_value>>16) & 0x7);
	d1 = ((reg_value>>16) >> 3) & 0x7;
	d2 = ((reg_value>>16) >> 6) & 0x7;
	d3 = ((reg_value>>16) >> 9) & 0x7;
	d4 = ((reg_value2>>16) & 0x7);
	d5 = ((reg_value2>>16) >> 3) & 0x7;
	d6 = ((reg_value2>>16) >> 6) & 0x7;
	d7 = ((reg_value2>>16) >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 13 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 13 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	reg_value = emi_reg_read(EMI_MPUL2);
	reg_value2 = emi_reg_read(EMI_MPUL2_2ND);
	d0 = (reg_value & 0x7);
	d1 = (reg_value >> 3) & 0x7;
	d2 = (reg_value >> 6) & 0x7;
	d3 = (reg_value >> 9) & 0x7;
	d4 = (reg_value2 & 0x7);
	d5 = (reg_value2 >> 3) & 0x7;
	d6 = (reg_value2 >> 6) & 0x7;
	d7 = (reg_value2 >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 14 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 14 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	d0 = ((reg_value>>16) & 0x7);
	d1 = ((reg_value>>16) >> 3) & 0x7;
	d2 = ((reg_value>>16) >> 6) & 0x7;
	d3 = ((reg_value>>16) >> 9) & 0x7;
	d4 = ((reg_value2>>16) & 0x7);
	d5 = ((reg_value2>>16) >> 3) & 0x7;
	d6 = ((reg_value2>>16) >> 6) & 0x7;
	d7 = ((reg_value2>>16) >> 9) & 0x7;
	ptr += sprintf(ptr, "Region 15 --> d0 = %s, d1 = %s, d2 = %s, d3 = %s\n",
					permission[d0],  permission[d1],  permission[d2], permission[d3]);
	ptr += sprintf(ptr, "Region 15 --> d4 = %s, d5 = %s, d6 = %s, d7 = %s\n",
					permission[d4],  permission[d5],  permission[d6], permission[d7]);

	return strlen(buf);
}

static ssize_t emi_mpu_store(struct device_driver *driver, const char *buf, size_t count)
{
	int i;
	unsigned int start_addr;
	unsigned int end_addr;
	unsigned int region;
	unsigned int access_permission;
	char *command;
	char *ptr;
	char *token[5];

	if ((strlen(buf) + 1) > MAX_EMI_MPU_STORE_CMD_LEN) {
		pr_err("emi_mpu_store command overflow.");
		return count;
	}
	pr_err("emi_mpu_store: %s\n", buf);

	command = kmalloc((size_t)MAX_EMI_MPU_STORE_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;

	strcpy(command, buf);
	ptr = (char *)buf;

	if (!strncmp(buf, EN_MPU_STR, strlen(EN_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_devel("token[%d] = %s\n", i, token[i]);
			i++;
		}
		for (i = 0; i < 5; i++)
			pr_devel("token[%d] = %s\n", i, token[i]);

		start_addr = kstrtoul(token[1], 16, (unsigned long *) &token[1]);
		end_addr = kstrtoul(token[2], 16, (unsigned long *) &token[2]);
		region = kstrtoul(token[3], 16, (unsigned long *) &token[3]);
		access_permission = kstrtoul(token[4], 16, (unsigned long *) &token[4]);
		emi_mpu_set_region_protection(start_addr, end_addr, region, access_permission);
		pr_err("Set EMI_MPU: start: 0x%x, end: 0x%x, region: %d, permission: 0x%x.\n",
				start_addr, end_addr, region, access_permission);
	} else if (!strncmp(buf, DIS_MPU_STR, strlen(DIS_MPU_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_devel("token[%d] = %s\n", i, token[i]);
			i++;
		}
		for (i = 0; i < 5; i++)
			pr_devel("token[%d] = %s\n", i, token[i]);

		start_addr = kstrtoul(token[1], 16, (unsigned long *) &token[1]);
		end_addr = kstrtoul(token[2], 16, (unsigned long *) &token[2]);
		region = kstrtoul(token[3], 16, (unsigned long *) &token[3]);
		emi_mpu_set_region_protection(0x0, 0x0, region,
							SET_ACCESS_PERMISSON(NO_PROTECTION,
							NO_PROTECTION, NO_PROTECTION, NO_PROTECTION,
							NO_PROTECTION, NO_PROTECTION, NO_PROTECTION,
							NO_PROTECTION));
	} else
		pr_err("Unknown emi_mpu command.\n");

	kfree(command);

	return count;
}

DRIVER_ATTR(mpu_config, 0644, emi_mpu_show, emi_mpu_store);

void mtk_search_full_pgtab(void)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	unsigned long addr;
#ifndef CONFIG_ARM_LPAE
	pte_t *pte;
	unsigned long addr_2nd, addr_2nd_end;
#endif
	unsigned int v_addr = vio_addr;

	for (addr = 0xC0000000; addr < 0xFFF00000; addr += 0x100000) {
		pgd = pgd_offset(&init_mm, addr);
		if (pgd_none(*pgd) || !pgd_present(*pgd))
			continue;

		pud = pud_offset(pgd, addr);
		if (pud_none(*pud) || !pud_present(*pud))
			continue;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd) || !pmd_present(*pmd))
			continue;

#ifndef CONFIG_ARM_LPAE
		if ((pmd_val(*pmd) & PMD_TYPE_MASK) == PMD_TYPE_TABLE) {
			/* Page table entry*/
			addr_2nd = addr;
			addr_2nd_end = addr_2nd + 0x100000;
			for (; addr_2nd < (addr_2nd_end); addr_2nd += 0x1000) {
				pte = pte_offset_map(pmd, addr_2nd);
				if (((unsigned long)v_addr & PAGE_MASK)
						== ((unsigned long)pte_val(*(pte)) & PAGE_MASK)) {
					return;
				}
			}
		} else if (((unsigned long)pmd_val(*(pmd)) & SECTION_MASK)
						== ((unsigned long)v_addr & SECTION_MASK)) {
			return;
		}
	}
#else
	/* TBD */
#endif

}

void emi_mpu_work_callback(struct work_struct *work)
{
	mtk_search_full_pgtab();
}

static ssize_t pgt_scan_show(struct device_driver *driver, char *buf)
{
	return 0;
}

static ssize_t pgt_scan_store(struct device_driver *driver, const char *buf, size_t count)
{
	unsigned int value;
	unsigned int ret;

	if (unlikely(kstrtoint(buf, 0, &value) != 1))
		return -EINVAL;

	if (value == 1)
		ret = queue_work(emi_mpu_workqueue, &emi_mpu_work);

	if (!ret)
		pr_devel("EMI MPU submit workqueue failed, ret = %d\n", ret);

	return count;
}
DRIVER_ATTR(pgt_scan, 0644, pgt_scan_show, pgt_scan_store);

#ifdef ENABLE_EMI_CHKER
static void emi_axi_set_chker(const unsigned int setting)
{
	int value;

	value = emi_reg_read(EMI_CHKER);
	value &= ~(0x7 << 16);
	value |= (setting);
	emi_reg_write(value, EMI_CHKER);
}

static void emi_axi_set_master(const unsigned int setting)
{
	int value;

	value = emi_reg_read(EMI_CHKER);
	value &= ~(0x0F << AXI_NON_ALIGN_CHK_MST);
	value |= (setting & 0xF) << AXI_NON_ALIGN_CHK_MST;
	emi_reg_write(value, EMI_CHKER);
}

static void emi_axi_dump_info(int aee_ke_en)
{
	int value, master_ID;
	char *master_name;

	value = emi_reg_read(EMI_CHKER);
	master_ID = (value & 0x0000FFFF);

	if (value & 0x0000FFFF) {
		pr_err("AXI violation.\n");
		pr_err("[EMI MPU AXI] Debug info start ----------------------------------------\n");
		pr_err("EMI_CHKER = %x.\n", value);
		pr_err("Violation address is 0x%x.\n", emi_reg_read(EMI_CHKER_ADR));
		pr_err("Violation master ID is 0x%x.\n", master_ID);
		pr_err("Violation type is: AXI_ADR_CHK_EN(%d), AXI_LOCK_CHK_EN(%d), AXI_NON_ALIGN_CHK_EN(%d).\n",
				(value & (1 << AXI_ADR_VIO)) ? 1 : 0, (value & (1 << AXI_LOCK_ISSUE)) ? 1 : 0,
				(value & (1 << AXI_NON_ALIGN_ISSUE)) ? 1 : 0);
		pr_err("%s violation.\n", (value & (1 << AXI_VIO_WR)) ?  "Write" : "Read");
		pr_err("[EMI MPU AXI] Debug info end ----------------------------------------\n");

		master_name = __id2name(master_ID);

#ifdef CONFIG_MTK_AEE_FEATURE
		if (aee_ke_en)
			aee_kernel_exception("EMI MPU AXI", "AXI violation.\EMI_CHKER = 0x%x, module is %s.\n",
								value, master_name);
#endif
		/* clear AXI checker status */
		emi_reg_write((1 << AXI_VIO_CLR) | emi_reg_read(EMI_CHKER), EMI_CHKER);
	}
}

static void emi_axi_vio_timer_func(unsigned long a)
{
	emi_axi_dump_info(1);
	mod_timer(&emi_axi_vio_timer, jiffies + AXI_VIO_MONITOR_TIME);
}

static ssize_t emi_axi_vio_show(struct device_driver *driver, char *buf)
{
	int value;

	value = emi_reg_read(EMI_CHKER);
	emi_axi_dump_info(0);

	return snprintf(buf, PAGE_SIZE, "AXI vio setting is: ADR_CHK_EN %s, LOCK_CHK_EN %s, NON_ALIGN_CHK_EN %s\n",
					(value & (1 << AXI_ADR_CHK_EN)) ? "ON" : "OFF",
					(value & (1 << AXI_LOCK_CHK_EN)) ? "ON" : "OFF",
					(value & (1 << AXI_NON_ALIGN_CHK_EN)) ? "ON" : "OFF");
}

ssize_t emi_axi_vio_store(struct device_driver *driver, const char *buf, size_t count)
{
	int value;
	/*assign timer to CPU0 to avoid CPU plug-out and timer will be unavailable*/
	int cpu = 0;

	value = emi_reg_read(EMI_CHKER);

	if (!strncmp(buf, "ADR_CHK_ON", strlen("ADR_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_ADR_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "LOCK_CHK_ON", strlen("LOCK_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_LOCK_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "NON_ALIGN_CHK_ON", strlen("NON_ALIGN_CHK_ON"))) {
		emi_axi_set_chker(1 << AXI_NON_ALIGN_CHK_EN);
		add_timer_on(&emi_axi_vio_timer, cpu);
	} else if (!strncmp(buf, "OFF", strlen("OFF"))) {
		emi_axi_set_chker(0);
		del_timer(&emi_axi_vio_timer);
	}
	return count;
}

DRIVER_ATTR(emi_axi_vio, 0644, emi_axi_vio_show,       emi_axi_vio_store);

#endif /*ifdef ENABLE_EMI_CHKER*/

#ifdef ENABLE_EMI_WATCH_POINT
static void emi_wp_set_address(unsigned int address)
{
	emi_reg_write(address - emi_physical_offset, EMI_WP_ADR);
}

/* 2^ range bytes */
static void emi_wp_set_range(unsigned int range)
{
	unsigned int value;

	value = emi_reg_read(EMI_WP_CTRL);
	value = (value & (~EMI_WP_RANGE)) | range;
	emi_reg_write(value, EMI_WP_CTRL);
}

static void emi_wp_set_monitor_type(unsigned int type)
{
	unsigned int value;

	value = emi_reg_read(EMI_WP_CTRL);
	value = (value & (~EMI_WP_RW_MONITOR)) | (type << EMI_WP_RW_MONITOR_SHIFT);
	emi_reg_write(value, EMI_WP_CTRL);
}

static void emi_wp_enable(int enable)
{
	unsigned int value;

    /* Enable WP */
	value = emi_reg_read(EMI_CHKER);
	value = (value & ~(1 << EMI_WP_ENABLE_SHIFT)) | (enable << EMI_WP_ENABLE_SHIFT);
	emi_reg_write(value, EMI_CHKER);
}

static void emi_wp_slave_error_enable(unsigned int enable)
{
	unsigned int value;

	value = emi_reg_read(EMI_WP_CTRL);
	value = (value & ~(1 << EMI_WP_SLVERR_SHIFT)) | (enable << EMI_WP_SLVERR_SHIFT);
	emi_reg_write(value, EMI_WP_CTRL);
}

static void emi_wp_int_enable(unsigned int enable)
{
	unsigned int value;

	value = emi_reg_read(EMI_WP_CTRL);
	value = (value & ~(1 << EMI_WP_INT_SHIFT)) | (enable << EMI_WP_INT_SHIFT);
	emi_reg_write(value, EMI_WP_CTRL);
}

static void emi_wp_clr_status(void)
{
	unsigned int value;
	int result;

	value = emi_reg_read(EMI_CHKER);
	value |= 1 << EMI_WP_VIO_CLR_SHIFT;
	emi_reg_write(value, EMI_CHKER);

	result = emi_reg_read(EMI_CHKER) & EMI_WP_AXI_ID;
	result |= emi_reg_read(EMI_CHKER_TYPE);
	result |= emi_reg_read(EMI_CHKER_ADR);

	if (result)
		pr_err("[EMI_WP] Clear WP status fail!!!!!!!!!!!!!!\n");
}

void emi_wp_get_status(void)
{
	unsigned int value, master_ID;
	char *master_name;

	value = emi_reg_read(EMI_CHKER);

	if ((value & 0x80000000) == 0) {
		pr_err("[EMI_WP] No watch point hit\n");
		return;
	}

	master_ID = (value & EMI_WP_AXI_ID);
	pr_err("[EMI_WP] Violation master ID is 0x%x.\n", master_ID);
	pr_err("[EMI_WP] Violation Address is : 0x%X\n", emi_reg_read(EMI_CHKER_ADR) + emi_physical_offset);

	master_name = __id2name(master_ID);
	pr_err("[EMI_WP] EMI_CHKER = 0x%x, module is %s.\n", value, master_name);

	value = emi_reg_read(EMI_CHKER_TYPE);
	pr_err("[EMI_WP] Transaction Type is : %d beat, %d byte, %s burst type (0x%X)\n", (value & 0xF) + 1,
			1 << ((value >> 4) & 0x7), (value >> 7 & 1) ? "INCR" : "WRAP", value);

	emi_wp_clr_status();
}

static int emi_wp_set(unsigned int enable, unsigned int address, unsigned int range, unsigned int rw)
{
	if (address < emi_physical_offset) {
		pr_err("[EMI_WP] Address error, you can't set address less than 0x%X\n", emi_physical_offset);
		return -1;
	}
	if (range < 4 || range > 32) {
		pr_err("[EMI_WP] Range error, you can't set range less than 16 bytes and more than 4G bytes\n");
		return -1;
	}

	emi_wp_set_monitor_type(rw);
	emi_wp_set_address(address);
	emi_wp_set_range(range);
	emi_wp_slave_error_enable(1);
	emi_wp_int_enable(0);
	emi_wp_enable(enable);

	return 0;
}

static ssize_t emi_wp_vio_show(struct device_driver *driver, char *buf)
{
	unsigned int value, master_ID, type, vio_addr;
	char *master_name;

	value = emi_reg_read(EMI_CHKER);

	if ((value & 0x80000000) == 0)
		return snprintf(buf, PAGE_SIZE, "[EMI_WP] No watch point hit\n");

	master_ID = (value & EMI_WP_AXI_ID);
	master_name = __id2name(master_ID);

	type = emi_reg_read(EMI_CHKER_TYPE);
	vio_addr = emi_reg_read(EMI_CHKER_ADR) + emi_physical_offset;
	emi_wp_clr_status();
	return snprintf(buf, PAGE_SIZE,
	"[EMI WP] vio setting is: CHKER 0x%X, module is %s, Address is : 0x%X, Transaction Type is : %d beat, %d byte, %s burst type (0x%X)\n",
					value, master_name, vio_addr, (type & 0xF) + 1, 1 << ((type >> 4) & 0x7),
					(type >> 7 & 1) ? "INCR" : "WRAP", type);
}

ssize_t emi_wp_vio_store(struct device_driver *driver, const char *buf, size_t count)
{
	int i;
	unsigned int wp_addr;
	unsigned int range, start_addr, end_addr;
	unsigned int rw;
	char *command;
	char *ptr;
	char *token[5];

	if ((strlen(buf) + 1) > MAX_EMI_MPU_STORE_CMD_LEN) {
		pr_err("emi_wp_store command overflow.");
		return count;
	}
	pr_err("emi_wp_store: %s\n", buf);

	command = kmalloc((size_t)MAX_EMI_MPU_STORE_CMD_LEN, GFP_KERNEL);
	if (!command)
		return count;

	strcpy(command, buf);
	ptr = (char *)buf;

	if (!strncmp(buf, EN_WP_STR, strlen(EN_WP_STR))) {
		i = 0;
		while (ptr != NULL) {
			ptr = strsep(&command, " ");
			token[i] = ptr;
			pr_devel("token[%d] = %s\n", i, token[i]);
			i++;
		}
		for (i = 0; i < 4; i++)
			pr_devel("token[%d] = %s\n", i, token[i]);

		wp_addr = kstrtoul(token[1], 16, (unsigned long *) &token[1]);
		range = kstrtoul(token[2], 16, (unsigned long *) &token[2]);
		rw = kstrtoul(token[3], 16, (unsigned long *) &token[3]);
		emi_wp_set(1, wp_addr, range, rw);
		start_addr = (wp_addr >> range) << range;
		end_addr = start_addr + (1 << range)     - 1;
		pr_err("Set EMI_WP: address: 0x%x, range:%d, start addr: 0x%x, end addr: 0x%x,  rw: %d .\n",
				wp_addr, range, start_addr, end_addr, rw);
	} else if (!strncmp(buf, DIS_WP_STR, strlen(DIS_WP_STR))) {
			i = 0;
			while (ptr != NULL) {
				ptr = strsep(&command, " ");
				token[i] = ptr;
				pr_devel("token[%d] = %s\n", i, token[i]);
				i++;
			}
			for (i = 0; i < 4; i++)
				pr_devel("token[%d] = %s\n", i, token[i]);

			wp_addr = kstrtoul(token[1], 16, (unsigned long *) &token[1]);
			range = kstrtoul(token[2], 16, (unsigned long *) &token[2]);
			rw = kstrtoul(token[3], 16, (unsigned long *) &token[3]);
			emi_wp_set(0, 0x40000000, 4, 2);

	} else
		pr_err("Unknown emi_wp command.\n");

	kfree(command);

	return count;
}


DRIVER_ATTR(emi_wp_vio, 0644, emi_wp_vio_show, emi_wp_vio_store);
#endif /*#ifdef ENABLE_EMI_WATCH_POINT */


#define AP_REGION_ID 15
static void protect_ap_region(void)
{
	unsigned int ap_mem_mpu_id, ap_mem_mpu_attr;
	unsigned int kernel_base;
	phys_addr_t dram_size;

	kernel_base = PHYS_OFFSET;
	dram_size = get_max_DRAM_size();

	ap_mem_mpu_id = AP_REGION_ID;
	ap_mem_mpu_attr = SET_ACCESS_PERMISSON(FORBIDDEN, NO_PROTECTION, FORBIDDEN,
						NO_PROTECTION, FORBIDDEN, FORBIDDEN, FORBIDDEN, NO_PROTECTION);

	emi_mpu_set_region_protection(kernel_base, (kernel_base+dram_size-1), ap_mem_mpu_id, ap_mem_mpu_attr);
}

static struct platform_driver emi_mpu_ctrl_platform_drv = {
	.driver = {
		.name = "emi_mpu_ctrl",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	}
};

static int __init emi_mpu_mod_init(void)
{
	int ret;
	struct basic_dram_setting DRAM_setting;
	struct device_node *node;
	unsigned int mpu_irq = 0;

	/* DTS version */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-emi");
	if (node) {
		mpu_irq = irq_of_parse_and_map(node, 0);
		if (EMI_BASE_ADDR == NULL)
			EMI_BASE_ADDR = of_iomap(node, 0);
	else
			return -1;
	}

	spin_lock_init(&emi_mpu_lock);

	if (1)
		emi_physical_offset = 0x40000000;
	else {/*enable 4G*/
		emi_physical_offset = 0;
	}

	/*emi will not request irq when tee config is disable*/
#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
	ret = request_irq(mpu_irq, (irq_handler_t)mpu_violation_irq,
						IRQF_TRIGGER_LOW | IRQF_SHARED, "mt_emi_mpu",
						&emi_mpu_ctrl_platform_drv);
	if (ret != 0) {
		pr_err("Fail to request EMI_MPU interrupt. Error = %d.\n", ret);
		return ret;
	}
#endif

	protect_ap_region();
	acquire_dram_setting(&DRAM_setting);

#ifdef ENABLE_EMI_CHKER
    /* AXI violation monitor setting and timer function create */
	emi_reg_write((1 << AXI_VIO_CLR) | emi_reg_read(EMI_CHKER), EMI_CHKER);
	emi_axi_set_master(MASTER_ALL);
	init_timer(&emi_axi_vio_timer);
	emi_axi_vio_timer.expires = jiffies + AXI_VIO_MONITOR_TIME;
	emi_axi_vio_timer.function = &emi_axi_vio_timer_func;
	emi_axi_vio_timer.data = ((unsigned long) 0);
#endif /*#ifdef ENABLE_EMI_CHKER*/

#if !defined(USER_BUILD_KERNEL)
#ifdef ENABLE_EMI_CHKER
    /* Enable AXI 4KB boundary violation monitor timer */
	emi_axi_set_chker(1 << AXI_ADR_CHK_EN);
    /* add_timer_on(&emi_axi_vio_timer, 0); */
#endif

    /* register driver and create sysfs files */
	ret = platform_driver_register(&emi_mpu_ctrl_platform_drv);
	if (ret)
		pr_err("Fail to register EMI_MPU driver.\n");

	ret = driver_create_file(&emi_mpu_ctrl_platform_drv.driver, &driver_attr_mpu_config);
	if (ret)
		pr_err("Fail to create MPU config sysfs file.\n");

#ifdef ENABLE_EMI_CHKER
	ret = driver_create_file(&emi_mpu_ctrl_platform_drv.driver, &driver_attr_emi_axi_vio);
	if (ret)
		pr_err("Fail to create AXI violation monitor sysfs file.\n");

#endif
	ret = driver_create_file(&emi_mpu_ctrl_platform_drv.driver, &driver_attr_pgt_scan);
	if (ret)
		pr_err("Fail to create pgt scan sysfs file.\n");

#ifdef ENABLE_EMI_WATCH_POINT
	ret = driver_create_file(&emi_mpu_ctrl_platform_drv.driver, &driver_attr_emi_wp_vio);
	if (ret)
		pr_err("Fail to create WP violation monitor sysfs file.\n");

#endif
#endif

    /* Create a workqueue to search pagetable entry */
	emi_mpu_workqueue = create_singlethread_workqueue("emi_mpu");
	INIT_WORK(&emi_mpu_work, emi_mpu_work_callback);
	return 0;
}

static void __exit emi_mpu_mod_exit(void)
{
}

module_init(emi_mpu_mod_init);
module_exit(emi_mpu_mod_exit);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK EMI DRIVER");
MODULE_LICENSE("GPL");
