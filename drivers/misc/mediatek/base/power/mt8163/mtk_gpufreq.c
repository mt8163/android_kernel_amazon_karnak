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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/clk.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#include "mtk_gpufreq.h"
//#include "mt_cpufreq.h"
//#include "mt_static_power.h"

#include "mt-plat/sync_write.h"
#include "mt-plat/mtk_devinfo.h"
//#include "mt-plat/upmu_common.h"
//#include "mach/upmu_sw.h"
//#include "mach/upmu_hw.h"
#include "mach/mt_thermal.h"
#include "mt_freqhopping_drv.h"

/* Operate Point Definition */
#define GPUOP(khz, volt)  \
{                         \
	.gpufreq_khz = khz,     \
	.gpufreq_volt = volt,   \
}

/* GPU DVFS OPP table setting */
#define GPU_DVFS_FREQT     (598000)	/* KHz */
#define GPU_DVFS_FREQ0     (520000)	/* KHz */
#define GPU_DVFS_FREQ1     (416000)	/* KHz */
#define GPU_DVFS_FREQ2     (299000)	/* KHz */
#define GPUFREQ_LAST_FREQ_LEVEL	(GPU_DVFS_FREQ2)

#define GPU_DVFS_VOLT0     (125000)	/* mV x 100 */
#define GPU_DVFS_VOLT1     (115000)	/* mV x 100 */
#define GPU_DVFS_VOLT2     (111000)	/* mV x 100 */
#define GPU_DVFS_VOLT3     (105000)	/* mV x 100 */

/* efuse */
#define GPUFREQ_EFUSE_INDEX      (4)
#define GPU_DEFAULT_MAX_FREQ_MHZ GPU_DVFS_FREQ0
#define GPU_DEFAULT_TYPE         (0)

/* LOG */
#define TAG	 "[Power/gpufreq] "

