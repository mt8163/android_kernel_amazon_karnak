/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
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

#define pr_fmt(fmt) "logger: " fmt

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/aio.h>
#include <linux/irq_work.h>
#include "logger.h"

#include <asm/ioctls.h>

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>

static int metrics_init;
#define VITAL_ENTRY_MAX_PAYLOAD 512
#endif

static int s_fake_read;
module_param_named(fake_read, s_fake_read, int, 0660);

#ifndef CONFIG_LOGCAT_SIZE
#define CONFIG_LOGCAT_SIZE 256
#endif

/**
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 * @buffer:	The actual ring buffer
 * @misc:	The "misc" device representing the log
 * @wq:		The wait queue for @readers
 * @readers:	This log's readers
 * @mutex:	The mutex that protects the @buffer
 * @sem:    The semphore that protects the kernel @buffer
 * @is_mutex: mutex or semphore flag.
 * @w_off:	The current write head offset
 * @head:	The head, or location that readers start reading at.
 * @size:	The size of the log
 * @logs:	The list of log channels
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char		*buffer;
	struct miscdevice	misc;
	wait_queue_head_t	wq;
	struct list_head	readers;
#ifdef CONFIG_AMAZON_KLOG_CONSOLE
	union {
		struct mutex		mutex;
		struct semaphore	sem;
	};
	int is_mutex;
#else
	struct mutex		mutex;
#endif
	size_t			w_off;
	size_t			head;
	size_t			size;
	struct list_head	logs;
};

#ifdef CONFIG_AMAZON_KLOG_CONSOLE
#define logger_lock(log) \
	do { \
		if(log->is_mutex) \
			mutex_lock(&log->mutex); \
		else \
			down(&log->sem); \
	} while(0)

#define logger_unlock(log) \
	do { \
		if(log->is_mutex) \
			mutex_unlock(&log->mutex); \
		else \
			up(&log->sem); \
	} while(0)

#define init_logger_lock(log) \
	do { \
		if(log->is_mutex) \
			mutex_init(&log->mutex); \
		else \
			sema_init(&log->sem, 1); \
	} while(0)

#else

#define logger_lock(log) mutex_lock(&log->mutex)

#define logger_unlock(log) mutex_unlock(&log->mutex);

#define init_logger_lock(log) mutex_init(&log->mutex);

#endif

static LIST_HEAD(log_list);

/**
 * struct logger_reader - a logging device open for reading
 * @log:	The associated log
 * @list:	The associated entry in @logger_log's list
 * @r_off:	The current read head offset.
 * @r_all:	Reader can read all entries
 * @r_ver:	Reader ABI version
 * @missing_bytes: android log missing warning
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;
	struct list_head	list;
	size_t			r_off;
	bool			r_all;
	int			r_ver;
    size_t      missing_bytes;
};

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
static size_t logger_offset(struct logger_log *log, size_t n)
{
	return n & (log->size - 1);
}


/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 *	1) Need to quickly obtain the associated log during an I/O operation
 *	2) Readers need to maintain state (logger_reader)
 *	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		return reader->log;
	} else
		return file->private_data;
}

/*
 * get_entry_header - returns a pointer to the logger_entry header within
 * 'log' starting at offset 'off'. A temporary logger_entry 'scratch' must
 * be provided. Typically the return value will be a pointer within
 * 'logger->buf'.  However, a pointer to 'scratch' may be returned if
 * the log entry spans the end and beginning of the circular buffer.
 */
static struct logger_entry *get_entry_header(struct logger_log *log,
		size_t off, struct logger_entry *scratch)
{
	size_t len = min(sizeof(struct logger_entry), log->size - off);
	if (len != sizeof(struct logger_entry)) {
		memcpy(((void *) scratch), log->buffer + off, len);
		memcpy(((void *) scratch) + len, log->buffer,
			sizeof(struct logger_entry) - len);
		return scratch;
	}

	return (struct logger_entry *) (log->buffer + off);
}

