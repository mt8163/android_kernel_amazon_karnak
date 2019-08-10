/*
 * isl29034.c - Intersil ISL29034  ALS & Proximity Driver
 *
 * By Intersil Corp
 * Michael DiGioia
 *
 * Based on isl29011.c
 *	by Mike DiGioia <mdigioia@intersil.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/hwmon.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#endif
/* Insmod parameters */
/*I2C_CLIENT_INSMOD_1(isl29034);*/

#define MODULE_NAME	"isl29034"

/* ICS932S401 registers */
#define ISL29034_VENDOR                         1
#define ISL29034_REV_SHIFT                      4
#define ISL29034_REG_DEVICE                     0x44
#define ISL29034_DEVICE                         44


#define REG_CMD_1		0x00
#define REG_CMD_2		0x01
#define REG_DATA_LSB		0x02
#define REG_DATA_MSB		0x03
#define ISL_MOD_MASK		0xE0
#define ISL_MOD_POWERDOWN	0
#define ISL_MOD_ALS_ONCE	1
#define ISL_MOD_IR_ONCE		2
#define ISL_MOD_PS_ONCE		3
#define ISL_MOD_RESERVED	4
#define ISL_MOD_ALS_CONT	5
#define ISL_MOD_IR_CONT		6
#define ISL_MOD_PS_CONT		7
#define IR_CURRENT_MASK		0xC0
#define IR_FREQ_MASK		0x30
#define SENSOR_RANGE_MASK	0x03
#define ISL_RES_MASK		0x0C

static int last_mod;

struct isl_device {
	struct i2c_client *client;
	int resolution;
	int range;
};

static DEFINE_MUTEX(mutex);

static int isl_set_range(struct i2c_client *client, int range)
{
	int ret_val;

	ret_val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	if (ret_val < 0)
		return -EINVAL;
	ret_val &= ~SENSOR_RANGE_MASK;	/*reset the bit */
	ret_val |= range;
	ret_val = i2c_smbus_write_byte_data(client, REG_CMD_2, ret_val);

	printk(KERN_INFO MODULE_NAME ": %s isl29034 set_range call, \n",
			__func__);
	if (ret_val < 0)
		return ret_val;
	return range;
}

static int isl_set_mod(struct i2c_client *client, int mod)
{
	int ret, val, freq;

	switch (mod) {
	case ISL_MOD_POWERDOWN:
	case ISL_MOD_RESERVED:
		goto setmod;
	case ISL_MOD_ALS_ONCE:
	case ISL_MOD_ALS_CONT:
		freq = 0;
		break;
	case ISL_MOD_IR_ONCE:
	case ISL_MOD_IR_CONT:
	case ISL_MOD_PS_ONCE:
	case ISL_MOD_PS_CONT:
		freq = 1;
		break;
	default:
		return -EINVAL;
	}
	/* set IR frequency */
	val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	if (val < 0)
		return -EINVAL;
	val &= ~IR_FREQ_MASK;
	if (freq)
		val |= IR_FREQ_MASK;
	ret = i2c_smbus_write_byte_data(client, REG_CMD_2, val);
	if (ret < 0)
		return -EINVAL;

setmod:
	/* set operation mod */
	val = i2c_smbus_read_byte_data(client, REG_CMD_1);
	if (val < 0)
		return -EINVAL;
	val &= ~ISL_MOD_MASK;
	val |= (mod << 5);
	ret = i2c_smbus_write_byte_data(client, REG_CMD_1, val);
	if (ret < 0)
		return -EINVAL;

	if (mod != ISL_MOD_POWERDOWN)
		last_mod = mod;

	return mod;
}

static int isl_get_res(struct i2c_client *client)
{
	int val;

	printk(KERN_INFO MODULE_NAME ": %s isl29034 get_res call, \n", __func__);
    val = i2c_smbus_read_word_data(client, 0)>>8 & 0xff;

	if (val < 0)
		return -EINVAL;

	val &= ISL_RES_MASK;
	val >>= 2;

	switch (val) {
	case 0:
		return 65536;
	case 1:
		return 4096;
	case 2:
		return 256;
	case 3:
		return 16;
	default:
		return -EINVAL;
	}
}

static int isl_get_mod(struct i2c_client *client)
{
	int val;

	val = i2c_smbus_read_byte_data(client, REG_CMD_1);
	if (val < 0)
		return -EINVAL;
	return val >> 5;
}