#define gpufreq_err(fmt, args...)       \
	pr_err(TAG"[ERROR]"fmt, ##args)
#define gpufreq_warn(fmt, args...)      \
	pr_warn(TAG"[WARNING]"fmt, ##args)
#define gpufreq_info(fmt, args...)      \
	pr_info(TAG""fmt, ##args)
#define gpufreq_dbg(fmt, args...)       \
	do {                                  \
		if (mt_gpufreq_debug)               \
			pr_info(TAG""fmt, ##args);        \
	} while (0)
#define gpufreq_ver(fmt, args...)       \
	do {                                  \
		if (mt_gpufreq_debug)               \
			pr_debug(TAG""fmt, ##args);       \
	} while (0)


static sampler_func g_pFreqSampler;
static sampler_func g_pVoltSampler;

static gpufreq_power_limit_notify g_pGpufreq_power_limit_notify;

#ifdef MT_GPUFREQ_INPUT_BOOST
static gpufreq_input_boost_notify g_pGpufreq_input_boost_notify;
#endif

struct mt_gpufreq_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
};

/* enable GPU DVFS count */
static int g_gpufreq_dvfs_disable_count;

static unsigned int g_cur_gpu_OPPidx = 0xFF;

static unsigned int mt_gpufreqs_num;
static struct mt_gpufreq_table_info *mt_gpufreqs;
static struct mt_gpufreq_table_info *mt_gpufreqs_default;

/* GPU DVFS OPP Table */
/* Default */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_0[] = {
	GPUOP(GPU_DVFS_FREQ0, GPU_DVFS_VOLT1),
	GPUOP(GPU_DVFS_FREQ1, GPU_DVFS_VOLT1),
	GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1),
};

/* V/A */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_1[] = {
	GPUOP(GPU_DVFS_FREQT, GPU_DVFS_VOLT1),
	GPUOP(GPU_DVFS_FREQ1, GPU_DVFS_VOLT1),
	GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1),
};

/* V/D */
static struct mt_gpufreq_table_info mt_gpufreq_opp_tbl_2[] = {
	GPUOP(GPU_DVFS_FREQ1, GPU_DVFS_VOLT1),
	GPUOP(GPU_DVFS_FREQ2, GPU_DVFS_VOLT1),
};

static struct mt_gpufreq_table_info *gpufreq_table_list[] = {
	mt_gpufreq_opp_tbl_0,
	mt_gpufreq_opp_tbl_1,
	mt_gpufreq_opp_tbl_0,
	mt_gpufreq_opp_tbl_2,
};

static int gpufreq_table_num[] = {
	ARRAY_SIZE(mt_gpufreq_opp_tbl_0),
	ARRAY_SIZE(mt_gpufreq_opp_tbl_1),
	ARRAY_SIZE(mt_gpufreq_opp_tbl_0),
	ARRAY_SIZE(mt_gpufreq_opp_tbl_2)
};

#define GPUFREQ_GET_TABLE(type) gpufreq_table_list[type]
#define GPUFREQ_GET_TABLE_LEN(type) gpufreq_table_num[type]

static bool mt_gpufreq_ready;

static const unsigned int g_gpufreq_max_id;

static int g_last_gpu_dvs_result = 0x7F;

/* If not limited, it should be set to freq_table[0] (MAX frequency) */
static unsigned int g_limited_max_id;
static unsigned int g_limited_min_id;

static bool mt_gpufreq_debug;
static bool mt_gpufreq_pause;
static bool mt_gpufreq_keep_max_frequency_state;
static bool mt_gpufreq_keep_opp_frequency_state;
static unsigned int mt_gpufreq_keep_opp_frequency;
static unsigned int mt_gpufreq_keep_opp_index;
static bool mt_gpufreq_fixed_freq_state;
static unsigned int mt_gpufreq_fixed_frequency;

static unsigned int mt_gpufreq_volt_enable_state;
#ifdef MT_GPUFREQ_INPUT_BOOST
static unsigned int mt_gpufreq_input_boost_state = 1;
#endif

/* POWER LIMIT RELATED */
enum {
	IDX_THERMAL_LIMITED,
	IDX_LOW_BATT_VOLT_LIMITED,
	IDX_LOW_BATT_VOLUME_LIMITED,
	IDX_OC_LIMITED,

	NR_IDX_POWER_LIMITED,
};

#ifdef MT_GPUFREQ_OC_PROTECT
static bool g_limited_oc_ignore_state;
static unsigned int mt_gpufreq_oc_level;

#define MT_GPUFREQ_OC_LIMIT_FREQ_1	 GPU_DVFS_FREQ3
static unsigned int mt_gpufreq_oc_limited_index_0;
static unsigned int mt_gpufreq_oc_limited_index_1;
static unsigned int mt_gpufreq_oc_limited_index;
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
static bool g_limited_low_batt_volume_ignore_state;
static unsigned int mt_gpufreq_low_battery_volume;

#define MT_GPUFREQ_LOW_BATT_VOLUME_LIMIT_FREQ_1	 GPU_DVFS_FREQ2
static unsigned int mt_gpufreq_low_bat_volume_limited_index_0;
static unsigned int mt_gpufreq_low_bat_volume_limited_index_1;
static unsigned int mt_gpufreq_low_batt_volume_limited_index;
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
static bool g_limited_low_batt_volt_ignore_state;
static unsigned int mt_gpufreq_low_battery_level;

#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_1	 GPU_DVFS_FREQ2
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_2	 GPU_DVFS_FREQ2
static unsigned int mt_gpufreq_low_bat_volt_limited_index_0;
static unsigned int mt_gpufreq_low_bat_volt_limited_index_1;
static unsigned int mt_gpufreq_low_bat_volt_limited_index_2;
static unsigned int mt_gpufreq_low_batt_volt_limited_index;
#endif

/* static bool g_limited_power_ignore_state; */
static bool g_limited_thermal_ignore_state;
static unsigned int mt_gpufreq_thermal_limited_gpu_power;

/* limit frequency index array */
static unsigned int
mt_gpufreq_power_limited_index_array[NR_IDX_POWER_LIMITED] = { 0 };

static bool mt_gpufreq_opp_max_frequency_state;
static unsigned int mt_gpufreq_opp_max_frequency;
static unsigned int mt_gpufreq_opp_max_index;

static unsigned int mt_gpufreq_dvfs_table_type;
static unsigned int mt_gpufreq_dvfs_mmpll_spd_bond;

/* static DEFINE_SPINLOCK(mt_gpufreq_lock); */
static DEFINE_MUTEX(mt_gpufreq_lock);
static DEFINE_MUTEX(mt_gpufreq_power_lock);

static struct mt_gpufreq_power_table_info *mt_gpufreqs_power;
static struct mtk_gpu_power_info *mt_gpufreqs_power_info;
static unsigned int mt_gpufreqs_power_num;	/* run-time decided */

/* Currently MMPLL rate is not flaged with noncached,
 * use this to get 'real' rate
 */
#define GPUFREQ_MAP_APMIXEDSYS

#ifdef GPUFREQ_MAP_APMIXEDSYS
static void __iomem *ap_start;
#else
static struct clk *clk_pll;
#endif


static unsigned int _mt_gpufreq_get_dvfs_table_type(void)
{
	unsigned int type = 0;
	unsigned int gpu_speed = 0;

#ifdef CONFIG_OF
	static const struct of_device_id gpu_ids[] = {
		{ .compatible = "arm,malit6xx" },
		{ .compatible = "arm,mali-midgard" },
		{ /* sentinel */ }
	};
	struct device_node *node;

	node = of_find_matching_node(NULL, gpu_ids);
	if (!node) {
		gpufreq_err("@%s: find GPU node failed\n", __func__);
		gpu_speed = GPU_DEFAULT_MAX_FREQ_MHZ;
	} else {
		if (!of_property_read_u32(node,
			"clock-frequency", &gpu_speed)) {
			gpu_speed = gpu_speed / 1000 / 1000;
		} else {
			gpufreq_err("missing property, use default GPU level\n");
			gpu_speed = GPU_DEFAULT_MAX_FREQ_MHZ;
		}
	}
	gpufreq_info("GPU clock-frequency from DT = %d MHz\n", gpu_speed);
#endif

	mt_gpufreq_dvfs_mmpll_spd_bond =
		(get_devinfo_with_index(GPUFREQ_EFUSE_INDEX) >> 20) & 0xf;

	gpufreq_info("@%s: mmpll_spd_bond = 0x%x\n",
		__func__, mt_gpufreq_dvfs_mmpll_spd_bond);

	/* No efuse, use clock-frequency from device tree */
	if (mt_gpufreq_dvfs_mmpll_spd_bond == 0) {
		if (gpu_speed > GPU_DEFAULT_MAX_FREQ_MHZ)
			type = 1;	/* 600M */
		else
			type = GPU_DEFAULT_TYPE;

		return type;
	}

	switch (mt_gpufreq_dvfs_mmpll_spd_bond) {
	case 0x4:
		type = 1;
		break;
	case 0x5:
		type = 2;
		break;
	case 0x7:
		type = 3;
		break;
	default:
		type = GPU_DEFAULT_TYPE;
		break;
	}

	return type;
}

#ifdef MT_GPUFREQ_INPUT_BOOST
static struct task_struct *mt_gpufreq_up_task;

static int _mt_gpufreq_input_boost_task(void *data)
{
	while (1) {
		gpufreq_dbg("@%s: begin\n", __func__);

		if (g_pGpufreq_input_boost_notify) {
			gpufreq_dbg("g_pGpufreq_input_boost_notify\n");
			g_pGpufreq_input_boost_notify(g_gpufreq_max_id);
		}

		gpufreq_dbg("@%s: end\n", __func__);

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;
	}

	return 0;
}

/* Input Boost */
static void _mt_gpufreq_input_event(struct input_handle *handle,
	unsigned int type, unsigned int code, int value)
{
	if (mt_gpufreq_ready == false) {
		gpufreq_dbg("@%s: GPU DVFS not ready!\n", __func__);
		return;
	}

	if ((type == EV_KEY) && (code == BTN_TOUCH) && (value == 1)
		&& (mt_gpufreq_input_boost_state == 1)) {
		gpufreq_dbg("@%s: accept.\n", __func__);

		wake_up_process(mt_gpufreq_up_task);
	}
}

static int _mt_gpufreq_input_connect(struct input_handler *handler,
	struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "gpufreq_ib";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void _mt_gpufreq_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id mt_gpufreq_ids[] = {
	{.driver_info = 1},
	{},
};

static struct input_handler mt_gpufreq_input_handler = {
	.event = _mt_gpufreq_input_event,
	.connect = _mt_gpufreq_input_connect,
	.disconnect = _mt_gpufreq_input_disconnect,
	.name = "gpufreq_ib",
	.id_table = mt_gpufreq_ids,
};
#endif

#if 0
/* Convert pmic wrap register to voltage */
static unsigned int _mt_gpufreq_pmic_wrap_to_volt(unsigned int pmic_wrap_value)
{
	unsigned int volt = 0;

	volt = (pmic_wrap_value * 625) + 70000;
	gpufreq_dbg("@%s: volt = %d\n", __func__, volt);

	if (volt > 139375) {
		gpufreq_err("@%s: volt > 1.39375v!\n", __func__);
		return 139375;
	}

	return volt;
}
#endif

/* Power table calculation */
static void _mt_gpufreq_power_calculation(unsigned int idx,
	unsigned int freq, unsigned int volt, unsigned int temp)
{
#define GPU_ACT_REF_POWER       720     /* mW  */
#define GPU_ACT_REF_FREQ        450000  /* KHz */
#define GPU_ACT_REF_VOLT        115000  /* mV x 100 */

	unsigned int p_total = 0, p_dynamic = 0;
	unsigned int p_leakage = 0, ref_freq = 0, ref_volt = 0;

	p_dynamic = GPU_ACT_REF_POWER;
	ref_freq = GPU_ACT_REF_FREQ;
	ref_volt = GPU_ACT_REF_VOLT;

	p_dynamic = p_dynamic *
		((freq * 100) / ref_freq) *
		((volt*100)/ref_volt)*((volt*100)/ref_volt)/(100*100*100);

	p_total = p_dynamic + p_leakage;

	gpufreq_ver("%d: p_dynamic = %d, p_leakage = %d, p_total = %d\n",
				idx, p_dynamic, p_leakage, p_total);

	mt_gpufreqs_power[idx].gpufreq_power = p_total;
}

static void _mt_setup_gpufreqs_power_table(int num)
{
	int i, temp = 0;
	unsigned int freq, volt;

	mt_gpufreqs_power = kzalloc((num)*
		sizeof(struct mt_gpufreq_power_table_info), GFP_KERNEL);
	if (mt_gpufreqs_power == NULL)
		return;

	mt_gpufreqs_power_info = kzalloc((num)*
		sizeof(struct mtk_gpu_power_info), GFP_KERNEL);

#ifdef CONFIG_THERMAL
	temp = get_immediate_gpu_wrap() / 1000;
#else
	temp = 40;
#endif

	gpufreq_info("@%s: temp = %d\n", __func__, temp);

	if ((temp < 0) || (temp > 125)) {
		gpufreq_warn("@%s: temp < 0 or temp > 125!\n", __func__);
		temp = 65;
	}

	/* fill-in freq and volt in power table */
	for (i = 0; i < mt_gpufreqs_num; i++) {	/* freq */
		mt_gpufreqs_power[i].gpufreq_khz = mt_gpufreqs[i].gpufreq_khz;
		mt_gpufreqs_power[i].gpufreq_volt = mt_gpufreqs[i].gpufreq_volt;
	}

	for (i = 0; i < num; i++) {
		freq = mt_gpufreqs_power[i].gpufreq_khz;
		volt = mt_gpufreqs_power[i].gpufreq_volt;

		_mt_gpufreq_power_calculation(i, freq, volt, temp);

		mt_gpufreqs_power_info[i].gpufreq_khz =
			mt_gpufreqs_power[i].gpufreq_khz;
		mt_gpufreqs_power_info[i].gpufreq_power =
			mt_gpufreqs_power[i].gpufreq_power;

		gpufreq_info("mt_gpufreqs_power[%d].gpufreq_khz = %u\n",
			i, freq);
		gpufreq_info("mt_gpufreqs_power[%d].gpufreq_volt = %u\n",
			i, volt);
		gpufreq_info("mt_gpufreqs_power[%d].gpufreq_power = %u\n", i,
				 mt_gpufreqs_power[i].gpufreq_power);
	}

#ifdef CONFIG_THERMAL
	mtk_gpufreq_register(mt_gpufreqs_power_info, num);
#endif
}

/* register frequency table to gpufreq subsystem */
static int _mt_setup_gpufreqs_table(
	struct mt_gpufreq_table_info *freqs, int num)
{
	int i = 0;

	mt_gpufreqs = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	mt_gpufreqs_default = kzalloc((num) * sizeof(*freqs), GFP_KERNEL);
	if (mt_gpufreqs == NULL)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mt_gpufreqs[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mt_gpufreqs[i].gpufreq_volt = freqs[i].gpufreq_volt;

		mt_gpufreqs_default[i].gpufreq_khz = freqs[i].gpufreq_khz;
		mt_gpufreqs_default[i].gpufreq_volt = freqs[i].gpufreq_volt;

		gpufreq_info("mt_gpufreqs[%d].gpufreq_khz = %u\n",
			i, mt_gpufreqs[i].gpufreq_khz);
		gpufreq_info("mt_gpufreqs[%d].gpufreq_volt = %u\n",
			i, mt_gpufreqs[i].gpufreq_volt);
	}

	mt_gpufreqs_num = num;

	g_limited_max_id = 0;
	g_limited_min_id = mt_gpufreqs_num - 1;

	/* setup power table */
	mt_gpufreqs_power_num = num;
	_mt_setup_gpufreqs_power_table(mt_gpufreqs_power_num);

	return 0;
}

/* set GPU DVFS status */
static int _mt_gpufreq_state_set(int enabled)
{
	if (enabled) {
		if (!mt_gpufreq_pause) {
			gpufreq_dbg("gpufreq already enabled\n");
			return 0;
		}

		/* enable GPU DVFS */
		g_gpufreq_dvfs_disable_count--;
		gpufreq_dbg("enable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
				g_gpufreq_dvfs_disable_count);

		/* enable DVFS if no any module still disable it */
		if (g_gpufreq_dvfs_disable_count <= 0)
			mt_gpufreq_pause = false;
		else
			gpufreq_warn("someone still disable gpufreq, cannot enable it\n");
	} else {
		/* disable GPU DVFS */
		g_gpufreq_dvfs_disable_count++;
		gpufreq_dbg("disable GPU DVFS: g_gpufreq_dvfs_disable_count = %d\n",
				g_gpufreq_dvfs_disable_count);

		if (mt_gpufreq_pause) {
			gpufreq_dbg("gpufreq already disabled\n");
			return 0;
		}

		mt_gpufreq_pause = true;
	}

	return 0;
}

static unsigned int _mt_gpufreq_dds_calc(unsigned int khz)
{
	unsigned int dds = 0;

	dds = ((khz * 4 / 1000) * 8192) / 13;
	return dds;
}

static void _mt_gpufreq_set_cur_freq(unsigned int freq_new)
{
	unsigned int dds = _mt_gpufreq_dds_calc(freq_new);

	mt_dfs_general_pll(5, dds);

	gpufreq_dbg("@%s: freq_new = %d, dds = 0x%x\n",
		__func__, freq_new, dds);

	if (g_pFreqSampler)
		g_pFreqSampler(freq_new);
}

static unsigned int _mt_gpufreq_get_cur_freq(void)
{
#ifdef GPUFREQ_MAP_APMIXEDSYS
	unsigned int mmpll;
	unsigned int freq;
	u32 div;
	u32 n_info;

	mmpll = __raw_readl(ap_start + 0x244) & ~0x80000000;
	div = 1 << ((mmpll  & (0x7 << 24)) >> 24);
	n_info = (mmpll  & 0x1fffff) >> 14;
	freq = n_info * 26000 / div;

	pr_debug("div %d, mmpll 0x%x, n_info %d", div, mmpll, n_info);
	return freq;
#else
	if (clk_pll) {
		unsigned int freq_khz;

		freq_khz = clk_get_rate(clk_pll)/1000;
		return freq_khz;
	}

	return 0;
#endif
}

static unsigned int _mt_gpufreq_get_cur_volt(void)
{
#if 0
	return _mt_gpufreq_pmic_wrap_to_volt(upmu_get_ni_vproc_vosel());
#else
	return GPU_DVFS_VOLT1;
#endif
}

/* frequency ramp up and ramp down handler */
/***********************************************************
 * [note]
 * 1. frequency ramp up need to wait voltage settle
 * 2. frequency ramp down do not need to wait voltage settle
 ************************************************************/
static int _mt_gpufreq_set(unsigned int freq_new)
{
	_mt_gpufreq_set_cur_freq(freq_new);
	return 0;
}

/* Set frequency and voltage at driver probe function */
static void _mt_gpufreq_set_initial(void)
{
	unsigned int cur_volt = 0, cur_freq = 0;

	mutex_lock(&mt_gpufreq_lock);

	cur_volt = _mt_gpufreq_get_cur_volt();
	cur_freq = _mt_gpufreq_get_cur_freq();
	gpufreq_dbg("init volt = %dmV, init freq = %dkHz\n",
		cur_volt/100, cur_freq);

	g_cur_gpu_OPPidx = mt_gpufreqs_num - 1;
	_mt_gpufreq_set(mt_gpufreqs[g_cur_gpu_OPPidx].gpufreq_khz);

	mutex_unlock(&mt_gpufreq_lock);
}

/* frequency adjust interface for thermal protect */
static int _mt_gpufreq_power_throttle_protect(void)
{
	int ret = 0;
	int i = 0;
	unsigned int limited_index = 0;

	/* Check lowest frequency in all limitation */
	for (i = 0; i < NR_IDX_POWER_LIMITED; i++) {
		if (mt_gpufreq_power_limited_index_array[i] != 0 &&
			limited_index == 0)
			limited_index = mt_gpufreq_power_limited_index_array[i];
		else if (mt_gpufreq_power_limited_index_array[i] != 0 &&
			limited_index != 0) {
			if (mt_gpufreq_power_limited_index_array[i] >
				limited_index)
				limited_index =
					mt_gpufreq_power_limited_index_array[i];
		}

		gpufreq_dbg("mt_gpufreq_power_limited_index_array[%d] = %d\n",
			i, mt_gpufreq_power_limited_index_array[i]);
	}

	g_limited_max_id = limited_index;
	gpufreq_dbg("Final limit frequency upper bound to id = %d, frequency = %d\n",
		g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);

	if (g_pGpufreq_power_limit_notify)
		g_pGpufreq_power_limit_notify(g_limited_max_id);

	return ret;
}

#ifdef MT_GPUFREQ_OC_PROTECT
/* GPU frequency adjust interface for oc protect */
static void _mt_gpufreq_oc_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_dbg("@%s: limited_index = %d\n", __func__, limited_index);

	mt_gpufreq_power_limited_index_array[IDX_OC_LIMITED] = limited_index;
	_mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}

void mt_gpufreq_oc_callback(BATTERY_OC_LEVEL oc_level)
{
	gpufreq_dbg("@%s: oc_level = %d\n", __func__, oc_level);

	if (mt_gpufreq_ready == false) {
		gpufreq_dbg("@%s: GPU DVFS not ready!\n", __func__);
		return;
	}

	if (g_limited_oc_ignore_state == true) {
		gpufreq_info("@%s: g_limited_oc_ignore_state == true!\n",
			__func__);
		return;
	}

	mt_gpufreq_oc_level = oc_level;

	/* BATTERY_OC_LEVEL_1: >= 7A, BATTERY_OC_LEVEL_0: < 7A */
	if (oc_level == BATTERY_OC_LEVEL_1) {
		if (mt_gpufreq_oc_limited_index !=
			mt_gpufreq_oc_limited_index_1) {
			mt_gpufreq_oc_limited_index =
				mt_gpufreq_oc_limited_index_1;
			_mt_gpufreq_oc_protect(mt_gpufreq_oc_limited_index_1);
		}
	} else {
		if (mt_gpufreq_oc_limited_index !=
			mt_gpufreq_oc_limited_index_0) {
			mt_gpufreq_oc_limited_index =
				mt_gpufreq_oc_limited_index_0;
			_mt_gpufreq_oc_protect(mt_gpufreq_oc_limited_index_0);
		}
	}
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
/* GPU frequency adjust interface for low bat_volume protect */
static void _mt_gpufreq_low_batt_volume_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_dbg("@%s: limited_index = %d\n", __func__, limited_index);

mt_gpufreq_power_limited_index_array[IDX_LOW_BATT_VOLUME_LIMITED] =
		limited_index;
	_mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}

void mt_gpufreq_low_batt_volume_callback(
	BATTERY_PERCENT_LEVEL low_battery_volume)
{
	gpufreq_dbg("@%s: low_battery_volume = %d\n",
	__func__, low_battery_volume);

	if (mt_gpufreq_ready == false) {
		gpufreq_dbg("@%s: GPU DVFS not ready!\n", __func__);
		return;
	}

	if (g_limited_low_batt_volume_ignore_state == true) {
		gpufreq_info("@%s: ignore_state == true!\n", __func__);
		return;
	}

	mt_gpufreq_low_battery_volume = low_battery_volume;

	/* LOW_BATTERY_VOLUME_1: <= 15%, LOW_BATTERY_VOLUME_0: >15% */
	if (low_battery_volume == BATTERY_PERCENT_LEVEL_1) {
		if (mt_gpufreq_low_batt_volume_limited_index !=
			mt_gpufreq_low_bat_volume_limited_index_1) {
			mt_gpufreq_low_batt_volume_limited_index =
				mt_gpufreq_low_bat_volume_limited_index_1;
			_mt_gpufreq_low_batt_volume_protect(
				mt_gpufreq_low_bat_volume_limited_index_1);
		}
	} else {
		if (mt_gpufreq_low_batt_volume_limited_index !=
			mt_gpufreq_low_bat_volume_limited_index_0) {
			mt_gpufreq_low_batt_volume_limited_index =
				mt_gpufreq_low_bat_volume_limited_index_0;
			_mt_gpufreq_low_batt_volume_protect(
				mt_gpufreq_low_bat_volume_limited_index_0);
		}
	}
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/* GPU frequency adjust interface for low bat_volt protect */
static void _mt_gpufreq_low_batt_volt_protect(unsigned int limited_index)
{
	mutex_lock(&mt_gpufreq_power_lock);

	gpufreq_dbg("@%s: limited_index = %d\n", __func__, limited_index);
	mt_gpufreq_power_limited_index_array[IDX_LOW_BATT_VOLT_LIMITED] =
		limited_index;
	_mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}

void mt_gpufreq_low_batt_volt_callback(LOW_BATTERY_LEVEL low_battery_level)
{
	gpufreq_dbg("@%s: low_battery_level = %d\n",
		__func__, low_battery_level);

	if (mt_gpufreq_ready == false) {
		gpufreq_dbg("@%s: GPU DVFS not ready!\n", __func__);
		return;
	}

	if (g_limited_low_batt_volt_ignore_state == true) {
		gpufreq_info("@%s: g_limited_low_batt_volt_ignore_state == true!\n",
			__func__);
		return;
	}

	mt_gpufreq_low_battery_level = low_battery_level;

	if (low_battery_level == LOW_BATTERY_LEVEL_2) {
		if (mt_gpufreq_low_batt_volt_limited_index !=
			mt_gpufreq_low_bat_volt_limited_index_2) {
			mt_gpufreq_low_batt_volt_limited_index =
				mt_gpufreq_low_bat_volt_limited_index_2;
			_mt_gpufreq_low_batt_volt_protect(
				mt_gpufreq_low_bat_volt_limited_index_2);
		}
	} else {
		if (mt_gpufreq_low_batt_volt_limited_index !=
			mt_gpufreq_low_bat_volt_limited_index_0) {
			mt_gpufreq_low_batt_volt_limited_index =
				mt_gpufreq_low_bat_volt_limited_index_0;
			_mt_gpufreq_low_batt_volt_protect(
				mt_gpufreq_low_bat_volt_limited_index_0);
		}
	}
}
#endif

/* frequency adjust interface for thermal protect */
static unsigned int _mt_gpufreq_get_limited_freq(unsigned int limited_power)
{
	int i = 0;
	unsigned int limited_freq = 0;
	unsigned int found = 0;

	for (i = 0; i < mt_gpufreqs_power_num; i++) {
		if (mt_gpufreqs_power[i].gpufreq_power <= limited_power) {
			limited_freq = mt_gpufreqs_power[i].gpufreq_khz;
			found = 1;
			break;
		}
	}

	if (!found)
		limited_freq =
		mt_gpufreqs_power[mt_gpufreqs_power_num - 1].gpufreq_khz;

	gpufreq_dbg("@%s: limited_freq = %d\n", __func__, limited_freq);

	return limited_freq;
}

void mt_gpufreq_thermal_protect(unsigned int limited_power)
{
	int i = 0;
	unsigned int limited_freq = 0;

	mutex_lock(&mt_gpufreq_power_lock);

	if (mt_gpufreq_ready == false) {
		gpufreq_dbg("@%s: GPU DVFS not ready!\n", __func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (mt_gpufreqs_num == 0) {
		gpufreq_warn("@%s: mt_gpufreqs_num == 0!\n", __func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	if (g_limited_thermal_ignore_state == true) {
		gpufreq_info("@%s: g_limited_thermal_ignore_state == true!\n",
			__func__);
		mutex_unlock(&mt_gpufreq_power_lock);
		return;
	}

	mt_gpufreq_thermal_limited_gpu_power = limited_power;
	gpufreq_dbg("@%s: limited_power = %d\n", __func__, limited_power);

	if (limited_power == 0)
		mt_gpufreq_power_limited_index_array[IDX_THERMAL_LIMITED] = 0;
	else {
		limited_freq = _mt_gpufreq_get_limited_freq(limited_power);

	for (i = 0; i < mt_gpufreqs_num; i++) {
	if (mt_gpufreqs[i].gpufreq_khz <= limited_freq) {
	mt_gpufreq_power_limited_index_array[IDX_THERMAL_LIMITED] = i;
	break;
	}
	}
	}

	gpufreq_dbg("Thermal limit frequency upper bound to id = %d\n",
		mt_gpufreq_power_limited_index_array[IDX_THERMAL_LIMITED]);

	_mt_gpufreq_power_throttle_protect();

	mutex_unlock(&mt_gpufreq_power_lock);
}
EXPORT_SYMBOL(mt_gpufreq_thermal_protect);

unsigned int mt_gpufreq_get_leakage_mw(void)
{
	return 0;
}

/* return current GPU thermal limit index */
unsigned int mt_gpufreq_get_thermal_limit_index(void)
{
	gpufreq_dbg("current GPU thermal limit index is %d\n",
	g_limited_max_id);
	return g_limited_max_id;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_index);

/* return current GPU thermal limit frequency */
unsigned int mt_gpufreq_get_thermal_limit_freq(void)
{
	gpufreq_dbg("current GPU thermal limit freq is %d MHz\n",
			mt_gpufreqs[g_limited_max_id].gpufreq_khz / 1000);
	return mt_gpufreqs[g_limited_max_id].gpufreq_khz;
}
EXPORT_SYMBOL(mt_gpufreq_get_thermal_limit_freq);

/* gpufreq target callback function */
/*************************************************
 * [note]
 * 1. handle frequency change request
 * 2. call mt_gpufreq_set to set target frequency
 **************************************************/
int mt_gpufreq_target(unsigned int idx)
{
	unsigned int target_freq, target_volt, target_OPPidx;
	unsigned int cur_freq;
	int ret = 0;

	mutex_lock(&mt_gpufreq_lock);

	if (mt_gpufreq_ready == false) {
		gpufreq_dbg("GPU DVFS not ready!\n");
		mutex_unlock(&mt_gpufreq_lock);
		return -1;
	}

	if (mt_gpufreq_volt_enable_state == 0) {
		gpufreq_dbg("mt_gpufreq_volt_enable_state == 0! return\n");
		mutex_unlock(&mt_gpufreq_lock);
		return -1;
	}

	if (idx > (mt_gpufreqs_num - 1)) {
		mutex_unlock(&mt_gpufreq_lock);
		gpufreq_err("@%s: idx out of range! idx = %d\n", __func__, idx);
		return -1;
	}

	/* If /proc command fix the frequency. */
	if (mt_gpufreq_fixed_freq_state == true) {
		mutex_unlock(&mt_gpufreq_lock);
		gpufreq_info("@%s: GPU freq is fixed by user! fixed_freq = %dKHz\n",
			__func__, mt_gpufreq_fixed_frequency);
		return 0;
	}

	/* look up for the target GPU OPP */
	target_freq = mt_gpufreqs[idx].gpufreq_khz;
	target_volt = mt_gpufreqs[idx].gpufreq_volt;
	target_OPPidx = idx;

	gpufreq_dbg("@%s: begin, receive freq: %d, OPPidx: %d\n",
		__func__, target_freq, target_OPPidx);

	/* Check if need to keep max frequency */
	if (mt_gpufreq_keep_max_frequency_state == true) {
		target_freq = mt_gpufreqs[g_gpufreq_max_id].gpufreq_khz;
		target_volt = mt_gpufreqs[g_gpufreq_max_id].gpufreq_volt;
		target_OPPidx = g_gpufreq_max_id;
		gpufreq_dbg("Keep MAX frequency %d !\n", target_freq);
	}

	/* If /proc command keep opp frequency. */
	if (mt_gpufreq_keep_opp_frequency_state == true) {
		target_freq =
		mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz;
		target_volt =
		mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt;
		target_OPPidx = mt_gpufreq_keep_opp_index;
		gpufreq_dbg("Keep opp! opp frequency %d, opp voltage %d, opp idx %d\n",
			target_freq, target_volt, target_OPPidx);
	}

	/* If /proc command keep opp max frequency. */
	if (mt_gpufreq_opp_max_frequency_state == true) {
		if (target_freq > mt_gpufreq_opp_max_frequency) {
			target_freq =
			mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz;
			target_volt =
			mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_volt;
			target_OPPidx = mt_gpufreq_opp_max_index;

			gpufreq_dbg("opp max freq! opp max frequency %d, opp max voltage %d, opp max idx %d\n",
				target_freq, target_volt, target_OPPidx);
		}
	}

	/* Thermal/Power limit */
	if (g_limited_max_id != 0) {
		if (target_freq > mt_gpufreqs[g_limited_max_id].gpufreq_khz) {
			/* target_freq > limited_freq, need to adjust */
			target_freq =
				mt_gpufreqs[g_limited_max_id].gpufreq_khz;
			target_volt =
				mt_gpufreqs[g_limited_max_id].gpufreq_volt;
			target_OPPidx = g_limited_max_id;
			gpufreq_dbg("Limit! Thermal/Power limit gpu frequency %d\n",
				mt_gpufreqs[g_limited_max_id].gpufreq_khz);
		}
	}

	/* target frequency == current frequency, skip it */
	cur_freq = _mt_gpufreq_get_cur_freq();
	if (cur_freq == target_freq) {
		mutex_unlock(&mt_gpufreq_lock);
		gpufreq_dbg("GPU frequency from %d KHz to %d KHz (skipped) due to same frequency\n",
			cur_freq, target_freq);
		return 0;
	}

	gpufreq_dbg("GPU current frequency %d KHz, target frequency %d KHz\n",
		cur_freq, target_freq);

	/* set to the target frequency */
	ret = _mt_gpufreq_set(target_freq);
	if (!ret)
		g_cur_gpu_OPPidx = target_OPPidx;

	mutex_unlock(&mt_gpufreq_lock);

	return ret;
}
EXPORT_SYMBOL(mt_gpufreq_target);

/* Set VGPU enable/disable when GPU clock be switched on/off. */
int mt_gpufreq_voltage_enable_set(unsigned int enable)
{
	int ret = 0;

	gpufreq_dbg("@%s: enable = %x\n", __func__, enable);

	mutex_lock(&mt_gpufreq_lock);

	if (mt_gpufreq_ready == false) {
		gpufreq_dbg("@%s: GPU DVFS not ready!\n", __func__);
		mutex_unlock(&mt_gpufreq_lock);
		return -1;
	}

	if (enable == mt_gpufreq_volt_enable_state) {
		mutex_unlock(&mt_gpufreq_lock);
		return 0;
	}
	mt_gpufreq_volt_enable_state = enable;

	mutex_unlock(&mt_gpufreq_lock);

	return ret;
}
EXPORT_SYMBOL(mt_gpufreq_voltage_enable_set);

unsigned int mt_gpufreq_get_dvfs_table_num(void)
{
	return mt_gpufreqs_num;
}
EXPORT_SYMBOL(mt_gpufreq_get_dvfs_table_num);

unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx)
{
	if (mt_gpufreq_ready == false) {
		gpufreq_dbg("@%s: GPU DVFS not ready!\n", __func__);
		return -1;
	}

	if (idx < mt_gpufreqs_num) {
		gpufreq_dbg("@%s: idx = %d, frequency= %d\n", __func__, idx,
				mt_gpufreqs[idx].gpufreq_khz);
		return mt_gpufreqs[idx].gpufreq_khz;
	}

	gpufreq_dbg("@%s: idx = %d, NOT found! return 0!\n", __func__, idx);
	return 0;
}
EXPORT_SYMBOL(mt_gpufreq_get_freq_by_idx);

/* return current GPU frequency index */
unsigned int mt_gpufreq_get_cur_freq_index(void)
{
	gpufreq_dbg("current GPU frequency OPP index is %d\n",
		g_cur_gpu_OPPidx);
	return g_cur_gpu_OPPidx;
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq_index);

/* return current GPU frequency */
unsigned int mt_gpufreq_get_cur_freq(void)
{
	return _mt_gpufreq_get_cur_freq();
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_freq);

/* return current GPU voltage */
unsigned int mt_gpufreq_get_cur_volt(void)
{
	return _mt_gpufreq_get_cur_volt();
}
EXPORT_SYMBOL(mt_gpufreq_get_cur_volt);

/* register / unregister GPU input boost notifiction CB */
void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB)
{
#ifdef MT_GPUFREQ_INPUT_BOOST
	g_pGpufreq_input_boost_notify = pCB;
#endif
}
EXPORT_SYMBOL(mt_gpufreq_input_boost_notify_registerCB);

/* register / unregister GPU power limit notifiction CB */
void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB)
{
	g_pGpufreq_power_limit_notify = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_power_limit_notify_registerCB);

/* register / unregister set GPU freq CB */
void mt_gpufreq_setfreq_registerCB(sampler_func pCB)
{
	g_pFreqSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setfreq_registerCB);

/* register / unregister set GPU volt CB */
void mt_gpufreq_setvolt_registerCB(sampler_func pCB)
{
	g_pVoltSampler = pCB;
}
EXPORT_SYMBOL(mt_gpufreq_setvolt_registerCB);

static int _mt_gpufreq_pm_restore_early(struct device *dev)
{
	int i = 0;
	int found = 0;
	unsigned int cur_freq = _mt_gpufreq_get_cur_freq();

	for (i = 0; i < mt_gpufreqs_num; i++) {
		if (cur_freq == mt_gpufreqs[i].gpufreq_khz) {
			g_cur_gpu_OPPidx = i;
			found = 1;
			gpufreq_dbg("match g_cur_gpu_OPPidx: %d\n",
				g_cur_gpu_OPPidx);
			break;
		}
	}

	if (found == 0) {
		g_cur_gpu_OPPidx = 0;
		gpufreq_err("gpu freq not found, set parameter to max freq\n");
	}

	gpufreq_dbg("GPU freq: %d\n", cur_freq);
	gpufreq_dbg("g_cur_gpu_OPPidx: %d\n", g_cur_gpu_OPPidx);

	return 0;
}

static int _mt_gpufreq_pdrv_probe(struct platform_device *pdev)
{
#ifdef MT_GPUFREQ_INPUT_BOOST
	int rc;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };
#endif
	struct device_node *node;

#ifdef GPUFREQ_MAP_APMIXEDSYS
	static const struct of_device_id ap_ids[] = {
		{.compatible = "mediatek,mt8163-apmixedsys"},
		{ /* sentinel */ }
	};
	node = of_find_matching_node(NULL, ap_ids);
	if (node)
		ap_start = of_iomap(node, 0);
#else
	static const struct of_device_id mfg_ids[] = {
		{.compatible = "mediatek,mt8163-mfg"},
		{.compatible = "mediatek,midgard-mfg"},
		{ /* sentinel */ }
	};

	node = of_find_matching_node(NULL, mfg_ids);
	if (node)
		clk_pll = of_clk_get_by_name(node, "pll");
	if (!IS_ERR_OR_NULL(clk_pll)) {
		pr_debug("found pll clk from mfg in gpufreq %p\n", clk_pll);
	} else {
		pr_err("cannot get mmpll clk from mfg\\n");
		clk_pll = NULL;
	}
#endif

	gpufreq_info("setup gpufreqs table\n");

	mt_gpufreq_dvfs_table_type = _mt_gpufreq_get_dvfs_table_type();
	_mt_setup_gpufreqs_table(GPUFREQ_GET_TABLE(mt_gpufreq_dvfs_table_type),
		GPUFREQ_GET_TABLE_LEN(mt_gpufreq_dvfs_table_type));

	mt_gpufreq_volt_enable_state = 1;

	/* setup initial frequency */
	_mt_gpufreq_set_initial();

	gpufreq_info("GPU current frequency = %dKHz\n",
		_mt_gpufreq_get_cur_freq());
	gpufreq_info("Current Vcore = %dmV\n",
		_mt_gpufreq_get_cur_volt() / 100);
	gpufreq_info("g_cur_gpu_OPPidx = %d\n",
		g_cur_gpu_OPPidx);

	mt_gpufreq_ready = true;

#ifdef MT_GPUFREQ_INPUT_BOOST
	mt_gpufreq_up_task =
		kthread_create(_mt_gpufreq_input_boost_task,
			NULL, "mt_gpufreq_input_boost_task");
	if (IS_ERR(mt_gpufreq_up_task))
		return PTR_ERR(mt_gpufreq_up_task);

	sched_setscheduler_nocheck(mt_gpufreq_up_task, SCHED_FIFO, &param);
	get_task_struct(mt_gpufreq_up_task);

	rc = input_register_handler(&mt_gpufreq_input_handler);
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
	{
		int i;

		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz ==
				MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_1) {
				mt_gpufreq_low_bat_volt_limited_index_1 = i;
				break;
			}
		}
		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz ==
				MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ_2) {
				mt_gpufreq_low_bat_volt_limited_index_2 = i;
				break;
			}
		}
		register_low_battery_notify(&mt_gpufreq_low_batt_volt_callback,
						LOW_BATTERY_PRIO_GPU);
	}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
	{
		int i;

		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz ==
				MT_GPUFREQ_LOW_BATT_VOLUME_LIMIT_FREQ_1) {
				mt_gpufreq_low_bat_volume_limited_index_1 = i;
				break;
			}
		}
		register_battery_percent_notify(
			&mt_gpufreq_low_batt_volume_callback,
			BATTERY_PERCENT_PRIO_GPU);
	}
