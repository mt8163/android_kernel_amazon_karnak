#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
/* #include <asm/outercache.h> */
/* #include <asm/system.h> */
/* #include <asm/delay.h> */
#if 0
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
#include "mt_freqhopping.h"
#include <mach/emi_bwl.h> /*  */
#include <mach/mt_typedefs.h>
#include <mach/memory.h>
#include <mach/mt_sleep.h> /*  */
#include <mach/mt_dramc.h>
#include <mach/dma.h>
#include <mach/mt_dcm.h> /*  */
#include <mach/sync_write.h>
#include <mach/md32_ipi.h> /*  */
#include <mach/md32_helper.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <mt-plat/mt_io.h>
#include <mt-plat/dma.h>
#include <mt-plat/sync_write.h>
#include "mt_dramc.h"
#include <mach/emi_bwl.h>
#include <linux/clk.h>

#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
#ifdef CONFIG_MTK_DCM
#include <mach/mt_dcm.h>
#endif
#ifdef CONFIG_MTK_MD32
#include <mach/md32_ipi.h>
#endif
#if 0 /* #ifdef CONFIG_MTK_SLEEP */
#include <mach/mt_sleep.h>
#endif


void __iomem *CQDMA_BASE_ADDR;
void __iomem *DRAMCAO_BASE_ADDR;
void __iomem *DDRPHY_BASE_ADDR;
void __iomem *DRAMCNAO_BASE_ADDR;

unsigned char *dst_array_v;
unsigned char *src_array_v;
unsigned int dst_array_p;
unsigned int src_array_p;
int init_done = 0;

static DEFINE_MUTEX(dram_dfs_mutex);

#if 0
void enter_pasr_dpd_config(unsigned char segment_rank0, unsigned char segment_rank1)
{
#if 0
	/* all segments of rank1 are not reserved -> rank1 enter DPD */
	if (segment_rank1 == 0xFF)
		slp_dpd_en(1);
#endif

	slp_pasr_en(1, segment_rank0 | (segment_rank1 << 8));
}

void exit_pasr_dpd_config(void)
{
	/* slp_dpd_en(0); */
	slp_pasr_en(0, 0);
}
#endif