/*
 * get_entry_msg_len - Grabs the length of the message of the entry
 * starting from from 'off'.
 *
 * An entry length is 2 bytes (16 bits) in host endian order.
 * In the log, the length does not include the size of the log entry structure.
 * This function returns the size including the log entry structure.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_msg_len(struct logger_log *log, size_t off)
{
	struct logger_entry scratch;
	struct logger_entry *entry;

	entry = get_entry_header(log, off, &scratch);
	return entry->len;
}

static size_t get_user_hdr_len(int ver)
{
	if (ver < 2)
		return sizeof(struct user_logger_entry_compat);
	else
		return sizeof(struct logger_entry);
}

static ssize_t copy_header_to_user(int ver, struct logger_entry *entry,
					 char __user *buf)
{
	void *hdr;
	size_t hdr_len;
	struct user_logger_entry_compat v1;

	if (ver < 2) {
		v1.len      = entry->len;
		v1.__pad    = 0;
		v1.pid      = entry->pid;
		v1.tid      = entry->tid;
		v1.sec      = entry->sec;
		v1.nsec     = entry->nsec;
		hdr         = &v1;
		hdr_len     = sizeof(struct user_logger_entry_compat);
	} else {
		hdr         = entry;
		hdr_len     = sizeof(struct logger_entry);
	}

	return copy_to_user(buf, hdr, hdr_len);
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	struct logger_entry scratch;
	struct logger_entry *entry;
	size_t len;
	size_t msg_start;

	/*
	 * First, copy the header to userspace, using the version of
	 * the header requested
	 */
	entry = get_entry_header(log, reader->r_off, &scratch);
	if (copy_header_to_user(reader->r_ver, entry, buf))
		return -EFAULT;

	count -= get_user_hdr_len(reader->r_ver);
	buf += get_user_hdr_len(reader->r_ver);
	msg_start = logger_offset(log,
		reader->r_off + sizeof(struct logger_entry));

	/*
	 * We read from the msg in two disjoint operations. First, we read from
	 * the current msg head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - msg_start);
	if (copy_to_user(buf, log->buffer + msg_start, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(log, reader->r_off +
		sizeof(struct logger_entry) + count);

	return count + get_user_hdr_len(reader->r_ver);
}

/*
 * get_next_entry_by_uid - Starting at 'off', returns an offset into
 * 'log->buffer' which contains the first entry readable by 'euid'
 */
static size_t get_next_entry_by_uid(struct logger_log *log,
		size_t off, kuid_t euid)
{
	while (off != log->w_off) {
		struct logger_entry *entry;
		struct logger_entry scratch;
		size_t next_len;

		entry = get_entry_header(log, off, &scratch);

		if (uid_eq(entry->euid, euid))
			return off;

		next_len = sizeof(struct logger_entry) + entry->len;
		off = logger_offset(log, off + next_len);
	}

	return off;
}

static ssize_t logger_fake_message(struct logger_log *log, struct logger_reader *reader, char __user *buf, const char *fmt, ...)
{
	int len, header_size, entry_len;
        char message[256], *tag;
	va_list ap;
	struct logger_entry *current_entry, scratch;

	header_size = get_user_hdr_len(reader->r_ver);

	current_entry = get_entry_header(log, reader->r_off, &scratch);

	memset(message, 0, header_size);
	va_start(ap, fmt);
        len = vsnprintf(message + header_size + 5, sizeof(message) - (header_size + 5), fmt, ap);
	va_end(ap);

	entry_len = 5 + len + 1/* message size */;
	tag = message + header_size;
	tag[0] = 0x5;
	tag[1] = 'A';
	tag[2] = 'E';
	tag[3] = 'E';
	tag[4] = 0;

	switch (reader->r_ver) {
	case 1: {
		struct user_logger_entry_compat *entryp = (struct user_logger_entry_compat *)message;
		entryp->sec = current_entry->sec;
		entryp->nsec = current_entry->nsec;
		entryp->len = entry_len;
		break;
	}
	case 2: {
		struct logger_entry *entryp = (struct logger_entry *)message;
		entryp->sec = current_entry->sec;
		entryp->nsec = current_entry->nsec;
		entryp->len = entry_len;
		entryp->hdr_size = header_size;
		break;
	}
	default:
		/* FIXME: ? */
		return 0;
		break;
	}

        if (copy_to_user(buf, message, entry_len + header_size))
		return -EFAULT;

	return entry_len + header_size;
}

/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 *	- O_NONBLOCK works
 *	- If there are no log entries to read, blocks until log is written to
 *	- Atomically reads exactly one log entry
 *
 * Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);

start:
	while (1) {
		logger_lock(log);

		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		ret = (log->w_off == reader->r_off);
		logger_unlock(log);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

	logger_lock(log);

	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		logger_unlock(log);
		goto start;
	}

	if (unlikely(s_fake_read)) {
		ret = logger_fake_message(log, reader, buf, "Fake read return string.");
		goto out;
	}

	/* for android log missing warning { */
	if (reader->missing_bytes > 0) {
		ret = logger_fake_message(log, reader, buf, "some logs have been lost (%u bytes estimated)", reader->missing_bytes);
		reader->missing_bytes = 0;
		goto out;
	}

	/* get the size of the next entry */
	ret = get_user_hdr_len(reader->r_ver) +
		get_entry_msg_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

