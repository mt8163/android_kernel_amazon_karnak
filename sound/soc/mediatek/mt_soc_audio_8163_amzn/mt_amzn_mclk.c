#include <linux/clk.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#ifndef CONFIG_MTK_SMI_VARIANT /* Power Domain */
#include <linux/pm_runtime.h>
#else
#include "mt_smi.h"
#endif
#include "mt_amzn_mclk.h"

typedef enum {
	ISP_BASE_ADDR = 0,
	ISP_IMGSYS_CONFIG_BASE_ADDR,
	ISP_MIPI_ANA_BASE_ADDR,
	ISP_CAM_BASEADDR_NUM
} ISP_CAM_BASEADDR_ENUM;

static unsigned long gISPSYS_Reg[ISP_CAM_BASEADDR_NUM];

#define ISP_ADDR                 (gISPSYS_Reg[ISP_BASE_ADDR])
#define ISP_IMGSYS_BASE          (gISPSYS_Reg[ISP_IMGSYS_CONFIG_BASE_ADDR])
#define ISP_ADDR_CAMINF          (gISPSYS_Reg[ISP_IMGSYS_CONFIG_BASE_ADDR])
#define ISP_MIPI_ANA_ADDR        (gISPSYS_Reg[ISP_MIPI_ANA_BASE_ADDR])

#define ISP_WR32(addr, data)     iowrite32(data, (void *)(addr))
#define ISP_RD32(addr)           ioread32((void *)(addr))

#define SENINF_TOP_CTRL          (void *)(ISP_ADDR + 0x4000)
#define SENINF_TG1_PH_CNT        (void *)(ISP_ADDR + 0x4200)
#define SENINF_TG1_SEN_CK        (void *)(ISP_ADDR + 0x4204)
#define SENINF1_CTRL             (void *)(ISP_ADDR + 0x4100)
#define SENINF1_MUX_CTRL         (void *)(ISP_ADDR + 0x4120)

#define SENINF1_EN_SHIFT            0
#define SENINF1_EN_MASK             (1 << SENINF1_EN_SHIFT)
#define SENINF1_MUX_EN_SHIFT        31
#define SENINF1_MUX_EN_MASK         (1 << SENINF1_MUX_EN_SHIFT)

#define SENINF12_PCLK_MASK          0xc00
#define SENINF12_PCLK_VAL           0x300

#define SENINF_TG1_CLKFL_MASK       0x3f
#define SENINF_TG1_CLKRS_MASK       0x3f00
#define SENINF_TG1_CLKCNT_MASK      0x3f0000
#define SENINF_TG1_CLKFL_SHIFT      0
#define SENINF_TG1_CLKRS_SHIFT      8
#define SENINF_TG1_CLKCNT_SHIFT     16

#define SENINF_TG1_TGCLK_SEL_SHIFT  0
#define SENINF_TG1_TGCLK_SEL_MASK   0x3
#define SENINF_TG1_CLKFL_POL_SHIFT  2
#define SENINF_TG1_CLKFL_POL_MASK   0x4
#define SENINF_TG1_PADCLK_INV_SHIFT 6
#define SENINF_TG1_PADCLK_INV_MASK  (1 << SENINF_TG1_PADCLK_INV_SHIFT)
#define SENINF_TG1_CLK_POL_SHIFT    28
#define SENINF_TG1_CLK_POL_MASK     (1 << SENINF_TG1_CLK_POL_SHIFT)
#define SENINF_TG1_PCEN_SHIFT       31
#define SENINF_TG1_PCEN_MASK        (1 << SENINF_TG1_PCEN_SHIFT)

#define PLL_FREQ_48_MHZ             48000000
#define CLK_CHILD                   0
#define CLK_PARENT                  1
#define LARB_COUNT                  2

#ifndef CONFIG_MTK_SMI_VARIANT /* Power Domain */
struct device *g_pmdev_isp;
#endif

struct CLK_STRUCT {
	char *clkname;
	struct clk *clock;
};
static struct CLK_STRUCT ispclk[] = {
#ifndef CONFIG_MTK_SMI_VARIANT
	{"MM_SMI_COMMON", NULL},
#endif
	{"IMG_SEN_TG", NULL},
	{"IMG_SEN_CAM", NULL},
#ifndef CONFIG_MTK_SMI_VARIANT
	{"IMG_LARB2_SMI", NULL},
#endif
};

static struct CLK_STRUCT ccfclk[] = {
	{"TOP_CAMTG_SEL", NULL}, /* CLK_CHILD */
	{"TOP_UNIVPLL_D26", NULL}, /* CLK_PARENT 48MHz */
	{"TOP_UNIVPLL2_D2", NULL}, /* CLK_PARENT 208MHz */
};