#define MEM_TEST_SIZE 0x2000
#define PATTERN1 0x5A5A5A5A
#define PATTERN2 0xA5A5A5A5
int Binning_DRAM_complex_mem_test(void)
{
	unsigned char *MEM8_BASE;
	unsigned short *MEM16_BASE;
	unsigned int *MEM32_BASE;
	unsigned int *MEM_BASE;
	unsigned long mem_ptr;
	unsigned char pattern8;
	unsigned short pattern16;
	unsigned int i, j, size, pattern32;
	unsigned int value;
	unsigned int len = MEM_TEST_SIZE;
	void *ptr;

	ptr = vmalloc(PAGE_SIZE*2);
	MEM8_BASE = (unsigned char *)ptr;
	MEM16_BASE = (unsigned short *)ptr;
	MEM32_BASE = (unsigned int *)ptr;
	MEM_BASE = (unsigned int *)ptr;
	pr_info("Test DRAM start address 0x%lx\n", (unsigned long)ptr);
	pr_info("Test DRAM SIZE 0x%x\n", MEM_TEST_SIZE);
	size = len >> 2;

	/* === Verify the tied bits (tied high) === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0;

	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0) {
			vfree(ptr);
			return -1;
		}
		MEM32_BASE[i] = 0xffffffff;
	}

	/* === Verify the tied bits (tied low) === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			vfree(ptr);
			return -2;
		}
		MEM32_BASE[i] = 0x00;
	}

	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = 0; i < len; i++)
		MEM8_BASE[i] = pattern8++;
	pattern8 = 0x00;
	for (i = 0; i < len; i++) {
		if (MEM8_BASE[i] != pattern8++) {
			vfree(ptr);
			return -3;
		}
	}

	/* === Verify pattern 2 (0x00~0xff) === */
	pattern8 = 0x00;
	for (i = j = 0; i < len; i += 2, j++) {
		if (MEM8_BASE[i] == pattern8)
			MEM16_BASE[j] = pattern8;
		if (MEM16_BASE[j] != pattern8) {
			vfree(ptr);
			return -4;
		}
		pattern8 += 2;
	}

	/* === Verify pattern 3 (0x00~0xffff) === */
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++)
		MEM16_BASE[i] = pattern16++;
	pattern16 = 0x00;
	for (i = 0; i < (len >> 1); i++) {
		if (MEM16_BASE[i] != pattern16++) {
			vfree(ptr);
			return -5;
		}
	}

	/* === Verify pattern 4 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++)
		MEM32_BASE[i] = pattern32++;
	pattern32 = 0x00;
	for (i = 0; i < (len >> 2); i++) {
		if (MEM32_BASE[i] != pattern32++) {
			vfree(ptr);
			return -6;
		}
	}

	/* === Pattern 5: Filling memory range with 0x44332211 === */
	for (i = 0; i < size; i++)
		MEM32_BASE[i] = 0x44332211;

	/* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x44332211) {
			vfree(ptr);
			return -7;
		}
		MEM32_BASE[i] = 0xa5a5a5a5;
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 0h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a5a5) {
			vfree(ptr);
			return -8;
		}
		MEM8_BASE[i * 4] = 0x00;
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 2h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5a5a500) {
			vfree(ptr);
			return -9;
		}
		MEM8_BASE[i * 4 + 2] = 0x00;
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 1h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa500a500) {
			vfree(ptr);
			return -10;
		}
		MEM8_BASE[i * 4 + 1] = 0x00;
	}

	/* === Read Check then Fill Memory with 00 Byte Pattern at offset 3h === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xa5000000) {
			vfree(ptr);
			return -11;
		}
		MEM8_BASE[i * 4 + 3] = 0x00;
	}

	/* === Read Check then Fill Memory with ffff Word Pattern at offset 1h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0x00000000) {
			vfree(ptr);
			return -12;
		}
		MEM16_BASE[i * 2 + 1] = 0xffff;
	}

	/* === Read Check then Fill Memory with ffff Word Pattern at offset 0h == */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffff0000) {
			vfree(ptr);
			return -13;
		}
		MEM16_BASE[i * 2] = 0xffff;
	}

	/*===  Read Check === */
	for (i = 0; i < size; i++) {
		if (MEM32_BASE[i] != 0xffffffff) {
			vfree(ptr);
			return -14;
		}
	}

	/************************************************
	* Additional verification
	************************************************/
	/* === stage 1 => write 0 === */

	for (i = 0; i < size; i++)
		MEM_BASE[i] = PATTERN1;

	/* === stage 2 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];

		if (value != PATTERN1) {
			vfree(ptr);
			return -15;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 3 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			vfree(ptr);
			return -16;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 4 => read 0, write 0xF === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			vfree(ptr);
			return -17;
		}
		MEM_BASE[i] = PATTERN2;
	}

	/* === stage 5 => read 0xF, write 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN2) {
			vfree(ptr);
			return -18;
		}
		MEM_BASE[i] = PATTERN1;
	}

	/* === stage 6 => read 0 === */
	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != PATTERN1) {
			vfree(ptr);
			return -19;
		}
	}

	/* === 1/2/4-byte combination test === */
	mem_ptr = (unsigned long)MEM_BASE;
	while (mem_ptr < ((unsigned long)MEM_BASE + (size << 2))) {
		*((unsigned char *) mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *) mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *) mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *) mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *) mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *) mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *) mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *) mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned char *) mem_ptr) = 0x78;
		mem_ptr += 1;
		*((unsigned char *) mem_ptr) = 0x56;
		mem_ptr += 1;
		*((unsigned short *) mem_ptr) = 0x1234;
		mem_ptr += 2;
		*((unsigned int *) mem_ptr) = 0x12345678;
		mem_ptr += 4;
		*((unsigned short *) mem_ptr) = 0x5678;
		mem_ptr += 2;
		*((unsigned char *) mem_ptr) = 0x34;
		mem_ptr += 1;
		*((unsigned char *) mem_ptr) = 0x12;
		mem_ptr += 1;
		*((unsigned int *) mem_ptr) = 0x12345678;
		mem_ptr += 4;
	}

	for (i = 0; i < size; i++) {
		value = MEM_BASE[i];
		if (value != 0x12345678) {
			vfree(ptr);
			return -20;
		}
	}


	/* === Verify pattern 1 (0x00~0xff) === */
	pattern8 = 0x00;
	MEM8_BASE[0] = pattern8;
	for (i = 0; i < size * 4; i++) {
		unsigned char waddr8, raddr8;

		waddr8 = i + 1;
		raddr8 = i;
		if (i < size * 4 - 1)
			MEM8_BASE[waddr8] = pattern8 + 1;
		if (MEM8_BASE[raddr8] != pattern8) {
			vfree(ptr);
			return -21;
		}
		pattern8++;
	}


	/* === Verify pattern 2 (0x00~0xffff) === */
	pattern16 = 0x00;
	MEM16_BASE[0] = pattern16;
	for (i = 0; i < size * 2; i++) {
		if (i < size * 2 - 1)
			MEM16_BASE[i + 1] = pattern16 + 1;
		if (MEM16_BASE[i] != pattern16) {
			vfree(ptr);
			return -22;
		}
		pattern16++;
	}
	/* === Verify pattern 3 (0x00~0xffffffff) === */
	pattern32 = 0x00;
	MEM32_BASE[0] = pattern32;
	for (i = 0; i < size; i++) {
		if (i < size - 1)
			MEM32_BASE[i + 1] = pattern32 + 1;
		if (MEM32_BASE[i] != pattern32) {
			vfree(ptr);
			return -23;
		}
		pattern32++;
	}
	pr_info("complex R/W mem test pass\n");
	vfree(ptr);
	return 1;
}

