#include <linux/init.h>
#include "mt-plat/mtk_rtc.h"

static int __init mt_power_management_init(void)
{
	pm_power_off = mt_power_off;
	return 0;
}
arch_initcall(mt_power_management_init);