out:
	logger_unlock(log);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = sizeof(struct logger_entry) +
			get_entry_msg_len(log, off);
		off = logger_offset(log, off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * is_between - is a < c < b, accounting for wrapping of a, b, and c
 *    positions in the buffer
 *
 * That is, if a<b, check for c between a and b
 * and if a>b, check for c outside (not between) a and b
 *
 * |------- a xxxxxxxx b --------|
 *               c^
 *
 * |xxxxx b --------- a xxxxxxxxx|
 *    c^
 *  or                    c^
 */
static inline int is_between(size_t a, size_t b, size_t c)
{
	if (a < b) {
		/* is c between a and b? */
		if (a < c && c <= b)
			return 1;
	} else {
		/* is c outside of b through a? */
		if (c <= b || a < c)
			return 1;
	}

	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(log, old + len);
	struct logger_reader *reader;

	if (is_between(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
		if (is_between(old, new, reader->r_off)) {
			size_t old_r_off = reader->r_off;
			reader->r_off = get_next_entry(log, reader->r_off, len);
			if (reader->r_off >= old_r_off) {
				reader->missing_bytes += (reader->r_off - old_r_off);
			}
			else {
				reader->missing_bytes += (reader->r_off + (log->size - old_r_off));
			}
		}
}

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->mutex.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

	log->w_off = logger_offset(log, log->w_off + count);

}

/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->mutex.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log,
				      const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			/*
			 * Note that by not updating w_off, this abandons the
			 * portion of the new entry that *was* successfully
			 * copied, just above.  This is intentional to avoid
			 * message corruption from missing fragments.
			 */
			return -EFAULT;

	log->w_off = logger_offset(log, log->w_off + count);

	return count;
}

/*
 * logger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
static ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	size_t orig;
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;


		// android default timestamp
		//now = current_kernel_time();
		getnstimeofday(&now);
		header.pid = current->tgid;
		header.tid = current->pid;
		header.sec = now.tv_sec;
		header.nsec = now.tv_nsec;
		header.euid = current_euid();
		header.len = min_t(size_t, iov_length(iov, nr_segs), LOGGER_ENTRY_MAX_PAYLOAD);
		header.hdr_size = sizeof(struct logger_entry);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	logger_lock(log);

	orig = log->w_off;

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);
		if (unlikely(nr < 0)) {
			log->w_off = orig;
			logger_unlock(log);
			return nr;
		}

		iov++;
		ret += nr;
	}

	logger_unlock(log);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return ret;
}

static struct logger_log *get_log_from_minor(int minor)
{
	struct logger_log *log;

	list_for_each_entry(log, &log_list, logs)
		if (log->misc.minor == minor)
			return log;
	return NULL;
}

/*
#ifdef CONFIG_AMAZON_METRICS_LOG
static struct logger_log *get_log_from_name(char* name)
{
    struct logger_log *log;
    if (0 == name) {
        return NULL;
    }
    list_for_each_entry(log, &log_list, logs)
        if (0 == strcmp(log->misc.name, name))
            return log;
    return NULL;
}
#endif
*/

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		reader->r_ver = 1;
		reader->r_all = in_egroup_p(inode->i_gid) ||
			capable(CAP_SYSLOG);
		reader->missing_bytes = 0;

		INIT_LIST_HEAD(&reader->list);

		logger_lock(log);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		logger_unlock(log);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		struct logger_log *log = reader->log;

		logger_lock(log);
		list_del(&reader->list);
		logger_unlock(log);

		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	logger_lock(log);
	if (!reader->r_all)
		reader->r_off = get_next_entry_by_uid(log,
			reader->r_off, current_euid());

	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	logger_unlock(log);

	return ret;
}

static long logger_set_version(struct logger_reader *reader, void __user *arg)
{
	int version;
	if (copy_from_user(&version, arg, sizeof(int)))
		return -EFAULT;

	if ((version < 1) || (version > 2))
		return -EINVAL;

	reader->r_ver = version;
	return 0;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -EINVAL;
	void __user *argp = (void __user *) arg;

	logger_lock(log);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;

		if (!reader->r_all)
			reader->r_off = get_next_entry_by_uid(log,
				reader->r_off, current_euid());

		if (log->w_off != reader->r_off)
			ret = get_user_hdr_len(reader->r_ver) +
				get_entry_msg_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		if (!(in_egroup_p(file->f_dentry->d_inode->i_gid) ||
				capable(CAP_SYSLOG))) {
			ret = -EPERM;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;
		ret = 0;
		break;
	case LOGGER_GET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = reader->r_ver;
		break;
	case LOGGER_SET_VERSION:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		ret = logger_set_version(reader, argp);
		break;
	}

	logger_unlock(log);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.aio_write = logger_aio_write,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};