unsigned int ucDram_Register_Read(unsigned int u4reg_addr)
{
	unsigned int pu4reg_value;

#if 0
	pu4reg_value = (*(unsigned int *)((uintptr_t)CHA_DRAMCAO_BASE + (u4reg_addr))) |
		(*(unsigned int *)((uintptr_t)CHA_DDRPHY_BASE + (u4reg_addr))) |
		(*(unsigned int *)((uintptr_t)CHA_DRAMCNAO_BASE + (u4reg_addr)));
#else
	pu4reg_value = (readl(IOMEM(DRAMCAO_BASE_ADDR + u4reg_addr)) |
		readl(IOMEM(DDRPHY_BASE_ADDR + u4reg_addr)) |
		readl(IOMEM(DRAMCNAO_BASE_ADDR + u4reg_addr)));
#endif

	return pu4reg_value;
}

void ucDram_Register_Write(unsigned int u4reg_addr, unsigned int u4reg_value)
{
#if 0
	(*(unsigned int *)((uintptr_t)CHA_DRAMCAO_BASE + (u4reg_addr))) = u4reg_value;
	(*(unsigned int *)((uintptr_t)CHA_DDRPHY_BASE + (u4reg_addr))) = u4reg_value;
	(*(unsigned int *)((uintptr_t)CHA_DRAMCNAO_BASE + (u4reg_addr))) = u4reg_value;
#else
	writel(u4reg_value, IOMEM(DRAMCAO_BASE_ADDR + u4reg_addr));
	writel(u4reg_value, IOMEM(DDRPHY_BASE_ADDR + u4reg_addr));
	writel(u4reg_value, IOMEM(DRAMCNAO_BASE_ADDR + u4reg_addr));
#endif
	/* dsb(); */
	mb();
}

bool pasr_is_valid(void)
{
	unsigned int ddr_type = 0;

	ddr_type = get_ddr_type();
	/* Following DDR types can support PASR */
	if ((ddr_type == TYPE_LPDDR3) || (ddr_type == TYPE_LPDDR2))
		return true;
	return false;
}

struct clk *dramc_f26m_clk;

unsigned int DRAM_MRR(int MRR_num)
{
	unsigned int MRR_value = 0x0;
	unsigned int u4value;

	/* set DQ bit 0, 1, 2, 3, 4, 5, 6, 7 pinmux for LPDDR3 */
	ucDram_Register_Write(DRAMC_REG_RRRATE_CTL, 0x13121110);
	ucDram_Register_Write(DRAMC_REG_MRR_CTL, 0x17161514);

	ucDram_Register_Write(DRAMC_REG_MRS, MRR_num);
	ucDram_Register_Write(DRAMC_REG_SPCMD, 0x00000002);
	udelay(1);
	ucDram_Register_Write(DRAMC_REG_SPCMD, 0x00000000);
	udelay(1);

	u4value = ucDram_Register_Read(DRAMC_REG_SPCMDRESP);
	MRR_value = (u4value >> 20) & 0xFF;

	return MRR_value;
}


unsigned int read_dram_temperature(void)
{
	unsigned int value;

	value = DRAM_MRR(4) & 0x7;
	return value;
}

