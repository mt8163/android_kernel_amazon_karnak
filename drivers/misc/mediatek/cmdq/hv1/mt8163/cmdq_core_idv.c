#include <linux/seq_file.h>

#include "cmdq_core.h"


void cmdqCorePrintStatus_idv(char *buf)
{
#ifdef CMDQ_PWR_AWARE
		buf += sprintf(buf, "====== Clock Status =======\n");
		buf += sprintf(buf, "MT_CG_INFRA_GCE: %d\n",
				   cmdq_core_clock_is_on(CMDQ_CLK_INFRA_GCE));
	/*			   clock_is_on(MT_CG_INFRA_GCE), clock_is_on(MT_CG_DISP0_MUTEX_32K));*/
	/* CCF */
#endif
}

void cmdqCorePrintStatusSeq_idv(struct seq_file *m)
{
#ifdef CMDQ_PWR_AWARE
	seq_puts(m, "====== Clock Status =======\n");
	seq_printf(m, "MT_CG_INFRA_GCE: %d\n",
		   cmdq_core_clock_is_on(CMDQ_CLK_INFRA_GCE));
/*		   clock_is_on(MT_CG_INFRA_GCE), clock_is_on(MT_CG_DISP0_MUTEX_32K));*/
/* CCF */
#endif
}

#ifdef CMDQ_OF_SUPPORT
char *cmdq_core_get_clk_name(CMDQ_CLK_ENUM clk_enum)
{
	switch (clk_enum) {
	case CMDQ_CLK_INFRA_GCE:
		return "MT_CG_INFRA_GCE";
	case CMDQ_CLK_DISP0_SMI_COMMON:
		return "MT_CG_DISP0_SMI_COMMON";
	case CMDQ_CLK_DISP0_SMI_LARB0:
		return "MT_CG_DISP0_SMI_LARB0";
	case CMDQ_CLK_DISP0_CAM_MDP:
		return "MT_CG_DISP0_CAM_MDP";
	case CMDQ_CLK_DISP0_MDP_RDMA0:
		return "MT_CG_DISP0_MDP_RDMA0";
	case CMDQ_CLK_DISP0_MDP_RSZ0:
		return "MT_CG_DISP0_MDP_RSZ0";
	case CMDQ_CLK_DISP0_MDP_RSZ1:
		return "MT_CG_DISP0_MDP_RSZ1";
	case CMDQ_CLK_DISP0_MDP_TDSHP0:
		return "MT_CG_DISP0_MDP_TDSHP0";
	case CMDQ_CLK_DISP0_MDP_WROT0:
		return "MT_CG_DISP0_MDP_WROT0";
	case CMDQ_CLK_DISP0_MDP_WDMA:
		return "MT_CG_DISP0_MDP_WDMA";
	default:
		CMDQ_ERR("invalid clk id=%d\n", clk_enum);
		return "UNKWON";
	}
}

#endif