static void logger_kernel_write(struct logger_log *log, const struct iovec *iov, unsigned long nr_segs)
{
	struct logger_entry header;
	struct timespec now;
	unsigned long count = nr_segs;
	size_t total_len = 0;

	if (log == NULL)
		return;

	while (count-- > 0)
		total_len += iov[count].iov_len;

	now = current_kernel_time();

	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.len = min_t(size_t, total_len, LOGGER_ENTRY_MAX_PAYLOAD);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return;

	logger_lock(log);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	total_len = 0;

	while (nr_segs-- > 0) {
		size_t len;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - total_len);

		/* write out this segment's payload */
		do_write_log(log, iov->iov_base, len);

		iov++;
		total_len += len;
	}

	logger_unlock(log);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);
}

/*
 * Log size must must be a power of two, and greater than
 * (LOGGER_ENTRY_MAX_PAYLOAD + sizeof(struct logger_entry)).
 */
static int __init create_log(char *log_name, int size)
{
	int ret = 0;
	struct logger_log *log;
	unsigned char *buffer;

	buffer = vmalloc(size);
	if (buffer == NULL)
		return -ENOMEM;

	log = kzalloc(sizeof(struct logger_log), GFP_KERNEL);
	if (log == NULL) {
		ret = -ENOMEM;
		goto out_free_buffer;
	}
	log->buffer = buffer;

	log->misc.minor = MISC_DYNAMIC_MINOR;
	log->misc.name = kstrdup(log_name, GFP_KERNEL);
	if (log->misc.name == NULL) {
		ret = -ENOMEM;
		goto out_free_log;
	}

	log->misc.fops = &logger_fops;
	log->misc.parent = NULL;

	init_waitqueue_head(&log->wq);
	INIT_LIST_HEAD(&log->readers);
#ifdef CONFIG_AMAZON_KLOG_CONSOLE
	log->is_mutex = strncmp(log_name, LOGGER_LOG_KERNEL, strlen(LOGGER_LOG_KERNEL));
#endif
	init_logger_lock(log);
	log->w_off = 0;
	log->head = 0;
	log->size = size;

	INIT_LIST_HEAD(&log->logs);
	list_add_tail(&log->logs, &log_list);

	/* finally, initialize the misc device for this log */
	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		pr_err("failed to register misc device for log '%s'!\n",
				log->misc.name);
		goto out_free_log;
	}

	pr_info("created %luK log '%s'\n",
		(unsigned long) log->size >> 10, log->misc.name);

	return 0;

out_free_log:
	kfree(log);

out_free_buffer:
	vfree(buffer);
	return ret;
}

#ifdef CONFIG_AMAZON_KLOG_CONSOLE
#define KERNEL_DOMAIN  "Kernel"
#define ANDROID_LOG_INFO   (4)

struct kmsg_write_priv {
	struct logger_log *log;
	struct irq_work kmsg_write_work;
};


static struct logger_log *kmsg_log;

#ifdef CONFIG_AMAZON_KLOG_CONSOLE_DEBUG
static long long kmsg_delayed_count;
static ssize_t kmsg_delayed_count_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", kmsg_delayed_count);
}
static struct kobj_attribute kmsg_delayed_count_attr =
	__ATTR_RO(kmsg_delayed_count);

static long long kmsg_failed_count;
static ssize_t kmsg_failed_count_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", kmsg_failed_count);
}
static struct kobj_attribute kmsg_failed_count_attr =
	__ATTR_RO(kmsg_failed_count);

static long long kmsg_total_count;
static ssize_t kmsg_total_count_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", kmsg_total_count);
}
static struct kobj_attribute kmsg_total_count_attr =
	__ATTR_RO(kmsg_total_count);

static struct attribute *kmsg_attrs[] = {
	&kmsg_delayed_count_attr.attr,
	&kmsg_total_count_attr.attr,
	&kmsg_failed_count_attr.attr,
	NULL
};

static struct attribute_group kmsg_attr_group = {
	.name = "kmsg",
	.attrs = kmsg_attrs,
};
#endif

