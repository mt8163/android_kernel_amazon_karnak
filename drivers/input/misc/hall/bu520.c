/*
 * Hall sensor driver capable of dealing with more than one sensor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>

#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/input/bu520.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define METRICS_STR_LEN 128
#endif

#define DELAY_VALUE 50
#define TIMEOUT_VALUE 100

enum cover_status {
	COVER_CLOSED = 0,
	COVER_OPENED
};
struct hall_priv {
	struct work_struct work;
	struct workqueue_struct *workqueue;
	enum   cover_status cover;
	struct input_dev *input;
	struct hall_sensor_data *sensor_data;
	int hall_sensor_num;
	struct wakeup_source hall_wake_lock;
};

#ifdef CONFIG_PROC_FS
#define HALL_PROC_FILE "driver/hall_sensor"
static int hall_show(struct seq_file *m, void *v)
{
	struct hall_priv *priv = m->private;
	struct hall_sensor_data *sensor_data = priv->sensor_data;

	u8 reg_val = 0;
	int i;

	for (i = 0; i < priv->hall_sensor_num; i++)
		reg_val = reg_val | (sensor_data[i].gpio_state << i);

	seq_printf(m, "0x%x\n", reg_val);
	return 0;
}

static int hall_open(struct inode *inode, struct file *file)
{
	return single_open(file, hall_show, PDE_DATA(inode));
}

static const struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.open = hall_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_hall_proc_file(struct hall_priv *priv)
{
	struct proc_dir_entry *entry;

	entry = proc_create_data(HALL_PROC_FILE, 0644, NULL, &proc_fops, priv);
	if (!entry)
		pr_err("%s: Error creating %s\n", __func__, HALL_PROC_FILE);
}

static void remove_hall_proc_file(void)
{
	remove_proc_entry(HALL_PROC_FILE, NULL);
}
#endif

static void hall_work_func(struct work_struct *work)
{
	struct hall_priv *priv = container_of(work, struct hall_priv, work);
#ifdef CONFIG_AMAZON_METRICS_LOG
	char metrics_log[METRICS_STR_LEN];
#endif
	struct input_dev *input = priv->input;
	unsigned long gpio_pin = priv->sensor_data[0].gpio_pin;

	if (!input)
		return;

	msleep(DELAY_VALUE);
	priv->sensor_data[0].gpio_state = gpio_get_value(gpio_pin);
	priv->cover = priv->sensor_data[0].gpio_state ? COVER_OPENED : COVER_CLOSED;

#ifdef CONFIG_AMAZON_METRICS_LOG
	/* Log in metrics */
	//TODO DISPLAY is useless for hall sensor under current implementation
	//Revise metrics format then stop logging fake display state.
	snprintf(metrics_log, METRICS_STR_LEN, "bu520:TIMEOUT:COVER=%d;CT;1,DISPLAY=%d;CT;1:NR",
		priv->cover, 0);
	log_to_metrics(ANDROID_LOG_INFO, "HallSensor", metrics_log);
#endif

	input_report_switch(input, SW_LID, priv->sensor_data[0].gpio_state ? 0 : 1);
	input_sync(input);
}

static irqreturn_t hall_sensor_isr(int irq, void *data)
{
	struct hall_priv *priv =
			(struct hall_priv *)data;
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	int i;
	unsigned long gpio_pin;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		if (irq == sensor_data[i].irq) {
			gpio_pin = sensor_data[i].gpio_pin;
			priv->sensor_data[i].gpio_state = gpio_get_value(gpio_pin);
			irq_set_irq_type(irq, priv->sensor_data[i].gpio_state ?
					 IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);

			/* Avoid unnecessary suspend */
			__pm_wakeup_event(&priv->hall_wake_lock, TIMEOUT_VALUE);
			queue_work(priv->workqueue, &priv->work);
			break;
		}
	}

	return IRQ_HANDLED;
}

static int configure_irqs(struct hall_priv *priv)
{
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	int i;
	int ret = 0;
	unsigned long irqflags = 0;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		int gpio_pin = sensor_data[i].gpio_pin;
#ifndef CONFIG_OF
		sensor_data[i].irq = gpio_to_irq(gpio_pin);
#endif
		sensor_data[i].gpio_state = gpio_get_value(gpio_pin);
		priv->cover = sensor_data[i].gpio_state ? COVER_OPENED : COVER_CLOSED;
		enable_irq_wake(sensor_data[i].irq);
		irqflags |= IRQF_ONESHOT;
		ret = request_threaded_irq(sensor_data[i].irq, NULL,
			hall_sensor_isr, irqflags,
			sensor_data[i].name, priv);

		if (ret < 0)
			break;

		irq_set_irq_type(sensor_data[i].irq, priv->sensor_data[i].gpio_state ?
			 IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING);
	}

	return ret;
}

struct input_dev *hall_input_device_create(void)
{
	int err = 0;
	struct input_dev *input;

	input = input_allocate_device();
	if (!input) {
		err = -ENOMEM;
		goto exit;
	}

	set_bit(EV_SW, input->evbit);
	set_bit(SW_LID, input->swbit);