#endif

#ifdef MT_GPUFREQ_OC_PROTECT
	{
		int i;

		for (i = 0; i < mt_gpufreqs_num; i++) {
			if (mt_gpufreqs[i].gpufreq_khz ==
				MT_GPUFREQ_OC_LIMIT_FREQ_1) {
				mt_gpufreq_oc_limited_index_1 = i;
				break;
			}
		}
		register_battery_oc_notify(&mt_gpufreq_oc_callback,
			BATTERY_OC_PRIO_GPU);
	}
#endif

	return 0;
}

/* this function should never be called */
static int _mt_gpufreq_pdrv_remove(struct platform_device *pdev)
{
#ifdef MT_GPUFREQ_INPUT_BOOST
	input_unregister_handler(&mt_gpufreq_input_handler);

	kthread_stop(mt_gpufreq_up_task);
	put_task_struct(mt_gpufreq_up_task);
#endif

#ifdef GPUFREQ_MAP_APMIXEDSYS
	if (ap_start)
		iounmap(ap_start);
#else
	if (clk_pll)
		clk_put(clk_pll);
#endif

	return 0;
}


const struct dev_pm_ops mt_gpufreq_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
	.restore_early = _mt_gpufreq_pm_restore_early,
};

struct platform_device mt_gpufreq_pdev = {
	.name = "mt-gpufreq",
	.id = -1,
};

