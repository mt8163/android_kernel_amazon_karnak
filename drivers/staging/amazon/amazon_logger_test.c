/* Amazon Logger driver Tester
 *
 * This is a driver to produce metrics/vitals logs to test
 * the amazon_logger.c. Once it is loaded, it will automatically and
 * continuously producing logs until it is being unloaded (rmmod).
 * There is a userland consumer to consume the logs, please check
 * tools/testing/amazon/logger/ for more information.
 *
 * Copyright 2016-2017 Amazon Technologies, Inc. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "amazon_logger_test: " fmt

#include <linux/cpu.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/metricslog.h>
#include <linux/sched.h>
#include <linux/time.h>

#define METRICS_STR_LEN 128
static int loop = 1;
module_param(loop, uint, 0644);
MODULE_PARM_DESC(loop, "# of loop");

static struct task_struct *producer_1;
static struct task_struct *producer_2;
static struct task_struct *producer_3;
static struct task_struct *producer_4;

static int metrics_test(void)
{
	int i;
	char metrics_log[METRICS_STR_LEN];
	int ret = -1;

	memset(metrics_log, 0, METRICS_STR_LEN);
	snprintf(metrics_log, METRICS_STR_LEN, "TEST:METRIC:VALUE_A=%d;CT;1,VALUE_B=%d;CT;1:NR",
			1, 0);

	for (i = 0; i < loop; i++) {
		/* normal case */
		ret = log_to_metrics(ANDROID_LOG_INFO, "metrics_test", metrics_log);
		if (ret)
			pr_err("log_to_metrics: fails in normal case %d\n", ret);
		/* domain=NULL case */
		ret = log_to_metrics(ANDROID_LOG_INFO, NULL, metrics_log);
		if (ret)
			pr_err("log_to_metrics: fails in domain=NULL case %d\n", ret);
		/* msg=NULL case */
		ret = log_to_metrics(ANDROID_LOG_INFO, NULL, NULL);
		if (!ret)
			pr_err("log_to_metrics: fails msg=NULL case %d\n", ret);

	}
	memset(metrics_log, 0, METRICS_STR_LEN);

	/* special msg case, need log_to_metrics() to replace the space into "_" */
	snprintf(metrics_log, METRICS_STR_LEN, "T e s t :M E T R I C:V A L U E A=%d;CT;1,V A L U E B=%d;CT;1:NR",
		1, 0);
	for (i = 0; i < loop; i++) {
		/* normal case */
		ret = log_to_metrics(ANDROID_LOG_INFO, "metrics_test", metrics_log);
		if (ret)
			pr_err("log_to_metrics with space: fails in normal case %d\n", ret);
		/* domain=NULL case */
		ret = log_to_metrics(ANDROID_LOG_INFO, NULL, metrics_log);
		if (ret)
			pr_err("log_to_metrics with space: fails in domain=NULL case %d\n", ret);
		/* msg=NULL case */
		ret = log_to_metrics(ANDROID_LOG_INFO, NULL, NULL);
		if (!ret)
			pr_err("log_to_metrics with space: fails msg=NULL case%d\n", ret);

	}
	return 0;
}

static int vital_test(void)
{
	int i;
	struct timespec now;
	char metrics_log[METRICS_STR_LEN];
	int ret = -1;

	now = current_kernel_time();
	memset(metrics_log, 0, METRICS_STR_LEN);
	snprintf(metrics_log, METRICS_STR_LEN, "Kernel:TEST:VALUE_A=%d;CT;1,VALUE_B=%d;CT;1:NR",
				1, 0);

	for (i = 0; i < loop; i++) {
		/* key=NULL case */
		ret = log_timer_to_vitals(ANDROID_LOG_INFO, "Kernel timer", "TEST",
					"time_in_test", NULL, now.tv_sec, NULL, VITALS_TIME_BUCKET);
		if (ret)
			pr_err("log_timer_to_vitals: fails in key=NULL case %d\n", ret);
		/* normal case */
		ret = log_timer_to_vitals(ANDROID_LOG_INFO, "Kernel timer", "TEST",
					"time_in_test", "Test", now.tv_sec, "s", VITALS_TIME_BUCKET);
		if (ret)
			pr_err("log_timer_to_vitals: fails in normal case %d\n", ret);
	}

	for (i = 0; i < loop; i++) {
		/* nomral case */
		ret = log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel vitals", "TEST",
					"VITALS", "test_value", (u32)1, "count", "s", VITALS_NORMAL);
		if (ret)
			pr_err("log_counter_to_vitals fails in nomral case %d\n", ret);
		/* metadata=NULL case */
		ret = log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel vitals", "TEST",
					"VITALS", "test_value", (u32)1, "count", NULL, VITALS_NORMAL);
		if (ret)
			pr_err("log_counter_to_vitals: fails in metadata=NULL %d\n", ret);
		/* key=NULL case */
		ret = log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel vitals", "TEST", "VITALS",
					NULL, (u32)1, "count", "s", VITALS_NORMAL);
		if (ret)
			pr_err("log_counter_to_vitals: fails in key=NULL case %d\n", ret);
		/* key=NULL, metadata=NULL case */
		ret = log_counter_to_vitals(ANDROID_LOG_INFO, "Kernel vitals", "TEST", "VITALS",
					NULL, (u32)1, "count", NULL, VITALS_NORMAL);
		if (ret)
			pr_err("log_counter_to_vitals: fails in key=NULL,metadata=NULL case %d\n", ret);
	}
	return 0;
}

/* metric only */
static int logger_producer_thread_1(void *arg)
{
	while (!kthread_should_stop()) {
		metrics_test();
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
	}
	return 0;
}

/* vitals only */
static int logger_producer_thread_2(void *arg)
{
	while (!kthread_should_stop()) {

		vital_test();
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
	}
	return 0;
}

/* vitals and metric same time */
static int logger_producer_thread_3(void *arg)
{

	while (!kthread_should_stop()) {

		metrics_test();
		vital_test();
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
	}
	return 0;
}

/* vitals and metric mixed at different time */
static int logger_producer_thread_4(void *arg)
{

	while (!kthread_should_stop()) {
		metrics_test();
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
		vital_test();
	}
	return 0;
}

static int __init logger_test_init(void)
{

	producer_1 = kthread_run(logger_producer_thread_1,
				NULL, "logger_producer_1");
	producer_2 = kthread_run(logger_producer_thread_2,
				NULL, "logger_producer_2");
	producer_3 = kthread_run(logger_producer_thread_3,
				NULL, "logger_producer_3");
	producer_4 = kthread_run(logger_producer_thread_4,
				NULL, "logger_producer_4");
	return 0;
}

static void __exit logger_test_exit(void)
{
	if (producer_1)
		kthread_stop(producer_1);
	if (producer_2)
		kthread_stop(producer_2);
	if (producer_3)
		kthread_stop(producer_3);
	if (producer_4)
		kthread_stop(producer_4);
}

module_init(logger_test_init);
module_exit(logger_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Amazon Logger Test");