static int isl_get_range(struct i2c_client *client)
{
	switch (i2c_smbus_read_word_data(client, 0)>>8 & 0xff & 0x3) {
	case 0: return  1000;
	case 1: return  4000;
	case 2: return 16000;
	case 3: return 64000;
	default: return -EINVAL;
	}
}

static ssize_t
isl_sensing_range_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int val;

	mutex_lock(&mutex);
	val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	mutex_unlock(&mutex);

	dev_dbg(dev, "%s: range: 0x%.2x\n", __func__, val);

	if (val < 0)
		return val;
	return sprintf(buf, "%d000\n", 1 << (2 * (val & 3)));
}

static ssize_t
ir_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int val;

	mutex_lock(&mutex);
	val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	mutex_unlock(&mutex);

	dev_dbg(dev, "%s: IR current: 0x%.2x\n", __func__, val);

	if (val < 0)
		return -EINVAL;
	val >>= 6;

	switch (val) {
	case 0:
		val = 100;
		break;
	case 1:
		val = 50;
		break;
	case 2:
		val = 25;
		break;
	case 3:
		val = 0;
		break;
	default:
		return -EINVAL;
	}

	if (val)
		val = sprintf(buf, "%d\n", val);
	else
		val = sprintf(buf, "%s\n", "12.5");
	return val;
}

static ssize_t
isl_sensing_mod_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);

	dev_dbg(dev, "%s: mod: 0x%.2x\n", __func__, last_mod);

	switch (last_mod) {
	case ISL_MOD_POWERDOWN:
		return sprintf(buf, "%s\n", "0-Power-down");
	case ISL_MOD_ALS_ONCE:
		return sprintf(buf, "%s\n", "1-ALS once");
	case ISL_MOD_IR_ONCE:
		return sprintf(buf, "%s\n", "2-IR once");
	case ISL_MOD_PS_ONCE:
		return sprintf(buf, "%s\n", "3-Proximity once");
	case ISL_MOD_RESERVED:
		return sprintf(buf, "%s\n", "4-Reserved");
	case ISL_MOD_ALS_CONT:
		return sprintf(buf, "%s\n", "5-ALS continuous");
	case ISL_MOD_IR_CONT:
		return sprintf(buf, "%s\n", "6-IR continuous");
	case ISL_MOD_PS_CONT:
		return sprintf(buf, "%s\n", "7-Proximity continuous");
	default:
		return -EINVAL;
	}
}

static ssize_t
isl_output_data_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret_val, val, mod;
	unsigned long int max_count, output = 0;
	int temp;

	mutex_lock(&mutex);

	temp = i2c_smbus_read_byte_data(client, REG_DATA_MSB);
	if (temp < 0)
		goto err_exit;
	ret_val = i2c_smbus_read_byte_data(client, REG_DATA_LSB);
	if (ret_val < 0)
		goto err_exit;
	ret_val |= temp << 8;

	dev_dbg(dev, "%s: Data: %04x\n", __func__, ret_val);

	mod = isl_get_mod(client);
	switch (last_mod) {
	case ISL_MOD_ALS_CONT:
	case ISL_MOD_ALS_ONCE:
	case ISL_MOD_IR_ONCE:
	case ISL_MOD_IR_CONT:
		output = ret_val;
		break;
	case ISL_MOD_PS_CONT:
	case ISL_MOD_PS_ONCE:
		val = i2c_smbus_read_byte_data(client, REG_CMD_2);
		if (val < 0)
			goto err_exit;
		max_count = isl_get_res(client);
		output = (((1 << (2 * (val & SENSOR_RANGE_MASK))) * 1000)
			  * ret_val) / max_count;
		break;
	default:
		goto err_exit;
	}
	mutex_unlock(&mutex);
	return sprintf(buf, "%ld\n", output);

err_exit:
	mutex_unlock(&mutex);
	return -EINVAL;
}

