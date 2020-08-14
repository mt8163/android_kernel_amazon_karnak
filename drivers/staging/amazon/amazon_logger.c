/* Amazon Logger driver
 *
 * This Amazon logger driver is a variant version of Google original
 * logging driver(drivers/misc/logger.c). It adds two Amazon specific logging
 * channels (metrics and vitals). It is re-implemented with Linux ring_buffer
 * to take benefits of ring_buffer's buffer management.
 * For more information, please refer to Documentation/amazon/amazon_logger.txt
 *
 * Portions copyright 2016-2017 Amazon Technologies, Inc. All Rights Reserved.
 *
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
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

#define pr_fmt(fmt) "amazon_logger: " fmt

#include <linux/aio.h>
#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/irq_work.h>
#include <linux/metricslog.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/ring_buffer.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/uio.h>

static int metrics_init;
#define VITAL_ENTRY_MAX_PAYLOAD 512
#define AMAZON_LOGGER_LOG_METRICS "metrics"
#define AMAZON_LOGGER_LOG_VITALS "vitals"
/* 4K - sizeof(logger_entry) = 4076 */
#define LOGGER_ENTRY_MAX_PAYLOAD 4076
#define AMAZON_METRICS_BUF_SIZE (16*1024)
#define AMAZON_VITALS_BUF_SIZE (128*1024)

/**
 * logger_entry (v1 version)
 * @len:	The length of the payload
 * @pad:	no matter what, we get 2 bytes of padding
 * @pid:	The generating process' process ID
 * @tid:	The generating process' thread ID
 * @sec:	The number of seconds that have elapsed since the Epoch
 * @nsec:	The number of nanoseconds that have elapsed since @sec
 * @msg:	The message that is to be logged
 */
struct logger_entry {
	__u16		len;
	__u16		pad;
	__s32		pid;
	__s32		tid;
	__s32		sec;
	__s32		nsec;
	char		msg[0];
} __attribute__((__packed__));

/*
 * amazon_logger: a linux trace ring_buffer based metric logger.
 * It takes the benefits of Linux trace ring_buffer:
 *  -let ring_buffer to manage the buffer.
 *  -ring_buffer allows multiply producers writing at same time
 *  on different cpus.
 */
struct amazon_logger {
	struct ring_buffer *buf;
	struct miscdevice	misc;
	struct mutex		mutex;
	struct list_head	logs;
};
static LIST_HEAD(log_list);

struct amazon_logger *metrics_logger;
struct amazon_logger *vitals_logger;

/*
 * logger_kernel_write,the ring_buffer producer.
 *
 */
static int logger_kernel_write(struct ring_buffer *buf,
			const struct iovec *iov, unsigned long nr_segs)
{
	struct logger_entry header;
	struct timespec now;
	unsigned long count = nr_segs;
	size_t total_len = 0;
	struct ring_buffer_event *event;
	unsigned int event_len;
	void *data;

	while (count-- > 0)
		total_len += iov[count].iov_len;

	now = current_kernel_time();
	header.pad = 0;
	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.len = min_t(size_t, total_len, LOGGER_ENTRY_MAX_PAYLOAD);

	event = ring_buffer_lock_reserve(buf, sizeof(struct logger_entry)
			+ header.len);
	if (!event) {
		pr_err("failed to ring_buffer_lock_reserve");
		return -ENOMEM;
	}

	event_len = ring_buffer_event_length(event);
	if (event_len < sizeof(struct logger_entry) + header.len) {
		pr_err("no enough buffer reserved %d", event_len);
		/* to be paired with ring_buffer_lock_reserve. */
		ring_buffer_unlock_commit(buf, event);
		return -ENOMEM;
	}

	data = ring_buffer_event_data(event);
	/* copy logger_entry's header to ring_buffer */
	memcpy(data, &header, sizeof(struct logger_entry));
	data += sizeof(struct logger_entry);

	total_len = 0;
	while (nr_segs-- > 0) {
		size_t len;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - total_len);
		/* copy msg payload to ring_buffer */
		memcpy(data + total_len, iov->iov_base, len);
		total_len += len;
		iov++;
	}
	/* commit the ring_buffer writing, it will wake up the
	 * ring_buffer waiters.
	 */
	ring_buffer_unlock_commit(buf, event);
	return 0;
}