/* buffer for kernel log when it cannot get semphore */
#define KERNEL_LOG_BUF_LEN (1 << CONFIG_LOG_BUF_SHIFT)
static char kernel_log_buf[KERNEL_LOG_BUF_LEN] __aligned(8);
static char *klog_buf = kernel_log_buf;
static u32 klog_buf_len = KERNEL_LOG_BUF_LEN;
static DEFINE_RAW_SPINLOCK(klog_buf_lock);
/* index and sequence number of the first record stored in the buffer */
static u64 log_first_seq;
static u32 log_first_idx;
/* index and sequence number of the next record to store in the buffer */
static u64 log_next_seq;
static u32 log_next_idx;

/* get record by index; idx must point to valid msg */
static struct logger_entry *log_from_idx(u32 idx)
{
	struct logger_entry *msg = (struct logger_entry *)(klog_buf + idx);

	/*
	 * A length == 0 record is the end of buffer marker. Wrap around and
	 * read the message at the start of the buffer.
	 */
	if (!msg->len)
		return (struct logger_entry *)klog_buf;
	return msg;
}

/* get next record; idx must point to valid msg */
static u32 log_next(u32 idx)
{
	struct logger_entry *msg = (struct logger_entry *)(klog_buf + idx);

	/* length == 0 indicates the end of the buffer; wrap */
	/*
	 * A length == 0 record is the end of buffer marker. Wrap around and
	 * read the message at the start of the buffer as *this* one, and
	 * return the one after that.
	 */
	if (!msg->len) {
		msg = (struct logger_entry *)klog_buf;
		return msg->len;
	}
	return idx + msg->len;
}

/*
 * Check whether there is enough free space for the given message.
 *
 * The same values of first_idx and next_idx mean that the buffer
 * is either empty or full.
 *
 * If the buffer is empty, we must respect the position of the indexes.
 * They cannot be reset to the beginning of the buffer.
 */
static int logbuf_has_space(u32 msg_size, bool empty)
{
	u32 free;

	if (log_next_idx > log_first_idx || empty)
		free = max(klog_buf_len - log_next_idx, log_first_idx);
	else
		free = log_first_idx - log_next_idx;

	/*
	 * We need space also for an empty header that signalizes wrapping
	 * of the buffer.
	 */
	return free >= msg_size + sizeof(struct logger_entry);
}

static int log_make_free_space(u32 msg_size)
{
	while (log_first_seq < log_next_seq) {
		if (logbuf_has_space(msg_size, false))
			return 0;
		/* drop old messages until we have enough contiguous space */
		log_first_idx = log_next(log_first_idx);
		log_first_seq++;
	}

	/* sequence numbers are equal, so the log buffer is empty */
	if (logbuf_has_space(msg_size, true))
		return 0;

	return -ENOMEM;
}

/* insert record into the buffer, discard old ones, update heads */
static int logger_kmsg_drain(struct logger_entry *header, const char *text, u16 text_len)
{
	unsigned long flags;
	struct logger_entry *msg;
	u16 payload_len = min_t(size_t, text_len, LOGGER_ENTRY_MAX_PAYLOAD);
	u32 size = payload_len + sizeof(struct logger_entry);
	raw_spin_lock_irqsave(&klog_buf_lock, flags);
	/* if has no space */
	if (log_make_free_space(size)) {
		raw_spin_unlock_irqrestore(&klog_buf_lock, flags);
		return 0;
	}
	if (log_next_idx + size + sizeof(struct logger_entry) > klog_buf_len) {
		/*
		 * This message + an additional empty header does not fit
		 * at the end of the buffer. Add an empty header with len == 0
		 * to signify a wrap around.
		 */
		memset(klog_buf + log_next_idx, 0, sizeof(struct logger_entry));
		log_next_idx = 0;
	}
	/* fill message */
	msg = (struct logger_entry *)(klog_buf + log_next_idx);
	msg->pid = header->pid;
	msg->tid = header->tid;
	msg->sec = header->sec;
	msg->nsec = header->nsec;
	msg->len = size; /* contains header*/
	memcpy(klog_buf + log_next_idx + sizeof(struct logger_entry), text, payload_len);
	/* insert message */
	log_next_idx += size;
	log_next_seq++;
	raw_spin_unlock_irqrestore(&klog_buf_lock, flags);

	return payload_len;
}

