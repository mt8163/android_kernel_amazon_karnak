/*
 * Copyright 2018 Amazon Technologies, Inc. All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "amazon-gating: " fmt

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/input.h>
#include <misc/gating.h>
#include <linux/gpio/consumer.h>

#define DEFAULT_DEBOUNCE_INTERVAL 5
#define LONG_PRESS_DURATION msecs_to_jiffies(500)

struct gating_input_event {
	const char *desc;
	unsigned int code;
	int debounce_interval;
	struct input_dev *input_dev;
	struct delayed_work work;
	bool wakeup_capable;
	struct gpio_desc *button_gpio;
	int last_button_event;
	unsigned long last_event_time;
	struct gating_priv *priv;
};

struct gating_priv {
	struct gpio_desc *state_gpio;
	struct gpio_desc *enable_gpio;
	struct mutex mutex;
	struct gating_input_event *input_event;
	struct work_struct priv_update;
	enum gating_state cur_state;
	enum gating_state req_state;
};

static struct srcu_notifier_head *priv_nh;

int register_gating_state_notifier(struct notifier_block *nb)
{
	int ret = -EAGAIN;

	if (priv_nh)
		ret = srcu_notifier_chain_register(priv_nh, nb);

	return ret;
}
EXPORT_SYMBOL(register_gating_state_notifier);

static enum gating_state __gating_state(struct gating_priv *priv)
{
	int value;

	value = gpiod_get_value_cansleep(priv->state_gpio);
	if (unlikely(value < 0)) {
		pr_err_ratelimited("gating: status gpio read failed=%d\n",
					value);
		return UNGATED;
	}

	return value;
}

static int __set_gating_state(struct gating_priv *priv, int enable)
{
	int i = 0;
	const int max_wait = 1000;

	gpiod_set_value_cansleep(priv->enable_gpio, enable);

	/* wait for gating enabled for up to 100ms */
	while (i < max_wait) {
		if (__gating_state(priv))
			break;
		usleep_range(1000, 1100);
		i++;
	}

	gpiod_set_value_cansleep(priv->enable_gpio, !enable);

	if (i == max_wait)
		return -ETIMEDOUT;

	return 0;
}

static void gating_update_work_func(struct work_struct *work)
{
	int ret = 0;
	struct gating_priv *priv =
		container_of(work, struct gating_priv, priv_update);
	enum gating_state cur_state, req_state = priv->req_state;

	ret = srcu_notifier_call_chain(priv_nh, req_state, NULL);
	if (ret == NOTIFY_BAD) {
		pr_err("Privacy notification failed : %d\n", ret);
		goto skip;
	}

	if (req_state == GATED) {
		ret = __set_gating_state(priv, req_state);
		if (ret)
			pr_err("Failed to configure gating state: %d\n", ret);
	}

skip:
	cur_state = __gating_state(priv);

	if (cur_state != req_state) {
		ret = srcu_notifier_call_chain(priv_nh, cur_state, NULL);
		if (ret == NOTIFY_BAD)
			pr_err("Privacy notification failed : %d\n", ret);
	}

	priv->cur_state = cur_state;
	priv->req_state = NONE;

	pr_debug("%s: cur_state : %d, req_state : %d\n", __func__,
			priv->cur_state, priv->req_state);
}

static void handle_gating_button_event(struct gating_input_event *input,
						bool value)
{
	int cur_state;
	struct gating_priv *priv = input->priv;
	unsigned long delay;

	/* if button is pressed */
	if (value) {
		input->last_event_time = jiffies;
		return;
	}

	delay = input->last_event_time + LONG_PRESS_DURATION;
	/* if it is long press, it might be a PWR or Factory reset */
	if (time_before(delay, jiffies))
		goto out;

	cancel_work_sync(&priv->priv_update);

	cur_state = __gating_state(priv);

	/* Theoretically cur_state should be UNGATED, because button press
	 * event automatically changes the state of circuit to UNGATED.
	 * This case _may_ occur only when a priv_update() work will get
	 * executed after handling an immediate gating interrupt but before
	 * executing the corresponding handle_gating_button_event().
	 * skip this event
	 */
	if (cur_state == GATED)
		goto out;

	mutex_lock(&priv->mutex);

	/* Just incase the priv_update() work, pending from previous interrupt
	 * got cancelled
	 */
	if (priv->req_state == GATED) {
		priv->cur_state = UNGATED;
		priv->req_state = NONE;
		mutex_unlock(&priv->mutex);
		goto out;
	}

	if (priv->cur_state == UNGATED) {
		priv->cur_state = ONGOING;
		priv->req_state = GATED;
	} else {
		priv->cur_state = UNGATED;
		priv->req_state = UNGATED;
	}

	mutex_unlock(&priv->mutex);

	pr_debug("%s: cur_state : %d, req_state : %d\n", __func__,
			priv->cur_state, priv->req_state);

	schedule_work(&priv->priv_update);
out:
	input->last_event_time = 0;
}