/**
 * log_to_metrics - add a metric message to metrics log buffer
 * @priority: the Android priority of the message
 * @domain:	the domain of this message belong to
 * @log_msg:	the message playload
 * @Returns:	0 on success, error code on failurel
 */
int log_to_metrics(enum android_log_priority priority,
	const char *domain, char *log_msg)
{
	char *p = log_msg;
	int ret = -EINVAL;

	if (metrics_init != 0 && log_msg != NULL) {
		struct iovec vec[3];

		if (domain == NULL)
			domain = "kernel";

		while (*p != '\0') {
			if (' ' == *p)
				*p = '_';
			p++;
		}

		vec[0].iov_base = (unsigned char *)&priority;
		vec[0].iov_len  = 1;

		vec[1].iov_base = (void *)domain;
		vec[1].iov_len  = strlen(domain) + 1;

		vec[2].iov_base = (void *)log_msg;
		vec[2].iov_len  = strlen(log_msg) + 1;

		ret = logger_kernel_write(metrics_logger->buf, vec, 3);
	}
	return ret;
}
EXPORT_SYMBOL(log_to_metrics);

static int log_to_vitals(enum android_log_priority priority,
	const char *domain, const char *log_msg)
{
	int ret = -EINVAL;

	if (metrics_init != 0 && log_msg != NULL) {
		struct iovec vec[3];

		if (domain == NULL)
			domain = "kernel";

		vec[0].iov_base = (unsigned char *)&priority;
		vec[0].iov_len  = 1;

		vec[1].iov_base = (void *)domain;
		vec[1].iov_len  = strlen(domain) + 1;

		vec[2].iov_base = (void *)log_msg;
		vec[2].iov_len  = strlen(log_msg) + 1;

		ret = logger_kernel_write(vitals_logger->buf, vec, 3);
	}
	return ret;
}

/**
 * log_counter_to_vitals - add a counter message to vitals log buffer
 * @priority:	the Android priority of the message
 * @domain:	the domain of this message belong to
 * @program:	the vital record category name
 * @source:	the vital name
 * @key:	the counter name
 * @counter_value:the counter value
 * @metadata:	the metadata info
 * @type:	the type of vitals
 * @Returns:	0 on success, error code on failure
 */
int log_counter_to_vitals(enum android_log_priority priority,
			const char *domain, const char *program,
			const char *source, const char *key,
			long counter_value, const char *unit,
			const char *metadata, vitals_type type)
{
	char str[VITAL_ENTRY_MAX_PAYLOAD];
	char metadata_msg[VITAL_ENTRY_MAX_PAYLOAD];

	if (metadata != NULL && strlen(metadata))
		snprintf(metadata_msg, VITAL_ENTRY_MAX_PAYLOAD,
				 ",metadata=%s;DV;1", metadata);
	else
		metadata_msg[0] = '\0';
	/* format (program):(source):[key=(key);
	   DV;1,]counter=(counter_value);1,unit=(unit);
	   DV;1,metadata=(metadata);DV;1:HI */
	if (key != NULL) {
		snprintf(str, VITAL_ENTRY_MAX_PAYLOAD,
			"%s:%s:type=%d;DV;1,key=%s;DV;1,counter=%ld;CT;1,unit=%s;DV;1%s:HI",
			program, source, type,
			key, counter_value, unit,
			metadata_msg);
	} else {
		snprintf(str, VITAL_ENTRY_MAX_PAYLOAD,
			"%s:%s:type=%d;DV;1,counter=%ld;CT;1,unit=%s;DV;1%s:HI",
			program, source, type,
			counter_value, unit,
			metadata_msg);
	}
	return log_to_vitals(priority, domain, str);
}
EXPORT_SYMBOL(log_counter_to_vitals);

