#include <linux/types.h>
#include <linux/kernel.h>
#include <mt-plat/battery_common.h>


/************* ATTENTATION ***************/
/* IF ANY NEW CHARGER IC SUPPORT IN THIS FILE, */
/* REMEMBER TO NOTIFY USB OWNER TO MODIFY OTG RELATED FILES!! */

void tbl_charger_otg_vbus(int mode)
{
	unsigned int otg_enable = mode & 0xFF;
	bool chr_status;

	battery_charging_control(CHARGING_CMD_GET_CHARGER_DET_STATUS, &chr_status);
	pr_debug("[%s] mode = %d, vchr(%d)\n", __func__, mode, chr_status);

	if (chr_status && otg_enable)
		return;

	battery_charging_control(CHARGING_CMD_BOOST_ENABLE, &otg_enable);
}