static void logger_kmsg_write_delayed(struct logger_entry *log_msg)
{
	struct logger_log *log = kmsg_log;
	struct logger_entry header;
	unsigned long count = 3;
	size_t total_len = 0;
	struct iovec iov[3];
	struct iovec *piov = &iov[0];
	unsigned char log_level = ANDROID_LOG_INFO;

	iov[0].iov_base = (unsigned char *)&log_level;
	iov[0].iov_len  = 1;

	iov[1].iov_base = (void *)KERNEL_DOMAIN;
	iov[1].iov_len  = strlen(KERNEL_DOMAIN) + 1;

	iov[2].iov_base = (void *)((char *)log_msg + sizeof(struct logger_entry));
	iov[2].iov_len  = log_msg->len - sizeof(struct logger_entry);

	while (count-- > 0) {
		total_len += iov[count].iov_len;
	}

	header.pid = log_msg->pid;
	header.tid = log_msg->tid;
	header.sec = log_msg->sec;
	header.nsec = log_msg->nsec;
	header.len = min_t(size_t, total_len, LOGGER_ENTRY_MAX_PAYLOAD);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return;

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	total_len = 0;

	count = 3;
	while (count-- > 0) {
		size_t len;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, piov->iov_len, header.len - total_len);

		/* write out this segment's payload */
		do_write_log(log, piov->iov_base, len);

		piov++;
		total_len += len;
	}
}

static void logger_kmsg_drain_delayed(struct logger_log *log)
{
	unsigned long flags;
	static char kbuffer[sizeof(struct logger_log) + LOGGER_ENTRY_MAX_PAYLOAD];
	struct logger_entry *msg;
	for (;;) {
		raw_spin_lock_irqsave(&klog_buf_lock, flags);
		if (log_first_seq == log_next_seq) {
			raw_spin_unlock_irqrestore(&klog_buf_lock, flags);
			break;
		}

		msg = log_from_idx(log_first_idx);

		log_first_idx = log_next(log_first_idx);
		log_first_seq++;

		memcpy(kbuffer, msg, msg->len);
		raw_spin_unlock_irqrestore(&klog_buf_lock, flags);

		logger_kmsg_write_delayed((struct logger_entry *)kbuffer);
	}
}

static void kmsg_write_work_func(struct irq_work *irq_work)
{
	struct kmsg_write_priv *priv = container_of(irq_work, struct kmsg_write_priv,
							kmsg_write_work);
	/* wake up any blocked readers */
	wake_up_interruptible(&priv->log->wq);
}

static DEFINE_PER_CPU(struct kmsg_write_priv, priv_data) = {
	.kmsg_write_work = {
		.func = kmsg_write_work_func,
		.flags = IRQ_WORK_LAZY,
	},
};

void logger_kmsg_write(const char *log_msg, size_t len)
{
	static struct logger_log *log;
	struct logger_entry header;
	struct timespec now;
	unsigned long count = 3;
	size_t total_len = 0;
	struct iovec iov[3];
	struct iovec *piov = &iov[0];
	unsigned char log_level = ANDROID_LOG_INFO;

	/* get the main log handler */
	if (!kmsg_log) {
		list_for_each_entry(log, &log_list, logs)
			if (0 == strcmp(log->misc.name, LOGGER_LOG_KERNEL)) {
				kmsg_log = log;
				break;
				}

		if (!kmsg_log)
			return;
	}

	log = kmsg_log;
	iov[0].iov_base = (unsigned char *)&log_level;
	iov[0].iov_len  = 1;

	iov[1].iov_base = (void *)KERNEL_DOMAIN;
	iov[1].iov_len  = strlen(KERNEL_DOMAIN) + 1;

	iov[2].iov_base = (void *)log_msg;
	iov[2].iov_len  = len;

	while (count-- > 0) {
		total_len += iov[count].iov_len;
	}

	now = current_kernel_time();

	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.len = min_t(size_t, total_len, LOGGER_ENTRY_MAX_PAYLOAD);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return;
#ifdef CONFIG_AMAZON_KLOG_CONSOLE_DEBUG
	kmsg_total_count++;
#endif

	/*
	 * We need to lock log->sem here, however, we can not call
	 * down(&log->sem) directly, due to the following
	 * function call sequence in console_unlock():
	 *
	 * -> raw_spin_lock_irqsave(&logbuf_lock, flags)
	 * ...
	 * -> raw_spin_unlock(&logbuf_lock)
	 * ...
	 * -> call_console_drivers()
	 *    -> klog_console_write()
	 *       -> logger_kmsg_write()
	 * ...
	 * -> local_irq_restore(flags)
	 *
	 * So if we call down(&log->sem) directly, might_sleep()
	 * in down() will raise in_atomic() warnings, instead, we
	 * call down_trylock() to avoid that, and if we fail locking it,
	 * we write it to a ring buffer. We count
	 * on the next call to logger_kmsg_write() would succeed on this
	 * down_trylock() call, then we call logger_kmsg_drain_delayed()
	 * to actually drain messages in the ring buffer.
	 */
	if (down_trylock(&log->sem)) {
		int ret = logger_kmsg_drain(&header, log_msg, len);

#ifdef CONFIG_AMAZON_KLOG_CONSOLE_DEBUG
		if (!ret) {
			kmsg_failed_count++;
		} else {
			kmsg_delayed_count++;
		}
#endif
		return;
	}

	/* drain the delayed messages (with log->sem locked) */
	logger_kmsg_drain_delayed(log);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	total_len = 0;

	count = 3;
	while (count-- > 0) {
		size_t len;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, piov->iov_len, header.len - total_len);

		/* write out this segment's payload */
		do_write_log(log, piov->iov_base, len);

		piov++;
		total_len += len;
	}

	logger_unlock(log);

	__get_cpu_var(priv_data).log = log;
	irq_work_queue(&(__get_cpu_var(priv_data).kmsg_write_work));
}
#endif