static struct platform_driver mt_gpufreq_pdrv = {
	.probe = _mt_gpufreq_pdrv_probe,
	.remove = _mt_gpufreq_pdrv_remove,
	.driver = {
		.name = "mt-gpufreq",
		.pm = &mt_gpufreq_pm_ops,
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_PROC_FS
/* show current debug status */
static int mt_gpufreq_debug_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_debug)
		seq_puts(m, "gpufreq debug enabled\n");
	else
		seq_puts(m, "gpufreq debug disabled\n");

	return 0;
}

/* enable debug message */
static ssize_t mt_gpufreq_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &debug)) {
		if (debug == 0)
			mt_gpufreq_debug = 0;
		else if (debug == 1)
			mt_gpufreq_debug = 1;
		else
			gpufreq_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	} else
		gpufreq_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");

	return count;
}

#ifdef MT_GPUFREQ_OC_PROTECT
/* show current limited by low batt volume */
static int mt_gpufreq_limited_oc_ignore_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_oc_ignore_state = %d\n",
		g_limited_max_id, g_limited_oc_ignore_state);

	return 0;
}

/* limited for low batt volume protect */
static ssize_t mt_gpufreq_limited_oc_ignore_proc_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtouint(desc, 10, &ignore)) {
		if (ignore == 1)
			g_limited_oc_ignore_state = true;
		else if (ignore == 0)
			g_limited_oc_ignore_state = false;
		else
			gpufreq_warn
				("bad argument!! should be 0 or 1\n");
	} else
		gpufreq_warn("bad argument!! should be 0 or 1\n");

	return count;
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
/* show current limited by low batt volume */
static int mt_gpufreq_limited_low_batt_volume_ignore_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_low_batt_volume_ignore_state = %d\n",
		   g_limited_max_id, g_limited_low_batt_volume_ignore_state);

	return 0;
}