	err = input_register_device(input);
	if (err) {
		goto exit_input_free;
	}
	return input;

exit_input_free:
	input_free_device(input);
exit:
	return NULL;
}

static int hall_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct input_dev *input;
	struct hall_priv *priv;
	struct hall_sensors *hall_sensors = pdev->dev.platform_data;
	int error = 0;

#ifdef CONFIG_OF
	struct hall_sensors hall_sensors_template;
	struct device_node *of_node;
	int i;

	hall_sensors = &hall_sensors_template;
	of_node = pdev->dev.of_node;

	error = of_property_read_u32(of_node, "sensor-num",
			&hall_sensors->hall_sensor_num);
	if (error) {
		dev_err(dev, "Unable to read hall_sensor_num\n");
		return error;
	}

	hall_sensors->hall_sensor_data = kzalloc(sizeof(struct hall_sensor_data)
								* hall_sensors->hall_sensor_num, GFP_KERNEL);
	if (!hall_sensors->hall_sensor_data) {
		dev_err(dev, "Failed to allocate hall sensor data\n");
		return -ENOMEM;
	}

	for (i = 0; i < hall_sensors->hall_sensor_num; i++) {
		hall_sensors->hall_sensor_data[i].gpio_pin = of_get_named_gpio_flags(
				of_node, "int-gpio", 0, NULL);
		if (!gpio_is_valid(hall_sensors->hall_sensor_data[i].gpio_pin)) {
			dev_err(dev, "Unable to read int-gpio[%d]\n", i);
			goto fail0;
		}

		error = of_property_read_string_index(of_node, "sensor-name", i,
				(const char**)&hall_sensors->hall_sensor_data[i].name);
		if (error) {
			dev_err(dev, "Unable to read sensor-name[%d]\n", i);
			goto fail0;
		}

		hall_sensors->hall_sensor_data[i].irq = irq_of_parse_and_map(of_node, i);
		if (hall_sensors->hall_sensor_data[i].irq <= 0) {
			dev_err(dev, "Unable to get interrupt number[%d]\n", i);
			error = -EINVAL;
			goto fail0;
		}

		hall_sensors->hall_sensor_data[i].gpio_state = -1;
	}
#endif

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Failed to allocate driver data\n");
		error = -ENOMEM;
#ifdef CONFIG_OF
		goto fail0;
#else
		return error;
#endif
	}

	input = hall_input_device_create();
	if (!input) {
		dev_err(dev, "Failed to allocate input device\n");
		error = -ENOMEM;
		goto fail1;
	}

	input->name = pdev->name;
	input->dev.parent = &pdev->dev;
	priv->input = input;
	priv->sensor_data = hall_sensors->hall_sensor_data;
	priv->hall_sensor_num = hall_sensors->hall_sensor_num;

	priv->workqueue = create_singlethread_workqueue("workqueue");
	if (!priv->workqueue) {
		dev_err(dev, "Unable to create workqueue\n");
		goto fail2;
	}
	INIT_WORK(&priv->work, hall_work_func);

	platform_set_drvdata(pdev, priv);

	device_init_wakeup(&pdev->dev, 1);

	wakeup_source_init(&priv->hall_wake_lock, "hall sensor wakelock");

	error = configure_irqs(priv);
	if (error) {
		dev_err(dev, "Failed to configure interupts, err=%d\n", error);
		goto fail3;
	}

#ifdef CONFIG_PROC_FS
	create_hall_proc_file(priv);
#endif

	return 0;

fail3:
	destroy_workqueue(priv->workqueue);
fail2:
	input_unregister_device(priv->input);
fail1:
	kfree(priv);
#ifdef CONFIG_OF
fail0:
	kfree(hall_sensors->hall_sensor_data);
#endif

	return error;
}

static int hall_remove(struct platform_device *pdev)
{
	struct hall_priv *priv = platform_get_drvdata(pdev);
	struct hall_sensor_data *sensor_data = priv->sensor_data;
	int i;

	for (i = 0; i < priv->hall_sensor_num; i++) {
		int irq = sensor_data[i].irq;
		free_irq(irq, priv);
	}

	flush_workqueue(priv->workqueue);
	destroy_workqueue(priv->workqueue);
	input_unregister_device(priv->input);

	wakeup_source_trash(&priv->hall_wake_lock);
#ifdef CONFIG_OF
	kfree(sensor_data);
#endif
	kfree(priv);
	device_init_wakeup(&pdev->dev, 0);

#ifdef CONFIG_PROC_FS
	remove_hall_proc_file();
#endif
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id hall_of_match[] = {
	{.compatible = "rohm,hall-bu520", },
	{.compatible = "rohm,hall-ah9250", },
	{ },
};

MODULE_DEVICE_TABLE(of, hall_of_match);
#endif

static struct platform_driver hall_driver = {
	.probe = hall_probe,
	.remove = hall_remove,
	.driver = {
		   .name = "hall-bu520",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = hall_of_match,
#endif
			}
};

module_platform_driver(hall_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Quanta");
MODULE_DESCRIPTION("Hall sensor driver");