static int __init logger_init(void)
{
	int ret;

#ifndef CONFIG_AMAZON_LOGD
	ret = create_log(LOGGER_LOG_MAIN, __MAIN_BUF_SIZE);
	if (unlikely(ret))
		goto out;

	ret = create_log(LOGGER_LOG_EVENTS, __EVENTS_BUF_SIZE);
	if (unlikely(ret))
		goto out;

	ret = create_log(LOGGER_LOG_RADIO, __RADIO_BUF_SIZE);
	if (unlikely(ret))
		goto out;

	ret = create_log(LOGGER_LOG_SYSTEM, __SYSTEM_BUF_SIZE);
	if (unlikely(ret))
		goto out;
#endif /* CONFIG_AMAZON_LOGD */

#ifdef CONFIG_AMAZON_KLOG_CONSOLE
	ret = create_log(LOGGER_LOG_KERNEL, __KERNEL_BUF_SIZE);
	if (unlikely(ret))
		goto out;
#ifdef CONFIG_AMAZON_KLOG_CONSOLE_DEBUG
	ret = sysfs_create_group(kernel_kobj, &kmsg_attr_group);
	if (ret)
		goto out;
#endif
#endif

#ifdef CONFIG_AMAZON_METRICS_LOG
	ret = create_log(LOGGER_LOG_METRICS, __METRICS_BUF_SIZE);
	if (unlikely(ret))
		goto out;

	metrics_init = 1;
#endif

#ifdef CONFIG_AMAZON_LOG
#ifndef CONFIG_AMAZON_LOGD
    ret = create_log(LOGGER_LOG_AMAZON_MAIN, 256*1024);
    if (unlikely(ret))
        goto out;
#endif /* CONFIG_AMAZON_LOGD */

	ret = create_log(LOGGER_LOG_AMAZON_VITALS, __VITALS_BUF_SIZE);
	if (unlikely(ret))
		goto out;
#endif
out:
	return ret;
}

static void __exit logger_exit(void)
{
	struct logger_log *current_log, *next_log;

	list_for_each_entry_safe(current_log, next_log, &log_list, logs) {
		/* we have to delete all the entry inside log_list */
		misc_deregister(&current_log->misc);
		vfree(current_log->buffer);
		kfree(current_log->misc.name);
		list_del(&current_log->logs);
		kfree(current_log);
	}
}

int panic_dump_main(char *buf, size_t size) {
		static size_t offset = 0; //offset of log buffer
		static int isFirst = 0;
		size_t len = 0;
		size_t distance = 0;
		size_t realsize = 0;
    struct logger_log *log;
    struct logger_log log_main;
    log_main.buffer = NULL;
    log_main.size = 0;
    log_main.head = 0;
    log_main.w_off = 0;
	list_for_each_entry(log, &log_list, logs)
		if (strncmp(log->misc.name, LOGGER_LOG_MAIN, strlen(LOGGER_LOG_MAIN)) == 0)
            log_main = *log;

	if (isFirst == 0){
		offset = log_main.head;
		isFirst++;
	}
	distance = (log_main.w_off + log_main.size - offset) & (log_main.size -1);
	//printk("offset = %d, w_off = %d, head = %d distance = %d\n", offset, log_main.w_off, log_main.head, distance);
	if(distance > size)
		realsize = size;
	else
		realsize = distance;
	len = min(realsize, log_main.size - offset);
	memcpy(buf, log_main.buffer + offset, len);
	if (realsize != len)
		memcpy(buf + len, log_main.buffer, realsize - len);
	offset += realsize;
	offset &= (log_main.size - 1);
	return realsize;
}