static void button_input_event_work_func(struct work_struct *work)
{
	int value;
	struct gating_input_event *input =
		container_of(work, struct gating_input_event, work.work);

	value = gpiod_get_value_cansleep(input->button_gpio);
	if (unlikely(value < 0)) {
		/*
		 * gpio read can fail, however we should report button
		 * press in order to notify userspace that gating
		 * state has been changed.  force it to
		 * !input->last_button_event for that case in the hope
		 * we just missed one press or release.
		 */
		pr_warn_ratelimited("gating: button gpio read failed=%d\n",
					value);
		value = !input->last_button_event;
	}

	if (input->last_button_event == value) {
		/*
		 * We can reach here when :
		 * 1) previous press/release has been canceled due to
		 *    debouce interval.
		 * 2) gpio_get_value() failed.
		 *
		 * We should report button press by all means in order for
		 * userspace to be notified about new gating mode change.
		 * Thus send out an artificial event.
		 */
		handle_gating_button_event(input, !value);
		input_report_key(input->input_dev, input->code, !value);
		input_sync(input->input_dev);
		pr_info("%s: sent a fake event : %d\n", __func__, !value);
	} else {
		input->last_button_event = value;
	}

	handle_gating_button_event(input, value);
	input_report_key(input->input_dev, input->code, value);
	input_sync(input->input_dev);
	pr_info("%s: sent an event : %d\n", __func__, value);

	if (input->wakeup_capable)
		pm_relax(input->input_dev->dev.parent);
}

static irqreturn_t gating_interrupt(int irq, void *arg)
{
	struct gating_input_event *priv_event = arg;

	if (priv_event->wakeup_capable)
		pm_stay_awake(priv_event->input_dev->dev.parent);

	cancel_delayed_work(&priv_event->work);
	schedule_delayed_work(&priv_event->work,
			msecs_to_jiffies(priv_event->debounce_interval));

	return IRQ_HANDLED;
}

static int gating_setup_input_event(struct platform_device *pdev)
{
	int ret;
	struct input_dev *input;
	struct device *dev = &pdev->dev;
	struct gating_priv *priv = platform_get_drvdata(pdev);

	INIT_DELAYED_WORK(&priv->input_event->work,
				  button_input_event_work_func);

	input = devm_input_allocate_device(dev);
	if (!input)
		return -ENOMEM;

	input->name = "gating";
	input->dev.parent = &pdev->dev;

	input_set_capability(input, EV_KEY, priv->input_event->code);

	priv->input_event->input_dev = input;

	ret = input_register_device(input);
	if (ret)
		return ret;

	return 0;
}

static int gating_input_event_parse_of(struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct device_node *input_event_node;
	struct gpio_desc *gpiod;
	struct gating_priv *priv = platform_get_drvdata(pdev);

	node = pdev->dev.of_node;

	input_event_node = of_get_child_by_name(node, "input_event");
	if (!input_event_node) {
		dev_err(&pdev->dev, "No input event configured\n");
		return -EINVAL;
	}

	priv->input_event = devm_kzalloc(&pdev->dev, sizeof(*priv->input_event),
					 GFP_KERNEL);
	if (!priv->input_event)
		return -ENOMEM;

	priv->input_event->desc = of_get_property(input_event_node, "label",
						  NULL);

	if (of_property_read_u32(input_event_node, "linux,code",
				 &priv->input_event->code))
		return -EINVAL;

	if (of_property_read_u32(input_event_node, "debounce-interval",
				 &priv->input_event->debounce_interval))
		priv->input_event->debounce_interval =
			DEFAULT_DEBOUNCE_INTERVAL;

	gpiod = devm_get_gpiod_from_child(&pdev->dev, NULL,
						&input_event_node->fwnode);
	ret = PTR_ERR_OR_ZERO(gpiod);
	if (ret) {
		dev_err(&pdev->dev, "falied to get MUTE gpio %d\n", ret);
		return ret;
	}
	gpiod_direction_input(gpiod);
	priv->input_event->button_gpio = gpiod;

	priv->input_event->wakeup_capable =
		of_property_read_bool(input_event_node, "wakeup-source");

	return 0;
}

static enum gating_state gating_state(struct device *dev)
{
	enum gating_state state;
	struct platform_device *pdev = to_platform_device(dev);
	struct gating_priv *priv = platform_get_drvdata(pdev);

	mutex_lock(&priv->mutex);

	if (priv->cur_state == ONGOING)
		state = ONGOING;
	else
		state = __gating_state(priv);

	mutex_unlock(&priv->mutex);

	return state;
}

static ssize_t show_gating_state(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int state = gating_state(dev);

	if (state < 0)
		return state;

	return snprintf(buf, PAGE_SIZE, "%d\n", state);
}

static int set_gating_state(struct device *dev, enum gating_state state)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct gating_priv *priv = platform_get_drvdata(pdev);

	/* Don't allow userspace to turn off Privacy Mode */
	if (state == UNGATED)
		return -EINVAL;

	cancel_work_sync(&priv->priv_update);

	mutex_lock(&priv->mutex);
	priv->cur_state = ONGOING;
	priv->req_state = GATED;
	mutex_unlock(&priv->mutex);

	schedule_work(&priv->priv_update);

	return ret;
}