static long of_get_phys_addr(struct device_node *dev, int index)
{
	struct resource res;

	if (of_address_to_resource(dev, index, &res))
		return 0;
	return res.start;
}

static struct platform_device *of_get_platform_device(const char *comp_str)
{
	struct device_node *dev_node = NULL;
	struct platform_device *plat_dev = NULL;

	dev_node = of_find_compatible_node(NULL, NULL, comp_str);

	if (dev_node == NULL) {
		pr_err("%s: of_find_compatible_node(%s) fail\n",
		__func__, comp_str);
		return NULL;
	}
	pr_info("%s: of_find_compatible_node(%s) success !!\n",	__func__,
		comp_str);

	plat_dev = of_find_device_by_node(dev_node);
	if (!plat_dev)
		pr_err("%s: %s plat_dev NULL", __func__, comp_str);

	return plat_dev;
}

static void isp_wr32_mask(void *reg, uint32_t mask, uint32_t shift,
			uint32_t value)
{
	uint32_t val;

	pr_debug("%s: writing reg=%p mask=%x shift=%x value=%x\n",
		__func__, reg, mask, shift, value);

	val = ISP_RD32(reg);
	val &= ~mask;
	val |= (value << shift);
	ISP_WR32(reg, val);
}

#ifdef DEBUG_ISP_MMCLK
static unsigned long g_scpsys_baseaddr;
static unsigned long g_ddrphy_baseaddr;

static unsigned long of_get_nodeVA(const char *comp_str)
{
	struct platform_device *plat_dev = NULL;
	unsigned long ret_baseaddr = 0;

	plat_dev = of_get_platform_device(comp_str);

	if (plat_dev) {
		ret_baseaddr = (unsigned long)of_iomap(plat_dev->dev.of_node,
				0);
		pr_err("%s: (%s)ret_baseaddr PA=0x%lx, VA = 0x%lx\n",
			__func__, comp_str,
			of_get_phys_addr(plat_dev->dev.of_node, 0),
			ret_baseaddr);
	}
	return ret_baseaddr;
}

void register_dump(void)
{
	int i;

	pr_info("%s: PH_CNT=%p READ=0x%x, SEN_Ck=%p READ=0x%x\n",
		__func__, (void *)SENINF_TG1_PH_CNT,
		ISP_RD32(SENINF_TG1_PH_CNT), (void *)SENINF_TG1_SEN_CK,
		ISP_RD32(SENINF_TG1_SEN_CK));

	pr_info("%s: [MTCMOS CHECK]===> [0x1000660c]=0x%x ,[0x10006610]=0x%x\n",
		__func__, ISP_RD32(g_scpsys_baseaddr+0x60c),
		ISP_RD32(g_scpsys_baseaddr+0x610));

	if ((ISP_RD32(g_scpsys_baseaddr+0x60c)&0x28) != 0x28)
		pr_err("MTCMOS: DISP-Bit3(%d), ISP-Bit5(%d) is not opened !!\n",
			(ISP_RD32(g_scpsys_baseaddr+0x60c)&0x08),
			(ISP_RD32(g_scpsys_baseaddr+0x60c)&0x20));

	pr_info("%s: [VENCPLL CHECK]===> [0x1000F800]=0x%x [0x1000F80c]=0x%x\n",
		__func__, ISP_RD32(g_ddrphy_baseaddr+0x800),
		ISP_RD32(g_ddrphy_baseaddr+0x80c));
	pr_info("%s: [IMG_CG_CON CHECK]===> [0x%lx]=0x%x\n", __func__,
		ISP_IMGSYS_BASE, ISP_RD32(ISP_IMGSYS_BASE));

	for (i = 0; i < ARRAY_SIZE(ccfclk); i++) {
		pr_info("%s: [%d] %s Freq = %lu\n", __func__, i,
			ccfclk[i].clkname, clk_get_rate(ccfclk[i].clock));
	}
}
#endif

int load_base_addr(void)
{
	int i;
	struct platform_device *plat_dev =
		of_get_platform_device("mediatek,mt8163-ispsys");

	/* iomap registers */
	for (i = 0; i < ISP_CAM_BASEADDR_NUM; i++) {
		gISPSYS_Reg[i] = (unsigned long) of_iomap(plat_dev->dev.of_node,
							i);
		if (!gISPSYS_Reg[i]) {
			pr_err("%s: Unable to ioremap registers i=%d\n",
				__func__, i);
			return -ENOMEM;
		}
		pr_info("%s: i=%d, PA(0x%lx) map_VA=0x%lx\n", __func__, i,
			of_get_phys_addr(plat_dev->dev.of_node, i),
			gISPSYS_Reg[i]);
	}
#ifdef DEBUG_ISP_MMCLK
	g_scpsys_baseaddr = of_get_nodeVA("mediatek,mt8163-scpsys");
	g_ddrphy_baseaddr = of_get_nodeVA("mediatek,mt8163-ddrphy");
#endif
	return 0;
}