int panic_dump_events(char *buf, size_t size) {
		static size_t offset = 0; //offset of log buffer
		static int isFirst = 0;
		size_t len = 0;
		size_t distance = 0;
		size_t realsize = 0;
    struct logger_log *log;
    struct logger_log log_events;
    log_events.buffer = NULL;
    log_events.size = 0;
    log_events.head = 0;
    log_events.w_off = 0;
	list_for_each_entry(log, &log_list, logs)
		if (strncmp(log->misc.name, LOGGER_LOG_EVENTS, strlen(LOGGER_LOG_EVENTS)) == 0)
            log_events = *log;

	if (isFirst == 0){
		offset = log_events.head;
		isFirst++;
	}
	distance = (log_events.w_off + log_events.size - offset) & (log_events.size -1);
	//printk("offset = %d, w_off = %d, head = %d distance = %d\n", offset, log_events.w_off, log_events.head, distance);
	if(distance > size)
		realsize = size;
	else
		realsize = distance;
	len = min(realsize, log_events.size - offset);
	memcpy(buf, log_events.buffer + offset, len);
	if (realsize != len)
		memcpy(buf + len, log_events.buffer, realsize - len);
	offset += realsize;
	offset &= (log_events.size - 1);
	return realsize;
}

int panic_dump_radio(char *buf, size_t size) {
		static size_t offset = 0; //offset of log buffer
		static int isFirst = 0;
		size_t len = 0;
		size_t distance = 0;
		size_t realsize = 0;
    struct logger_log *log;
    struct logger_log log_radio;
    log_radio.buffer = NULL;
    log_radio.size = 0;
    log_radio.head = 0;
    log_radio.w_off = 0;
	list_for_each_entry(log, &log_list, logs)
		if (strncmp(log->misc.name, LOGGER_LOG_RADIO, strlen(LOGGER_LOG_RADIO)) == 0)
            log_radio = *log;

	if (isFirst == 0){
		offset = log_radio.head;
		isFirst++;
	}
	distance = (log_radio.w_off + log_radio.size - offset) & (log_radio.size -1);
	//printk("offset = %d, w_off = %d, head = %d distance = %d\n", offset, log_radio.w_off, log_radio.head, distance);
	if(distance > size)
		realsize = size;
	else
		realsize = distance;
	len = min(realsize, log_radio.size - offset);
	memcpy(buf, log_radio.buffer + offset, len);
	if (realsize != len)
		memcpy(buf + len, log_radio.buffer, realsize - len);
	offset += realsize;
	offset &= (log_radio.size - 1);
	return realsize;
}

int panic_dump_system(char *buf, size_t size) {
		static size_t offset = 0; //offset of log buffer
		static int isFirst = 0;
		size_t len = 0;
		size_t distance = 0;
		size_t realsize = 0;
    struct logger_log *log;
    struct logger_log log_system;
    log_system.buffer = NULL;
    log_system.size = 0;
    log_system.head = 0;
    log_system.w_off = 0;
	list_for_each_entry(log, &log_list, logs)
		if (strncmp(log->misc.name, LOGGER_LOG_SYSTEM, strlen(LOGGER_LOG_SYSTEM)) == 0)
            log_system = *log;

	if (isFirst == 0){
		offset = log_system.head;
		isFirst++;
	}
	distance = (log_system.w_off + log_system.size - offset) & (log_system.size -1);
	//printk("offset = %d, w_off = %d, head = %d distance = %d\n", offset, log_system.w_off, log_system.head, distance);
	if(distance > size)
		realsize = size;
	else
		realsize = distance;
	len = min(realsize, log_system.size - offset);
	memcpy(buf, log_system.buffer + offset, len);
	if (realsize != len)
		memcpy(buf + len, log_system.buffer, realsize - len);
	offset += realsize;
	offset &= (log_system.size - 1);
	return realsize;
}


int panic_dump_android_log(char *buf, size_t size, int type)
{
	size_t ret = 0 ;
	//printk ("type %d, buf %x, size %d\n", type, buf, size) ;
	memset (buf, 0, size);
	switch (type)
	{
        case 1:
		ret = panic_dump_main(buf, size);
		break;
        case 2:
		ret = panic_dump_events(buf, size);
		break;
        case 3:
		ret = panic_dump_radio(buf, size);
		break;
        case 4:
		ret = panic_dump_system(buf, size);
		break;
        default:
		ret = 0;
		break;
    }
    if (ret!=0)
	    ret = size;

    return ret;
}

device_initcall(logger_init);
module_exit(logger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Robert Love, <rlove@google.com>");
MODULE_DESCRIPTION("Android Logger");