static int isl_get_lux_data(struct i2c_client *client)
{
	struct isl_device *idev = i2c_get_clientdata(client);

	__u16 resH, resL;
	int range;
	resL = i2c_smbus_read_word_data(client, 1)>>8;
	resH = i2c_smbus_read_word_data(client, 2)&0xff00;
	return (resH | resL) * idev->range / idev->resolution;
}
static ssize_t
isl_lux_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	__u16 resH, resL, L1, L2, H1, H2, thresL, thresH;
	char cmd2;
	int res, data, tmp, range, resolution;

	mutex_lock(&mutex);

	/*cmd2 = i2c_smbus_read_word_data(client, 0)>>8 & 0xff;  // 01h*/
	resL = i2c_smbus_read_word_data(client, 1)>>8;         /* 02h */
	resH = i2c_smbus_read_word_data(client, 2)&0xff00;     /* 03h */
	/*L1 = i2c_smbus_read_word_data(client, 3)>>8;           // 04h*/
	/*L2 = i2c_smbus_read_word_data(client, 4)&0xff00;       // 05h*/
	/*H1 = i2c_smbus_read_word_data(client, 5)>>8;           // 06h*/
	/*H2 = i2c_smbus_read_word_data(client, 6)&0xff00;       // 07h*/

	res = resH | resL;
	/*thresL = L2 | L1; */
	/*thresH = H2 | H1; */

    cmd2 = i2c_smbus_read_word_data(client, 0)>>8 & 0xff;
	resolution = isl_get_res(client);  /*resolution*/
	mutex_unlock(&mutex);
	tmp = cmd2 & 0x3;            /*range*/
	switch (tmp) {
	case 0:
		range = 1000;
		break;
	case 1:
		range = 4000;
		break;
	case 2:
		range = 16000;
		break;
	case 3:
		range = 64000;
		break;
	default:
		return -EINVAL;
	}
	data = res * range / resolution;

	/*printk("Data = 0x%04x [%d]\n", data, data);*/
	/*printk("CMD2 = 0x%x\n", cmd2);*/
	/*printk("Threshold Low  = 0x%04x\n", thresL);*/
	/*printk("Threshold High = 0x%04x\n", thresH);*/

	return sprintf(buf, "%u\n", data);
}

static ssize_t
isl_cmd2_show(struct device *dev,
		     struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned long cmd2;
	mutex_lock(&mutex);
    cmd2 = i2c_smbus_read_word_data(client, 0)>>8 & 0xff;
	mutex_unlock(&mutex);
	printk(KERN_INFO "isl29034: cmd2: 0x%.2x\n", __func__, cmd2);
	switch (cmd2) {
	case 0:
		return sprintf(buf, "%s\n", "[cmd2 = 0] n = 16, range = 1000");
	case 1:
		return sprintf(buf, "%s\n", "[cmd2 = 1] n = 16, range = 4000");
	case 2:
		return sprintf(buf, "%s\n", "[cmd2 = 2] n = 16, range = 16000");
	case 3:
		return sprintf(buf, "%s\n", "[cmd2 = 3] n = 16, range = 64000");

	case 4:
		return sprintf(buf, "%s\n", "[cmd2 = 4] n = 12, range = 1000");
	case 5:
		return sprintf(buf, "%s\n", "[cmd2 = 5] n = 12, range = 4000");
	case 6:
		return sprintf(buf, "%s\n", "[cmd2 = 6] n = 12, range = 16000");
	case 7:
		return sprintf(buf, "%s\n", "[cmd2 = 7] n = 12, range = 64000");

	case 8:
		return sprintf(buf, "%s\n", "[cmd2 = 8] n = 8, range = 1000");
	case 9:
		return sprintf(buf, "%s\n", "[cmd2 = 9] n = 8, range = 4000");
	case 10:
		return sprintf(buf, "%s\n", "[cmd2 = 10] n = 8, range = 16000");
	case 11:
		return sprintf(buf, "%s\n", "[cmd2 = 11] n = 8, range = 64000");

	case 12:
		return sprintf(buf, "%s\n", "[cmd2 = 12] n = 4, range = 1000");
	case 13:
		return sprintf(buf, "%s\n", "[cmd2 = 13] n = 4, range = 4000");
	case 14:
		return sprintf(buf, "%s\n", "[cmd2 = 14] n = 4, range = 16000");
	case 15:
		return sprintf(buf, "%s\n", "[cmd2 = 15] n = 4, range = 64000");

	default:
		return -EINVAL;
	}
}


static ssize_t
isl_sensing_range_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned int ret_val;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	switch (val) {
	case 1000:
		val = 0;
		break;
	case 4000:
		val = 1;
		break;
	case 16000:
		val = 2;
		break;
	case 64000:
		val = 3;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&mutex);
	ret_val = isl_set_range(client, val);
	mutex_unlock(&mutex);

	if (ret_val < 0)
		return ret_val;
	return count;
}