/* limited for low batt volume protect */
static ssize_t mt_gpufreq_limited_low_batt_volume_ignore_proc_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtouint(desc, 10, &ignore)) {
		if (ignore == 1)
			g_limited_low_batt_volume_ignore_state = true;
		else if (ignore == 0)
			g_limited_low_batt_volume_ignore_state = false;
		else
			gpufreq_warn
				("bad argument!! should be 0 or 1\n");
	} else
		gpufreq_warn("bad argument!! should be 0 or 1\n");

	return count;
}
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
/* show current limited by low batt volt */
static int mt_gpufreq_limited_low_batt_volt_ignore_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_low_batt_volt_ignore_state = %d\n",
		   g_limited_max_id, g_limited_low_batt_volt_ignore_state);

	return 0;
}

/* limited for low batt volt protect */
static ssize_t mt_gpufreq_limited_low_batt_volt_ignore_proc_write(
	struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtouint(desc, 10, &ignore)) {
		if (ignore == 1)
			g_limited_low_batt_volt_ignore_state = true;
		else if (ignore == 0)
			g_limited_low_batt_volt_ignore_state = false;
		else
			gpufreq_warn
				("bad argument!! should be 0 or 1\n");
	} else
		gpufreq_warn("bad argument!! should be 0 or 1\n");

	return count;
}
#endif

