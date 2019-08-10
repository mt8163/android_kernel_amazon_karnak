#include <linux/kernel.h>
#include <linux/module.h>
#include <sound/soc.h>
#include <linux/printk.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/ts3a225e.h>

static headset_plug_type_t current_plug_type = INPUT_TYPE_INVALID;

struct ts3a225e_dev {
    struct i2c_client *client;
};

struct ts3a225e_dev *switch_dev_info;

#define MAX_RETRY_COUNT 5

int ts3a225e_headset_switch_write_register(struct ts3a225e_dev *switch_info,
        unsigned char reg_addr, unsigned char reg_value);
int ts3a225e_headset_switch_read_register(struct ts3a225e_dev *switch_info,
        unsigned char reg_addr, unsigned char *reg_data, int max_bytes);

static int ts3a225e_switch_driver_probe(struct i2c_client *client,
                 const struct i2c_device_id *id)
{
    int retval = 0;
    int num_attempts = 0;
    int dev_found = 0;
    unsigned char reg_data;
    struct ts3a225e_dev *switch_dev = NULL;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("%s : need I2C_FUNC_I2C\n", __func__);
        return  -ENODEV;
    }

    switch_dev = kzalloc(sizeof(struct ts3a225e_dev), GFP_KERNEL);
    if (NULL == switch_dev) {
        pr_err("%s: failed to allocate memory for module data\n", __func__);
            retval = -ENOMEM;
            goto err_exit;
    }
    switch_dev->client = client;

    /* Verify a register can be read and is in the default state at power up */
    while (num_attempts < MAX_RETRY_COUNT) {
        retval = ts3a225e_headset_switch_read_register(switch_dev, TS3A225E_ID_REG_ADDR, &reg_data, 1);
        if (retval >= 0 && TS3A225E_ID_REG_DEFAULT_VALUE == reg_data) {
            dev_found = 1;
            break;
        }
        num_attempts++;
        mdelay(1);
    }

    if (!dev_found) {
        pr_err("%s: failed to read from TS3A225E chip during probing.  Result = %d, reg_data = 0x%02X\n", __func__, retval, reg_data);
        retval = -ENODEV;
        goto err_exit;
    }

    switch_dev_info = switch_dev;

    i2c_set_clientdata(client, switch_dev_info);
    ts3a225e_trigger_detection(INPUT_TYPE_INVALID);

    pr_info("%s: Completed TS3A225E driver probing for Client = %s, Address = 0x%02X, Flags = 0x%02X\n",
        __func__, client->name, client->addr, client->flags);
    return 0;

err_exit:
    if(switch_dev)
        kfree(switch_dev);

    pr_err("%s: ERROR Completing TS3A225E driver probing\n", __func__);
    return retval;
}

int ts3a225e_headset_switch_write_register(struct ts3a225e_dev *switch_info,
        unsigned char reg_addr, unsigned char reg_value) {
    int result = 0;
    unsigned char to_send[3] =  { reg_addr, reg_value };

    if (NULL == switch_info || NULL == switch_info->client) {
        pr_err("%s: No TS3A225E device was found during startup to write to. switch_dev_info = %s, client = %s\n",
            __func__, (NULL == switch_dev_info) ? "NULL" : "NOT NULL",
            (NULL == switch_dev_info || NULL == switch_dev_info->client) ? "NULL" : "NOT NULL");
        return -ENODEV;
    }

    result = i2c_master_send(switch_info->client, to_send, 2);
    pr_debug("%s: reg_addr = 0x%02X, reg_value = 0x%02X, result = %d\n", __func__, reg_addr, reg_value, result);

    return result;
}

int ts3a225e_headset_switch_read_register(struct ts3a225e_dev *switch_info,
        unsigned char reg_addr, unsigned char *reg_data, int max_bytes) {
    int result = 0;

    if (NULL == switch_info || NULL == switch_info->client) {
        pr_err("%s: No TS3A225E device was found during startup to write to. switch_dev_info = %s, client = %s\n",
            __func__, (NULL == switch_dev_info) ? "NULL" : "NOT NULL",
            (NULL == switch_dev_info || NULL == switch_dev_info->client) ? "NULL" : "NOT NULL");
        return -ENODEV;
    }

    result = i2c_master_send(switch_info->client, &reg_addr, 1);
    if (result > 0) {
        result = i2c_master_recv(switch_info->client, reg_data, result);
    } else {
        pr_err("%s: bad result from send = %d\n", __func__, result);
    }


    return result;
}

int ts3a225e_headset_switch_init(void) {
    int result = 0;
    if (NULL == switch_dev_info || NULL == switch_dev_info->client) {
        pr_err("%s: No TS3A225E device was found during startup, switch_dev_info = %s, client = %s\n",
            __func__, (NULL == switch_dev_info) ? "NULL" : "NOT NULL",
            (NULL == switch_dev_info || NULL == switch_dev_info->client) ? "NULL" : "NOT NULL");
        return -ENODEV;
    }

    /* Set CTRL3 Register AUTO_SW_DIS to 0, to disable automatic detection
       since QC is able to detect the type.  Remaining register values
       set to the defaults.
     */
    result = ts3a225e_headset_switch_write_register(switch_dev_info, TS3A225E_CTRL3_REG_ADDR, 0x0C);
    if (result < 0) {
        /* Note: We should continute at the point and try to set the device into
           CTIA mode.
         */
        pr_err("Failed to disable TS3A225E auto switching mode");
    }

    pr_debug("%s: completed with result %d\n", __func__, result);
    return result;
}
EXPORT_SYMBOL(ts3a225e_headset_switch_init);

