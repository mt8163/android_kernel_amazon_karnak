#ifndef _MTK_THERMAL_H_
#define _MTK_THERMAL_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/notifier.h>
#include <linux/leds.h>
#include <linux/thermal_framework.h>

struct virtual_sensor_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
};

struct mtk_cooler_platform_data {
	char type[THERMAL_NAME_LENGTH];
	unsigned long state;
	unsigned long max_state;
	struct thermal_cooling_device *cdev;
	int level;
	int levels[THERMAL_MAX_TRIPS];
};

struct cooler_sort_list{
	struct mtk_cooler_platform_data *pdata;
	struct list_head list;
};

#ifdef CONFIG_roc123
struct cdev_t {
	char type[THERMAL_NAME_LENGTH];
	unsigned long upper;
	unsigned long lower;
};
#endif

struct trip_t {
	unsigned long temp;
	enum thermal_trip_type type;
	unsigned long hyst;
#ifdef CONFIG_roc123
	struct cdev_t cdev[THERMAL_MAX_TRIPS];
#endif
};

struct mtk_thermal_platform_data {
	int num_trips;
	enum thermal_device_mode mode;
	int polling_delay;
	struct list_head ts_list;
	struct thermal_zone_params tzp;
	struct trip_t trips[THERMAL_MAX_TRIPS];
	int num_cdevs;
	char cdevs[THERMAL_MAX_TRIPS][THERMAL_NAME_LENGTH];
};

struct mtk_thermal_platform_data_wrapper {
	struct mtk_thermal_platform_data *data;
	struct thermal_dev_params params;
};

struct alt_cpu_thermal_zone {
	struct thermal_zone_device *tz;
	struct work_struct therm_work;
	struct mtk_thermal_platform_data *pdata;
};

/**
 * virtual_sensor_temp_sensor structure
 * @iclient - I2c client pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for sysfs, irq and PM
 * @therm_fw - thermal device
 */
struct virtual_sensor_temp_sensor {
        struct i2c_client *iclient;
        struct device *dev;
        struct mutex sensor_mutex;
        struct thermal_dev *therm_fw;
        u16 config_orig;
        u16 config_current;
        unsigned long last_update;
        int temp[3];
        int debug_temp;
};

extern int thermal_level_compare(struct mtk_cooler_platform_data *cooler_data, struct cooler_sort_list *head, bool positive_seq);

void last_kmsg_thermal_shutdown(void);

#endif /* _MTK_THERMAL_H_ */