/**
 * log_timer_to_vitals - add a timer message to vitals log buffer
 * @priority:	the Android priority of the message
 * @domain:	the domain of this message belong to
 * @program:	the vital record category name
 * @source:	the vital name
 * @key:	the timer name
 * @timer_value:the timer value
 * @unit:	unit for the timer
 * @type:	the type of vitals.
 * @Returns:	0 on success, error code on failure
 */
int log_timer_to_vitals(enum android_log_priority priority,
			const char *domain, const char *program,
			const char *source, const char *key,
			long timer_value, const char *unit, vitals_type type)
{
	char str[VITAL_ENTRY_MAX_PAYLOAD];
	/* format (program):(source):[key=(key);
	   DV;1,]timer=(timer_value);1,unit=(unit);DV;1:HI */
	if (key != NULL) {
		snprintf(str, VITAL_ENTRY_MAX_PAYLOAD,
			"%s:%s:type=%d;DV;1,key=%s;DV;1,timer=%ld;TI;1,unit=%s;DV;1:HI",
			program, source, type,
			key, timer_value, unit);
	} else {
		snprintf(str, VITAL_ENTRY_MAX_PAYLOAD,
			"%s:%s:type=%d;DV;1,timer=%ld;TI;1,unit=%s;DV;1:HI",
			program, source, type,
			timer_value, unit);
	}
	return log_to_vitals(priority, domain, str);
}
EXPORT_SYMBOL(log_timer_to_vitals);


static struct amazon_logger *get_log_from_minor(int minor)
{
	struct amazon_logger *log;

	list_for_each_entry(log, &log_list, logs) {
		if (log->misc.minor == minor)
			return log;
	}
	return NULL;
}

/* find a cpu who has new data */
static int ring_buffer_find_cpu(struct ring_buffer *buffer)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		if (!ring_buffer_empty_cpu(buffer, cpu))
			return cpu;
	}
	return -1;
}

static int amazon_logger_open(struct inode *inode, struct file *file)
{
	struct amazon_logger *logger;

	logger = get_log_from_minor(MINOR(inode->i_rdev));
	if (!logger)
		return -ENODEV;

	file->private_data = logger;
	return 0;
}

/*
 * amazon_logger_read, the ring_buffer comsumer:
 * The logic is almost same as Google's logger_read:
 *  -supports O_NONBLOCK mode
 *  -If there are no log entries to read, blocks until log is written to
 *  -Atomically reads exactly one log entry
 *  -Will set errno to EINVAL if read buffer is insufficient to hold the
 *   logger entry.
 */
static ssize_t amazon_logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct amazon_logger *logger = file->private_data;
	int ret = 0;
	int cpu = -1;
	struct ring_buffer_event *event;
	const char *msg;
	size_t len = 0;
	struct logger_entry *header;

	if (!logger)
		return -ENODEV;

again:
	while (1) {
		/*
		 * if there is new data pending,
		 * break the loop to read out the data.
		 */
		if (!ring_buffer_empty(logger->buf))
			break;
		/*
		 * if no data with O_NONBLOCK mode
		 * return immediately.
		 */
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		/*
		 * if no data and with BLOCK mode,
		 * then need to block and wait for new data
		 */
		ret = ring_buffer_wait(logger->buf, RING_BUFFER_ALL_CPUS, false);
		if (ret)
			return ret;
		/*
		 * if waiting was interrupted by signal
		 * then return with EINTR.
		 */
		if (signal_pending(current))
			return -EINTR;
	}

	mutex_lock(&logger->mutex);
	cpu = ring_buffer_find_cpu(logger->buf);
	/*
	 * unlikely race case: someone else has read out all
	 * data from ring_buffer. goto wait again.
	 */
	if (unlikely(cpu < 0)) {
		mutex_unlock(&logger->mutex);
		goto again;
	}
	/* consume exactly one logger_entry(event) from the ring_buffer */
	event = ring_buffer_consume(logger->buf, cpu, NULL, NULL);
	if (event) {
		msg = ring_buffer_event_data(event);
		len = ring_buffer_event_length(event);
		header = (struct logger_entry *)msg;
		/*
		 * the event len may be aligned to 4 bytes, but we need
		 * to return the real msg length without alignment.
		 */
		len = min_t(size_t, len,
			sizeof(struct logger_entry) + header->len);

		if (count < len) {
			ret = -EINVAL;
			goto out;
		}
		if (copy_to_user(buf, msg, len)) {
			ret = -EFAULT;
			goto out;
		}
	} else {
		/* unlikely cases */
		pr_err("we should get data from cpu[%d]", cpu);
	}

	ret = len;