static ssize_t
ir_current_store(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned int ret_val;
	unsigned long val;

	if (!strncmp(buf, "12.5", 4))
		val = 3;
	else {
		if (kstrtoul(buf, 10, &val))
			return -EINVAL;
		switch (val) {
		case 100:
			val = 0;
			break;
		case 50:
			val = 1;
			break;
		case 25:
			val = 2;
			break;
		default:
			return -EINVAL;
		}
	}

	mutex_lock(&mutex);

	ret_val = i2c_smbus_read_byte_data(client, REG_CMD_2);
	if (ret_val < 0)
		goto err_exit;

	ret_val &= ~IR_CURRENT_MASK;	/*reset the bit before setting them */
	ret_val |= (val << 6);

	ret_val = i2c_smbus_write_byte_data(client, REG_CMD_2, ret_val);
	if (ret_val < 0)
		goto err_exit;

	mutex_unlock(&mutex);

	return count;

err_exit:
	mutex_unlock(&mutex);
	return -EINVAL;
}

static ssize_t
isl_sensing_mod_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret_val;
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
	if (val > 7)
		return -EINVAL;

	mutex_lock(&mutex);
	ret_val = isl_set_mod(client, val);
	mutex_unlock(&mutex);

	if (ret_val < 0)
		return ret_val;
	return count;
}

static ssize_t
isl_cmd2_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct isl_device *idev = i2c_get_clientdata(client);
	int res;
	unsigned long val;
	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
	if (val > 15 || val < 0)
		return -EINVAL;

	mutex_lock(&mutex);
	res = i2c_smbus_write_byte_data(client, REG_CMD_2, val);
	if (res < 0)
		printk("Warning - write failed\n");

	idev->resolution = isl_get_res(client);
	idev->range = isl_get_range(client);
	mutex_unlock(&mutex);

	return count;
}

static DEVICE_ATTR(range, S_IRUGO | S_IWUSR,
		   isl_sensing_range_show, isl_sensing_range_store);
static DEVICE_ATTR(mod, S_IRUGO | S_IWUSR,
		   isl_sensing_mod_show, isl_sensing_mod_store);
static DEVICE_ATTR(ir_current, S_IRUGO | S_IWUSR,
		   ir_current_show, ir_current_store);
static DEVICE_ATTR(output, S_IRUGO, isl_output_data_show, NULL);
static DEVICE_ATTR(cmd2, 0660,
		   isl_cmd2_show, isl_cmd2_store);
static DEVICE_ATTR(lux, S_IRUGO,
		   isl_lux_show, NULL);

static struct attribute *mid_att_isl[] = {
	&dev_attr_range.attr,
	&dev_attr_mod.attr,
	&dev_attr_ir_current.attr,
	&dev_attr_output.attr,
	&dev_attr_lux.attr,
	&dev_attr_cmd2.attr,
	NULL
};

static struct attribute_group m_isl_gr = {
	.name = "isl29034",
	.attrs = mid_att_isl
};