int get_ddr_type(void)
{
	unsigned int value;

	value =  ucDram_Register_Read(DRAMC_LPDDR2);
	/* check LPDDR2_EN */
	if ((value>>28) & 0x1)
		return TYPE_LPDDR2;

	value = ucDram_Register_Read(DRAMC_PADCTL4);
	/* check DDR3_EN */
	if ((value>>7) & 0x1)
		return TYPE_PCDDR3;

	value = ucDram_Register_Read(DRAMC_ACTIM1);
	/* check LPDDR3_EN */
	if ((value>>28) & 0x1)
		return TYPE_LPDDR3;

	return TYPE_DDR1;
}

#if 0
unsigned int shuffer_done;

void dram_dfs_ipi_handler(int id, void *data, unsigned int len)
{
	shuffer_done = 1;
}
#endif

#if 0
void Reg_Sync_Writel(addr, val)
{
	(*(unsigned int *)(uintptr_t)(addr)) = val;
	dsb();
}

unsigned int Reg_Readl(addr)
{
	return (*(unsigned int *)(uintptr_t)(addr));
}
#else
#define Reg_Sync_Writel(addr, val)   writel(val, IOMEM(addr))
#define Reg_Readl(addr) readl(IOMEM(addr))
#endif

static ssize_t complex_mem_test_show(struct device_driver *driver, char *buf)
{
	int ret;

	ret = Binning_DRAM_complex_mem_test();
	if (ret > 0)
		return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
	else
		return snprintf(buf, PAGE_SIZE, "MEM TEST failed %d\n", ret);
}

static ssize_t complex_mem_test_store(struct device_driver *driver, const char *buf, size_t count)
{
	return count;
}

DRIVER_ATTR(emi_clk_mem_test, 0664, complex_mem_test_show, complex_mem_test_store);


static int dramc_clk_probe(struct platform_device *pdev)
{
	dramc_f26m_clk = devm_clk_get(&pdev->dev, "dramc_f26m");
	BUG_ON(IS_ERR(dramc_f26m_clk));
	clk_prepare(dramc_f26m_clk);
	clk_enable(dramc_f26m_clk);

	return 0;
}

static struct platform_driver dram_test_drv = {
	.driver = {
		.name = "emi_clk_test",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	},
};

static const struct of_device_id dramc_clk_of_match[] = {
	{.compatible = "mediatek,mt8163-ddrphy",},
	{},
};

MODULE_DEVICE_TABLE(of, dramc_clk_of_match);

static struct platform_driver dramc_clk_drv = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = dramc_clk_probe,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "dramc_clk",
		.of_match_table = of_match_ptr(dramc_clk_of_match),
	},
};


/* extern char __ssram_text, _sram_start, __esram_text; */
int __init dram_test_init(void)
{
	int ret, err;
	struct device_node *node;

	err = platform_driver_register(&dramc_clk_drv);
	if (err)
		pr_err("fail to register dramc_f26m_drv.\n");

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-dramco");
	if (node) {
		DRAMCAO_BASE_ADDR = of_iomap(node, 0);
		/*pr_info("get DRAMCAO_BASE_ADDR @ %p\n", DRAMCAO_BASE_ADDR);*/
	} else {
		pr_warn("can't find DRAMC0 compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-ddrphy");
	if (node) {
		DDRPHY_BASE_ADDR = of_iomap(node, 0);
		/*pr_info("get DDRPHY_BASE_ADDR @ %p\n", DDRPHY_BASE_ADDR);*/
	} else {
		pr_warn("can't find DDRPHY compatible node\n");
		return -1;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8163-dramc_nao");
	if (node) {
		DRAMCNAO_BASE_ADDR = of_iomap(node, 0);
		/*pr_info("get DRAMCNAO_BASE_ADDR @ %p\n", DRAMCNAO_BASE_ADDR);*/
	} else {
		pr_warn("can't find DRAMCNAO compatible node\n");
		return -1;
	}

	ret = platform_driver_register(&dram_test_drv);
	if (ret) {
		pr_err("fail to create the dram_test driver\n");
		return ret;
	}

	ret = driver_create_file(&dram_test_drv.driver, &driver_attr_emi_clk_mem_test);
	if (ret) {
		pr_err("fail to create the emi_clk_mem_test sysfs files\n");
		return ret;
	}

	return 0;
}

module_init(dram_test_init);