out:
	mutex_unlock(&logger->mutex);
	return ret;
}

static int amazon_logger_release(struct inode *ignored, struct file *file)
{
	/* currently nothing to do */
	return 0;
}

/*
 * amazon_logger_poll with ring_buffer's poll wait api.
 * poll_wait exit once any cpu's ring_buffer is not empty.
 *
 */
static unsigned int amazon_logger_poll(struct file *file, poll_table *wait)
{
	int ret;
	struct amazon_logger *logger = file->private_data;

	if (!logger)
		return -ENODEV;
	ret = ring_buffer_poll_wait(logger->buf,
			RING_BUFFER_ALL_CPUS, file, wait);
	return ret;
}

static const struct file_operations amazon_logger_fops = {
	.owner = THIS_MODULE,
	.read = amazon_logger_read,
	.poll = amazon_logger_poll,
	.open = amazon_logger_open,
	.release = amazon_logger_release,
};

static struct amazon_logger *create_log(char *log_name, int size)
{
	struct amazon_logger *log;
	int ret;

	log = kzalloc(sizeof(struct amazon_logger), GFP_KERNEL);
	if (log == NULL) {
		pr_err("failed to kzalloc %s\n", log_name);
		return ERR_PTR(-ENOMEM);
	}

	log->misc.minor = MISC_DYNAMIC_MINOR;
	log->misc.name = kstrdup(log_name, GFP_KERNEL);
	if (log->misc.name == NULL) {
		pr_err("failed to malloc log_name %s", log_name);
		ret = -ENOMEM;
		goto out_free_log;
	}

	log->misc.fops = &amazon_logger_fops;
	log->misc.parent = NULL;

	log->buf = ring_buffer_alloc(size, RB_FL_OVERWRITE);
	if (log->buf == NULL) {
		pr_err("failed to alloc ring_buffer %s\n", log_name);
		ret = -ENOMEM;
		goto out_free_name;
	}

	mutex_init(&log->mutex);
	INIT_LIST_HEAD(&log->logs);
	list_add_tail(&log->logs, &log_list);

	/* finally, initialize the misc device for this log */
	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		pr_err("failed to register misc device for log '%s'!\n",
				log->misc.name);
		ring_buffer_free(log->buf);
		goto out_free_name;
	}
	/* turn on ring_buffer for writing */
	ring_buffer_record_on(log->buf);
	pr_info("created amazon_logger: %s\n", log->misc.name);

	return log;

out_free_name:
	kfree(log->misc.name);
out_free_log:
	kfree(log);
	return ERR_PTR(ret);
}

static int __init amazon_logger_init(void)
{
	metrics_logger = create_log(AMAZON_LOGGER_LOG_METRICS,
						AMAZON_METRICS_BUF_SIZE);
	if (IS_ERR_OR_NULL(metrics_logger)) {
		pr_err("failed to create_log for metrics\n");
		goto out;
	}

	vitals_logger = create_log(AMAZON_LOGGER_LOG_VITALS,
						AMAZON_VITALS_BUF_SIZE);
	if (IS_ERR_OR_NULL(vitals_logger)) {
		pr_err("failed to create_log for vitals\n");
		goto out;
	}

	metrics_init = 1;
	return 0;

out:
	return -1;
}

static void __exit amazon_logger_exit(void)
{
	struct amazon_logger *current_log, *next_log;

	list_for_each_entry_safe(current_log, next_log, &log_list, logs) {
		/* we have to delete all the entry inside log_list */
		misc_deregister(&current_log->misc);
		ring_buffer_free(current_log->buf);
		kfree(current_log->misc.name);
		list_del(&current_log->logs);
		kfree(current_log);
	}

}

device_initcall(amazon_logger_init);
module_exit(amazon_logger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Amazon Logger");