/* show current limited by thermal */
static int mt_gpufreq_limited_thermal_ignore_proc_show(
	struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, g_limited_thermal_ignore_state = %d\n",
		   g_limited_max_id, g_limited_thermal_ignore_state);

	return 0;
}

/* limited for thermal protect */
static ssize_t mt_gpufreq_limited_thermal_ignore_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int ignore = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtouint(desc, 10, &ignore)) {
		if (ignore == 1)
			g_limited_thermal_ignore_state = true;
		else if (ignore == 0)
			g_limited_thermal_ignore_state = false;
		else
			gpufreq_warn
				("bad argument!! should be 0 or 1\n");
	} else
		gpufreq_warn("bad argument!! should be 0 or 1\n");

	return count;
}

/* show current limited power */
static int mt_gpufreq_limited_power_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "g_limited_max_id = %d, limit frequency = %d\n",
		g_limited_max_id, mt_gpufreqs[g_limited_max_id].gpufreq_khz);

	return 0;
}

/* limited power for thermal protect */
static ssize_t mt_gpufreq_limited_power_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	unsigned int power = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtouint(desc, 10, &power))
		mt_gpufreq_thermal_protect(power);
	else
		gpufreq_warn("bad argument!! please provide the maximum limited power\n");

	return count;
}

/* show current GPU DVFS stauts */
static int mt_gpufreq_state_proc_show(struct seq_file *m, void *v)
{
	if (!mt_gpufreq_pause)
		seq_puts(m, "GPU DVFS enabled\n");
	else
		seq_puts(m, "GPU DVFS disabled\n");

	return 0;
}