static ssize_t store_gating_enable(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int enable, ret;

	if (!kstrtoint(buf, 10, &enable)) {
		ret = set_gating_state(dev, enable);
		if (ret)
			return ret;
	} else {
		return -EINVAL;
	}

	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IWGRP,
		   NULL, store_gating_enable);
static DEVICE_ATTR(state, S_IRUGO, show_gating_state, NULL);

static struct attribute *gpio_keys_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_state.attr,
	NULL,
};

static struct attribute_group gpio_keys_attr_group = {
	.attrs = gpio_keys_attrs,
};

static int gating_probe(struct platform_device *pdev)
{
	int ret;
	struct gating_priv *priv;
	struct device *dev = &pdev->dev;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv_nh = devm_kzalloc(dev, sizeof(*priv_nh), GFP_KERNEL);
	if (!priv_nh)
		return -ENOMEM;

	srcu_init_notifier_head(priv_nh);

	mutex_init(&priv->mutex);

	platform_set_drvdata(pdev, priv);

	priv->state_gpio = devm_gpiod_get(dev, "state", GPIOD_IN);
	ret = PTR_ERR_OR_ZERO(priv->state_gpio);
	if (ret) {
		dev_err(dev, "cannot get state-gpios %d\n", ret);
		return ret;
	}

	priv->enable_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	ret = PTR_ERR_OR_ZERO(priv->enable_gpio);
	if (ret) {
		dev_err(dev, "cannot get state-gpios %d\n", ret);
		return ret;
	}

	ret = gating_input_event_parse_of(pdev);
	if (ret) {
		pr_err("failed to parse input event device tree = %d\n", ret);
		return ret;
	}

	ret = gating_setup_input_event(pdev);
	if (ret) {
		pr_err("failed to setup input event = %d\n", ret);
		return ret;
	}

	priv->input_event->priv = priv;
	priv->req_state = NONE;
	INIT_WORK(&priv->priv_update, gating_update_work_func);

	ret = devm_request_irq(&pdev->dev,
				gpiod_to_irq(priv->input_event->button_gpio),
				gating_interrupt,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"gating", priv->input_event);
	if (ret) {
		pr_err("failed to request interrupt = %d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(&dev->kobj, &gpio_keys_attr_group);
	if (ret) {
		pr_err("failed to create sysfs group = %d\n", ret);
		return ret;
	}

	if (priv->input_event != NULL)
		device_init_wakeup(&pdev->dev,
					priv->input_event->wakeup_capable);

	dev_info(dev, "%s : done\n", __func__);

	return 0;
}

static int gating_remove(struct platform_device *pdev)
{
	struct gating_priv *priv;
	struct device *dev = &pdev->dev;
	struct gating_input_event *priv_event;

	priv = platform_get_drvdata(pdev);
	priv_event = priv->input_event;

	devm_free_irq(dev,
		      gpiod_to_irq(priv->state_gpio),
		      "gating");

	sysfs_remove_group(&dev->kobj, &gpio_keys_attr_group);
	cancel_delayed_work_sync(&priv_event->work);
	pm_relax(priv_event->input_dev->dev.parent);
	mutex_destroy(&priv->mutex);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gating_suspend(struct device *dev)
{
	struct gating_priv *priv;
	struct gating_input_event *input_event;
	struct platform_device *pdev = to_platform_device(dev);

	priv = platform_get_drvdata(pdev);
	input_event = priv->input_event;

	if (input_event && input_event->wakeup_capable)
		enable_irq_wake(gpiod_to_irq(input_event->button_gpio));

	return 0;
}

static int gating_resume(struct device *dev)
{
	struct gating_priv *priv;
	struct gating_input_event *input_event;
	struct platform_device *pdev = to_platform_device(dev);

	priv = platform_get_drvdata(pdev);
	input_event = priv->input_event;

	if (input_event && input_event->wakeup_capable)
		disable_irq_wake(gpiod_to_irq(input_event->button_gpio));

	return 0;
}
#endif

static const struct of_device_id gating_of_table[] = {
	{ .compatible = "amazon-gating", },
	{ },
};
MODULE_DEVICE_TABLE(of, gating_of_table);

static SIMPLE_DEV_PM_OPS(gating_pm_ops, gating_suspend,
			gating_resume);

static struct platform_driver gating_driver = {
	.driver = {
		.name = "gating",
#ifdef CONFIG_PM_SLEEP
		.pm = &gating_pm_ops,
#endif
		.of_match_table	= of_match_ptr(gating_of_table),
	},
	.probe	= gating_probe,
	.remove	= gating_remove,
};

static int __init gating_init(void)
{
	return platform_driver_register(&gating_driver);
}

static void __exit gating_exit(void)
{
	platform_driver_unregister(&gating_driver);
}

module_init(gating_init);
module_exit(gating_exit);
MODULE_LICENSE("GPL");
