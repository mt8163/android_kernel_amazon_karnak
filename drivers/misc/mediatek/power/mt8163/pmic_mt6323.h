#ifndef _PMIC_MT6323_H_
#define _PMIC_MT6323_H_

struct mt6323_chip_priv {
	struct device *dev;
	struct irq_domain *domain;
	struct wake_lock irq_lock;
	struct hrtimer check_pwrkey_release_timer;
	struct hrtimer long_press_pwrkey_shutdown_timer;
	unsigned long event_mask;
	unsigned long wake_mask;
	unsigned int saved_mask;
	u32 shutdown_time;
	u16 int_con[2];
	u16 int_stat[2];
	int irq;
	int num_int;
	int irq_hw_id;
	bool pressed:1;
};

struct mt6323_irq_data {
	const char *name;
	unsigned int irq_id;
	 irqreturn_t (*action_fn)(int irq, void *dev_id);
	bool enabled:1;
	bool wake_src:1;
};

#endif				/* _PMIC_MT6323_H_ */