static int isl_set_default_config(struct i2c_client *client)
{
	struct isl_device *idev = i2c_get_clientdata(client);

	int ret = 0;
/* We don't know what it does ... */
	/*ret = i2c_smbus_write_byte_data(client, REG_CMD_1, 0xE0);*/
	/*ret = i2c_smbus_write_byte_data(client, REG_CMD_2, 0xC3);*/
/* Set default to ALS continuous */
	ret = i2c_smbus_write_byte_data(client, REG_CMD_1, 0xA0);
	if (ret < 0)
		return -EINVAL;
/* Range: 0~16000, number of clock cycles: 65536 */
	ret = i2c_smbus_write_byte_data(client, REG_CMD_2, 0x02);
	if (ret < 0)
		return -EINVAL;
	idev->resolution = isl_get_res(client);
	idev->range = isl_get_range(client);
	printk(KERN_INFO MODULE_NAME ": %s isl29034 set_default_config call, \n", __func__);

	return 0;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int isl29034_detect(struct i2c_client *client, int kind,
		struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;
	printk(KERN_INFO MODULE_NAME ": %s isl29034 detact call, kind:%d type:%s addr:%x \n", __func__, kind, info->type, info->addr);

	if (kind <= 0) {
		int vendor, device, revision;
		vendor = ISL29034_VENDOR;
		revision = ISL29034_REV_SHIFT;
		device = ISL29034_DEVICE;
	} else
		dev_dbg(&adapter->dev, "detection forced\n");

	strlcpy(info->type, "isl29034", I2C_NAME_SIZE);

	return 0;
}

static int
isl29034_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;

	struct isl_device *idev = kzalloc(sizeof(struct isl_device), GFP_KERNEL);
	if (!idev)
		return -ENOMEM;
	dev_info(&client->dev, "%s: ISL 011 chip found\n", client->name);

	printk(KERN_INFO MODULE_NAME ": %s isl29034 probe call, ID= %s\n", __func__, id->name);
	/*msleep(100);*/
	if (res < 0) {
		printk(KERN_INFO MODULE_NAME ": %s isl29034 set default config failed\n", __func__);
		return -EINVAL;
	}

	res = sysfs_create_group(&client->dev.kobj, &m_isl_gr);
	if (res) {
		printk(KERN_INFO MODULE_NAME ": %s isl29034 device create file failed\n", __func__);
		return -EINVAL;
	}

/* last mod is ALS continuous */
	last_mod = 5;
	pm_runtime_enable(&client->dev);

	dev_dbg(&client->dev, "isl29034 probe succeed!\n");

	printk(KERN_INFO MODULE_NAME ": %s isl29034 input registration passed\n", __func__);
	i2c_set_clientdata(client, idev);
	/* set default config after set_clientdata */
	res = isl_set_default_config(client);
	goto done;

done:

	return res;
}

static int isl29034_remove(struct i2c_client *client)
{
	struct isl_device *idev = i2c_get_clientdata(client);
	sysfs_remove_group(&client->dev.kobj, &m_isl_gr);
	__pm_runtime_disable(&client->dev, false);
	printk(KERN_INFO MODULE_NAME ": %s isl29034 remove call, \n", __func__);
	return 0;
}

static struct i2c_device_id isl29034_id[] = {
	{"isl29034", 0},
	{}
};

static int isl29034_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	dev_dbg(dev, "suspend\n");

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	isl_set_mod(client, ISL_MOD_POWERDOWN);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

	printk(KERN_INFO MODULE_NAME ": %s isl29034 suspend call, \n", __func__);
	return 0;
}

static int isl29034_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	dev_dbg(dev, "resume\n");

	mutex_lock(&mutex);
	pm_runtime_get_sync(dev);
	isl_set_mod(client, last_mod);
	pm_runtime_put_sync(dev);
	mutex_unlock(&mutex);

	printk(KERN_INFO MODULE_NAME ": %s isl29034 resume call, \n", __func__);
	return 0;
}

MODULE_DEVICE_TABLE(i2c, isl29034_id);

static const struct dev_pm_ops isl29034_pm_ops = {
	.runtime_suspend = isl29034_runtime_suspend,
	.runtime_resume = isl29034_runtime_resume,
};


#ifdef CONFIG_OF
static const struct of_device_id of_isl29034_als_match[] = {
	{ .compatible = "isl,isl29034", },
	{},
};
MODULE_DEVICE_TABLE(of, of_isl29034_als_match);
#endif

static struct i2c_driver isl29034_driver = {
	.driver = {
		   .name = "isl29034",
		   .pm = &isl29034_pm_ops,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(of_isl29034_als_match),
#endif
		   },
	.probe = isl29034_probe,
	.remove = isl29034_remove,
	.id_table = isl29034_id,
	.detect         = isl29034_detect,
};

static int __init sensor_isl29034_init(void)
{
	printk(KERN_INFO MODULE_NAME ": %s isl29034 init call, \n", __func__);
	/*
	 * Force device to initialize: i2c-15 0x44
	 * If i2c_new_device is not called, even isl29034_detect will not run
	 * TODO: rework to automatically initialize the device
	 */
	return i2c_add_driver(&isl29034_driver);
}

static void __exit sensor_isl29034_exit(void)
{
	printk(KERN_INFO MODULE_NAME ": %s isl29034 exit call \n", __func__);
	i2c_del_driver(&isl29034_driver);
}

module_init(sensor_isl29034_init);
module_exit(sensor_isl29034_exit);

MODULE_AUTHOR("mdigioia");
MODULE_ALIAS("isl29034 ALS/PS");
MODULE_DESCRIPTION("Intersil isl29034 ALS/PS Driver");
MODULE_LICENSE("GPL v2");