int mclk_enable_reg(int enable)
{
	uint32_t val;
	int ret;

	/* Load register address first */
	if (ISP_ADDR == 0) {
		ret = load_base_addr();
		if (ret)
			return ret;
	}

	val = ISP_RD32((ISP_ADDR + 0x4200));
	pr_info("%s: reg=%p curr=%x en=%d\n", __func__,
		(void *)(ISP_ADDR + 0x4200), val, enable);
	if (enable)
		val |= 0x20000000;
	else
		val &= 0xDFFFFFFF;
	ISP_WR32((ISP_ADDR + 0x4200), val);

	return 0;
}

int mclk_divider_reg(uint64_t freq)
{
	uint32_t clkcnt, clkf_pol, clkf_edge;
	uint32_t clkr_edge = 0, clk_pol = 0, pad_clkinv = 0;
	int ret;

	/* Load register address first */
	if (ISP_ADDR == 0) {
		ret = load_base_addr();
		if (ret)
			return ret;
	}

	/* TG1_SEN_CK.CLKCNT = 48M/9.6M -1 = 4 */
	clkcnt = (uint32_t) ((PLL_FREQ_48_MHZ/freq) - 1);

	clkf_pol = !(clkcnt & 0x1);
	clkf_edge = clkcnt > 1 ? ((clkcnt + 1) >> 1) : 1;

	pr_info("%s: clk divider cnt=%u clkf_pol=%u clkf_edge=%u\n",
		__func__, clkcnt, clkf_pol, clkf_edge);

	/* Set PCEN */
	isp_wr32_mask(SENINF_TG1_PH_CNT, SENINF_TG1_PCEN_MASK,
			SENINF_TG1_PCEN_SHIFT, 1);

	/* Clear top clock gating in SENINF_TOP_CTRL, the case where pcen = 1*/
	isp_wr32_mask(SENINF_TOP_CTRL, SENINF12_PCLK_MASK, 0,
			SENINF12_PCLK_VAL);

	/* Setup SENINF_TG1_SEN_CK  */
	isp_wr32_mask(SENINF_TG1_SEN_CK, SENINF_TG1_CLKFL_MASK,
			SENINF_TG1_CLKFL_SHIFT, clkf_edge);

	isp_wr32_mask(SENINF_TG1_SEN_CK, SENINF_TG1_CLKRS_MASK,
			SENINF_TG1_CLKRS_SHIFT, clkr_edge);

	/* Clkcnt value */
	isp_wr32_mask(SENINF_TG1_SEN_CK, SENINF_TG1_CLKCNT_MASK,
			SENINF_TG1_CLKCNT_SHIFT, clkcnt);

	/* Setup SENINF_TG1_PH_CNT  */
	/* TGCLK_SEL = 1 */
	isp_wr32_mask(SENINF_TG1_PH_CNT, SENINF_TG1_TGCLK_SEL_MASK,
			SENINF_TG1_TGCLK_SEL_SHIFT, 1);

	isp_wr32_mask(SENINF_TG1_PH_CNT, SENINF_TG1_CLKFL_POL_MASK,
			SENINF_TG1_CLKFL_POL_SHIFT, clkf_pol);

	isp_wr32_mask(SENINF_TG1_PH_CNT, SENINF_TG1_PADCLK_INV_MASK,
			SENINF_TG1_PADCLK_INV_SHIFT, pad_clkinv);

	isp_wr32_mask(SENINF_TG1_PH_CNT, SENINF_TG1_CLK_POL_MASK,
			SENINF_TG1_CLK_POL_SHIFT, clk_pol);

	/* Set SENINF1_MUX_EN */
	isp_wr32_mask(SENINF1_MUX_CTRL, SENINF1_MUX_EN_MASK,
			SENINF1_MUX_EN_SHIFT, 1);
	isp_wr32_mask(SENINF1_CTRL, SENINF1_EN_MASK, SENINF1_EN_SHIFT, 1);

	return 0;
}

int isp_clk_init(void)
{
	int i;
	struct platform_device *plat_dev =
		of_get_platform_device("mediatek,mt8163-ispsys");

	if (!plat_dev)
		return -1;

	for (i = 0; i < ARRAY_SIZE(ispclk); i++) {
		ispclk[i].clock = devm_clk_get(&plat_dev->dev,
			ispclk[i].clkname);
		if (IS_ERR(ispclk[i].clock)) {
			pr_err("%s: cannot get %s clock\n", __func__,
				ispclk[i].clkname);
			return PTR_ERR(ispclk[i].clock);
		}
	}
	return 0;
}