/* set GPU DVFS stauts by sysfs interface */
static ssize_t mt_gpufreq_state_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int enabled = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &enabled)) {
		if (enabled == 1) {
			mt_gpufreq_keep_max_frequency_state = false;
			_mt_gpufreq_state_set(1);
		} else if (enabled == 0) {
			/* Keep MAX frequency when GPU DVFS disabled. */
			mt_gpufreq_keep_max_frequency_state = true;
			mt_gpufreq_voltage_enable_set(1);
			mt_gpufreq_target(g_gpufreq_max_id);
			_mt_gpufreq_state_set(0);
		} else
			gpufreq_warn("bad argument!! argument should be \"1\" or \"0\"\n");
	} else
		gpufreq_warn("bad argument!! argument should be \"1\" or \"0\"\n");

	return count;
}

/* show GPU OPP table */
static int mt_gpufreq_opp_dump_proc_show(struct seq_file *m, void *v)
{
	int i = 0;

	for (i = 0; i < mt_gpufreqs_num; i++) {
		seq_printf(m, "[%d] ", i);
		seq_printf(m, "freq = %d, ", mt_gpufreqs[i].gpufreq_khz);
		seq_printf(m, "volt = %d\n", mt_gpufreqs[i].gpufreq_volt);
	}

	return 0;
}

/* show GPU power table */
static int mt_gpufreq_power_dump_proc_show(struct seq_file *m, void *v)
{
	int i = 0;

	for (i = 0; i < mt_gpufreqs_power_num; i++) {
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_khz = %d\n", i,
			   mt_gpufreqs_power[i].gpufreq_khz);
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_volt = %d\n", i,
			   mt_gpufreqs_power[i].gpufreq_volt);
		seq_printf(m, "mt_gpufreqs_power[%d].gpufreq_power = %d\n", i,
			   mt_gpufreqs_power[i].gpufreq_power);
	}

	return 0;
}

/* show current specific frequency status */
static int mt_gpufreq_opp_freq_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_keep_opp_frequency_state) {
		seq_puts(m, "gpufreq keep opp frequency enabled\n");
		seq_printf(m, "freq = %d\n",
			mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_khz);
		seq_printf(m, "volt = %d\n",
			mt_gpufreqs[mt_gpufreq_keep_opp_index].gpufreq_volt);
	} else
		seq_puts(m, "gpufreq keep opp frequency disabled\n");

	return 0;
}

/* enable specific frequency */
static ssize_t mt_gpufreq_opp_freq_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int i = 0;
	int fixed_freq = 0;
	int found = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &fixed_freq)) {
		if (fixed_freq == 0) {
			mt_gpufreq_keep_opp_frequency_state = false;
		} else {
			for (i = 0; i < mt_gpufreqs_num; i++) {
				if (fixed_freq == mt_gpufreqs[i].gpufreq_khz) {
					mt_gpufreq_keep_opp_index = i;
					found = 1;
					break;
				}
			}

			if (found == 1) {
				mt_gpufreq_keep_opp_frequency_state = true;
				mt_gpufreq_keep_opp_frequency = fixed_freq;

				mt_gpufreq_voltage_enable_set(1);
				mt_gpufreq_target(mt_gpufreq_keep_opp_index);
			}

		}
	} else
		gpufreq_warn("bad argument!! please provide the fixed frequency\n");

	return count;
}

/* show current specific frequency status */
static int mt_gpufreq_opp_max_freq_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_opp_max_frequency_state) {
		seq_puts(m, "gpufreq opp max frequency enabled\n");
		seq_printf(m, "freq = %d\n",
			mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz);
		seq_printf(m, "volt = %d\n",
			mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_volt);
	} else
		seq_puts(m, "gpufreq opp max frequency disabled\n");

	return 0;
}

/* enable specific frequency */
static ssize_t mt_gpufreq_opp_max_freq_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int i = 0;
	int max_freq = 0;
	int found = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &max_freq)) {
		if (max_freq == 0) {
			mt_gpufreq_opp_max_frequency_state = false;
		} else {
			for (i = 0; i < mt_gpufreqs_num; i++) {
				if (mt_gpufreqs[i].gpufreq_khz <= max_freq) {
					mt_gpufreq_opp_max_index = i;
					found = 1;
					break;
				}
			}

			if (found == 1) {
				mt_gpufreq_opp_max_frequency_state = true;
				mt_gpufreq_opp_max_frequency =
			mt_gpufreqs[mt_gpufreq_opp_max_index].gpufreq_khz;

				mt_gpufreq_voltage_enable_set(1);
				mt_gpufreq_target(mt_gpufreq_opp_max_index);
			}
		}
	} else
		gpufreq_warn("bad argument!! please provide the maximum limited frequency\n");

	return count;
}