int ts3a225e_headset_switch_deinit(void) {
    switch_dev_info = NULL;
    return 0;
}
EXPORT_SYMBOL(ts3a225e_headset_switch_deinit);

int ts3a225e_trigger_detection(headset_plug_type_t plug_type) {
    int result = 0;
    unsigned char reg_data;

    if (current_plug_type == plug_type && plug_type != INPUT_TYPE_INVALID)
        return 0;

    current_plug_type = plug_type;
    if (NULL == switch_dev_info || NULL == switch_dev_info->client) {
      pr_err("%s: No TS3A225E device was found during startup, switch_dev_info = %s, client = %s\n",
          __func__, (NULL == switch_dev_info) ? "NULL" : "NOT NULL",
          (NULL == switch_dev_info || NULL == switch_dev_info->client) ? "NULL" : "NOT NULL");
      return -ENODEV;
    }
    ts3a225e_headset_switch_read_register(switch_dev_info, TS3A225E_CTRL3_REG_ADDR, &reg_data, 1);

    result = ts3a225e_headset_switch_write_register(switch_dev_info, TS3A225E_CTRL3_REG_ADDR, reg_data|0x01);
    if (result < 0) {
      pr_err("Failed to trigger TS3A225E \n");
    }
    pr_err("ts3a225e_trigger_detection!plug_type=%d reg_data=0x%2x\n", plug_type, reg_data);
    return result;
}
EXPORT_SYMBOL(ts3a225e_trigger_detection);


int ts3a225e_headset_switch_set_type(headset_plug_type_t plug_type) {
    int result = 0;
    if (NULL == switch_dev_info) {
        pr_err("%s: No TS3A225E device was found during startup\n", __func__);
        return -ENODEV;
    }

    if (current_plug_type != plug_type) {
        if (INPUT_TYPE_HEADSET == plug_type) {
            /* Set CTRL1 Register to 0x0A for CTIA */
            result = ts3a225e_headset_switch_write_register(switch_dev_info,
                TS3A225E_CTRL1_REG_ADDR, 0x0A);

            if (result < 0) {
                pr_err("%s: Failed to update TS3A225E switch configuration to CTIA\n", __func__);
            } else {
                result = 0;
                current_plug_type = plug_type;
            }
        } else if(INPUT_TYPE_GND_MIC_SWAP == plug_type){
            /* Set CTRL1 Register to 0x0B for OMTP */
            result = ts3a225e_headset_switch_write_register(switch_dev_info,
                TS3A225E_CTRL1_REG_ADDR, 0x0B);
            if (result < 0) {
                pr_err("%s: Failed to update TS3A225E switch configuration to OMTP\n", __func__);
            } else {
                result = 0;
                current_plug_type = plug_type;
            }
    } else
        current_plug_type = plug_type;
    }

    pr_debug("%s: completed with result %d for plug_type = %d\n", __func__, result, plug_type);
    return result;
}
EXPORT_SYMBOL(ts3a225e_headset_switch_set_type);

static int ts3a225e_switch_driver_remove(struct i2c_client *client)
{
    struct ts3a225e_dev *dev_info = i2c_get_clientdata(client);
    if (NULL != dev_info)
        kfree(dev_info);
    switch_dev_info = NULL;

    pr_debug("Completed removal of TS3A225E device\n");
    return 0;
}

static struct of_device_id switch_match_table[] = {
	{ .compatible = "ti,ts3a225e",},
	{ },
};

static const struct i2c_device_id ts3a225e_id[] = {
    { "ts3a225e", 0 },
    {},
};
MODULE_DEVICE_TABLE(i2c, ts3a225e_id);

static struct i2c_driver ts3a225e_switch_driver = {
    .driver = {
        .owner  = THIS_MODULE,
        .name   = "ts3a225e",
        .of_match_table = switch_match_table,
     },
    .probe         = ts3a225e_switch_driver_probe,
    .remove        = ts3a225e_switch_driver_remove,
    .id_table      = ts3a225e_id,
};

static int __init ts3a225e_switch_driver_init(void)
{
    int ret;
    ret = i2c_add_driver(&ts3a225e_switch_driver);
    if (ret)
        pr_err("Unable to register TS3A225E driver\n");
    return ret;
}
module_init(ts3a225e_switch_driver_init);

static void __exit ts3a225e_switch_driver_exit(void)
{
    i2c_del_driver(&ts3a225e_switch_driver);
}
module_exit(ts3a225e_switch_driver_exit);


MODULE_AUTHOR("Amazon.com Inc.");
MODULE_DESCRIPTION("TS3A225E Headset switch module");