int isp_clk_enable(int enable)
{
	int ret, i;

	pr_info("%s: enable=%d\n", __func__, enable);
	if (enable) {
		/* Power on ISP clock */
#ifndef CONFIG_MTK_SMI_VARIANT
		/*
			pm_runtime_get_sync return val:
			> 0 : already power on
			==0 : do power on success
			< 0 : do power on fail
		*/
		pr_info("ISP power/clock on by PM ==>");
		ret = pm_runtime_get_sync(g_pmdev_isp);
		if (ret < 0)
			pr_err("cannot pm_runtime_get_sync(ISP)\n");
#else
		ret = mtk_smi_larb_clock_on(LARB_COUNT, true);
		if (ret != 0) {
			pr_err("%s: mtk_smi_larb_clock_on(%d, true) failed, Err =% d\n",
			__func__, LARB_COUNT, ret);
			return ret;
		}
#endif
		for (i = 0; i < ARRAY_SIZE(ispclk); i++) {
			ret = clk_prepare_enable(ispclk[i].clock);
			if (ret)
				pr_err("%s: cannot prepare_enable %s clock\n",
				__func__, ispclk[i].clkname);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(ispclk); i++)
			clk_disable_unprepare(ispclk[i].clock);
		/* Power off ISP clock */
#ifndef CONFIG_MTK_SMI_VARIANT
		pm_runtime_put_sync(g_pmdev_isp);
#else
		mtk_smi_larb_clock_off(LARB_COUNT, true);
#endif
	}
	return 0;
}

int ccf_clk_init(void)
{
	int i;
	struct platform_device *plat_dev =
		of_get_platform_device("mediatek,mt8163-camera_hw");

	if (!plat_dev)
		return -1;

	for (i = 0; i < ARRAY_SIZE(ccfclk); i++) {
		ccfclk[i].clock = devm_clk_get(&plat_dev->dev,
					ccfclk[i].clkname);
		if (IS_ERR(ccfclk[i].clock)) {
			pr_err("%s: CCF cannot get %s clock\n", __func__,
				ccfclk[i].clkname);
			return PTR_ERR(ccfclk[i].clock);
		}
	}

	return 0;
}

int ccf_clk_enable(int enable)
{
	int ret;

	pr_info("%s: enable=%d\n", __func__, enable);
	if (enable) {
		ret = clk_prepare_enable(ccfclk[CLK_CHILD].clock);
		if (ret) {
			pr_err("%s: cannot prepare_enable %s clock\n",
			__func__, ccfclk[CLK_CHILD].clkname);
			return ret;
		}

		/* Setup the clock parent for 26MHz parent */
		ret = clk_set_parent(ccfclk[CLK_CHILD].clock,
				ccfclk[CLK_PARENT].clock);
		if (ret) {
			pr_err("%s: Failed to set parent, Error=%d\n",
			__func__, ret);
			return ret;
		}
	} else
		clk_disable_unprepare(ccfclk[CLK_CHILD].clock);

	return 0;
}

#ifndef CONFIG_MTK_SMI_VARIANT

/* Attach another pm_domain driver */
static int isp_pm_probe(struct platform_device *pdev)
{
	/* save disp power domain device */
	g_pmdev_isp = &pdev->dev;

	if (g_pmdev_isp->pm_domain == NULL)
		pr_err("g_pmdev_isp->pm_domain is NULL");

	pr_info("pm_runtime_enable(ISP)");
	pm_runtime_enable(g_pmdev_isp);

	pr_info("pm_runtime_get_sync(ISP)");
	pm_runtime_get_sync(g_pmdev_isp);
	return 0;
}

static int isp_pm_remove(struct platform_device *pdev)
{
	pr_info("pm_runtime_disable(ISP)");
	pm_runtime_disable(g_pmdev_isp);
	return 0;
}

static const struct of_device_id isp_pm_id_table[] = {
	{ .compatible = "mediatek,mt8163-ispsys",},
	{ }
};

MODULE_DEVICE_TABLE(of, isp_pm_id_table);

static struct platform_driver isp_pm_driver = {
	.probe		= isp_pm_probe,
	.remove		= isp_pm_remove,
	.driver		= {
	.name	= "isp_pm_driver",
	.owner	= THIS_MODULE,
	.of_match_table = isp_pm_id_table,
	},
};
module_platform_driver(isp_pm_driver);


/*
We need to power on(get_sync) ISP MTCMOS in kernel init stage,
and power off(put_sync) in late_init to make sure ISP HW engine is workable
Otherwise, ISP reg would be UN-accessed, even its power/clock is on.
*/
int __init ISP_lateinit(void)
{
	pr_err("pm_runtime_put_sync(ISP)");
	pm_runtime_put_sync(g_pmdev_isp);
	return 0;
}
late_initcall(ISP_lateinit);

#endif