/* show variable dump */
static int mt_gpufreq_var_dump_proc_show(struct seq_file *m, void *v)
{
	int i = 0;

	seq_printf(m, "GPU current frequency = %dKHz\n",
		_mt_gpufreq_get_cur_freq());
	seq_printf(m, "Current Vcore = %dmV\n",
		_mt_gpufreq_get_cur_volt() / 100);
	seq_printf(m, "g_cur_gpu_OPPidx = %d\n",
		g_cur_gpu_OPPidx);
	seq_printf(m, "g_last_gpu_dvs_result = %d (0:success, 127:not enabled, else:error)\n",
		   g_last_gpu_dvs_result);

	seq_printf(m, "g_limited_max_id = %d\n", g_limited_max_id);

	for (i = 0; i < NR_IDX_POWER_LIMITED; i++)
		seq_printf(m, "mt_gpufreq_power_limited_index_array[%d] = %d\n",
			i, mt_gpufreq_power_limited_index_array[i]);

	seq_printf(m, "mt_gpufreq_volt_enable_state = %d\n",
		mt_gpufreq_volt_enable_state);
	seq_printf(m, "mt_gpufreq_fixed_freq_state = %d\n",
		mt_gpufreq_fixed_freq_state);
	seq_printf(m, "mt_gpufreq_dvfs_table_type = %d\n",
		mt_gpufreq_dvfs_table_type);
	seq_printf(m, "mt_gpufreq_dvfs_mmpll_spd_bond = %d\n",
		mt_gpufreq_dvfs_mmpll_spd_bond);

	return 0;
}

/* show current specific frequency status */
static int mt_gpufreq_fixed_freq_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_fixed_freq_state) {
		seq_puts(m, "gpufreq fixed frequency enabled\n");
		seq_printf(m, "fixed frequency = %d\n",
			mt_gpufreq_fixed_frequency);
	} else
		seq_puts(m, "gpufreq fixed frequency disabled\n");

	return 0;
}

/* enable specific frequency */
static ssize_t mt_gpufreq_fixed_freq_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int fixed_freq = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &fixed_freq)) {
		mutex_lock(&mt_gpufreq_lock);
		if (fixed_freq == 0) {
			mt_gpufreq_fixed_freq_state = false;
			mt_gpufreq_fixed_frequency = 0;
		} else {
			_mt_gpufreq_set_cur_freq(fixed_freq);
			mt_gpufreq_fixed_freq_state = true;
			mt_gpufreq_fixed_frequency = fixed_freq;
			g_cur_gpu_OPPidx = 0;	/* keep Max Vcore */
		}
		mutex_unlock(&mt_gpufreq_lock);
	} else
		gpufreq_warn("bad argument!! should be [enable fixed_freq(KHz)]\n");

	return count;
}

#ifdef MT_GPUFREQ_INPUT_BOOST
/* show current input boost status */
static int mt_gpufreq_input_boost_proc_show(struct seq_file *m, void *v)
{
	if (mt_gpufreq_input_boost_state == 1)
		seq_puts(m, "gpufreq input boost enabled\n");
	else
		seq_puts(m, "gpufreq input boost disabled\n");

	return 0;
}

/* enable/disable input boost */
static ssize_t mt_gpufreq_input_boost_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;

	int debug = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (!kstrtoint(desc, 10, &debug)) {
		if (debug == 0)
			mt_gpufreq_input_boost_state = 0;
		else if (debug == 1)
			mt_gpufreq_input_boost_state = 1;
		else
			gpufreq_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");
	} else
		gpufreq_warn("bad argument!! should be 0 or 1 [0: disable, 1: enable]\n");

	return count;
}
#endif

#define PROC_FOPS_RW(name) \
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode)); \
} \
static const struct file_operations mt_ ## name ## _proc_fops = { \
	.owner		  = THIS_MODULE, \
	.open		   = mt_ ## name ## _proc_open, \
	.read		   = seq_read, \
	.llseek		 = seq_lseek, \
	.release		= single_release, \
	.write		  = mt_ ## name ## _proc_write, \
}

#define PROC_FOPS_RO(name) \
static int mt_ ## name ## _proc_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, mt_ ## name ## _proc_show, PDE_DATA(inode)); \
} \
static const struct file_operations mt_ ## name ## _proc_fops = { \
	.owner		 = THIS_MODULE, \
	.open		   = mt_ ## name ## _proc_open, \
	.read		   = seq_read, \
	.llseek		 = seq_lseek, \
	.release	= single_release, \
}

#define PROC_ENTRY(name)	{__stringify(name), &mt_ ## name ## _proc_fops}

PROC_FOPS_RW(gpufreq_debug);
PROC_FOPS_RW(gpufreq_limited_power);
PROC_FOPS_RW(gpufreq_limited_thermal_ignore);
PROC_FOPS_RW(gpufreq_state);
PROC_FOPS_RO(gpufreq_opp_dump);
PROC_FOPS_RO(gpufreq_power_dump);
PROC_FOPS_RW(gpufreq_opp_freq);
PROC_FOPS_RW(gpufreq_opp_max_freq);
PROC_FOPS_RO(gpufreq_var_dump);
PROC_FOPS_RW(gpufreq_fixed_freq);

#ifdef MT_GPUFREQ_INPUT_BOOST
PROC_FOPS_RW(gpufreq_input_boost);
#endif

#ifdef MT_GPUFREQ_OC_PROTECT
PROC_FOPS_RW(gpufreq_limited_oc_ignore);
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
PROC_FOPS_RW(gpufreq_limited_low_batt_volume_ignore);
#endif

#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
PROC_FOPS_RW(gpufreq_limited_low_batt_volt_ignore);
#endif

static int mt_gpufreq_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(gpufreq_debug),
		PROC_ENTRY(gpufreq_limited_power),
#ifdef MT_GPUFREQ_OC_PROTECT
		PROC_ENTRY(gpufreq_limited_oc_ignore),
#endif
#ifdef MT_GPUFREQ_LOW_BATT_VOLUME_PROTECT
		PROC_ENTRY(gpufreq_limited_low_batt_volume_ignore),
#endif
#ifdef MT_GPUFREQ_LOW_BATT_VOLT_PROTECT
		PROC_ENTRY(gpufreq_limited_low_batt_volt_ignore),
#endif
		PROC_ENTRY(gpufreq_limited_thermal_ignore),
		PROC_ENTRY(gpufreq_state),
		PROC_ENTRY(gpufreq_opp_dump),
		PROC_ENTRY(gpufreq_power_dump),
		PROC_ENTRY(gpufreq_opp_freq),
		PROC_ENTRY(gpufreq_opp_max_freq),
		PROC_ENTRY(gpufreq_var_dump),
		PROC_ENTRY(gpufreq_fixed_freq),
#ifdef MT_GPUFREQ_INPUT_BOOST
		PROC_ENTRY(gpufreq_input_boost),
#endif
	};

	dir = proc_mkdir("gpufreq", NULL);

	if (!dir) {
		gpufreq_err("fail to create /proc/gpufreq @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
			(entries[i].name, 0664, dir, entries[i].fops))
			gpufreq_err("@%s: create /proc/gpufreq/%s failed\n",
					__func__, entries[i].name);
	}

	return 0;
}
#endif	/* CONFIG_PROC_FS */

/* mediatek gpufreq initialization */
static int __init _mt_gpufreq_init(void)
{
	int ret = 0;

	gpufreq_info("@%s\n", __func__);

#ifdef CONFIG_PROC_FS
	if (mt_gpufreq_create_procfs())
		goto out;

#endif

	/* register platform device/driver */
	ret = platform_device_register(&mt_gpufreq_pdev);
	if (ret) {
		gpufreq_err("fail to register gpufreq device\n");
		goto out;
	}

	ret = platform_driver_register(&mt_gpufreq_pdrv);
	if (ret) {
		gpufreq_err("fail to register gpufreq driver\n");
		platform_device_unregister(&mt_gpufreq_pdev);
	}

out:
	return ret;
}

static void __exit _mt_gpufreq_exit(void)
{
	platform_driver_unregister(&mt_gpufreq_pdrv);
	platform_device_unregister(&mt_gpufreq_pdev);
}
module_init(_mt_gpufreq_init);
module_exit(_mt_gpufreq_exit);

MODULE_DESCRIPTION("MediaTek GPU Frequency Scaling driver");
MODULE_LICENSE("GPL");
