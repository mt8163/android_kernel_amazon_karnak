/* Amazon logger driver unit test
 *
 * Copyright 2017 Amazon Technologies, Inc. All Rights Reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

typedef enum android_LogPriority {
	ANDROID_LOG_UNKNOWN = 0,
	ANDROID_LOG_DEFAULT,    /* only for SetMinPriority() */
	ANDROID_LOG_VERBOSE,
	ANDROID_LOG_DEBUG,
	ANDROID_LOG_INFO,
	ANDROID_LOG_WARN,
	ANDROID_LOG_ERROR,
	ANDROID_LOG_FATAL,
	ANDROID_LOG_SILENT,	   /* only for SetMinPriority(); must be last */
} android_LogPriority;

struct logger_entry {
	uint16_t	len;	/* length of the payload */
	uint16_t	pad;	/* no matter what, we get 2 bytes of padding */
	int32_t		pid;	/* generating process's pid */
	int32_t		tid;	/* generating process's tid */
	int32_t		sec;	/* seconds since Epoch */
	int32_t		nsec;	/* nanoseconds */
	char		msg[0];
} __attribute__((__packed__));

struct amazon_msg_buf {
	union {
		char buf[4076];
		struct logger_entry entry;
	} __attribute__((aligned(4)));
};

const static char log_pri_info[] = {' ', ' ', 'V', 'D', 'I', 'W', 'E', 'F', 'S'};
static int error_count;
static int log_count;

static int print_log_msg(struct amazon_msg_buf *buf)
{
	int i;
	int msgStart = -1;
	int msgEnd = -1;
	/**
	 * format: <priority:1><tag:N>\0<message:N>\0
	 * tag starts at buf->msg+1
	 * msg starts at buf->msg+1+len(tag)+1
	 */
	android_LogPriority pri = (android_LogPriority) buf->entry.msg[0];

	if (pri > 8) {
		fprintf(stderr, "Error: android log priotiry is incorrect, malformat ?");
		error_count++;
		return -1;
	}

	char *msg = buf->entry.msg;

	for (i = 1; i < buf->entry.len; i++) {
		if (msg[i] == '\0') {
			if (msgStart == -1) {
				msgStart = i + 1;
			} else {
				msgEnd = i;
				break;
			}
		}
	}

	if (msgStart == -1) {
		for (i = 1; i < buf->entry.len; i++) {
			/* odd characters in tag? */
			if ((msg[i] <= ' ') || (msg[i] == ':') || (msg[i] >= 0x7f)) {
				msg[i] = '\0';
				msgStart = i + 1;
				break;
			}
		}
		if (msgStart == -1) {
			msgStart = buf->entry.len - 1;
		}
	}

	if (msgEnd == -1) {
		/* incoming message not null-terminated; force it */
		msgEnd = buf->entry.len - 1; /* may result in msgEnd < msgStart */
		msg[msgEnd] = '\0';
	}

	char *tag = buf->entry.msg + 1;
	char *message = buf->entry.msg + msgStart;
	printf("sec[%d] nsec[%d] pid[%d] tid[%d] Pri[%c] tag[%s] msg[%s]\n",
		buf->entry.sec, buf->entry.nsec, buf->entry.pid,
		buf->entry.tid, log_pri_info[pri], tag, message);

	return 0;
}

int main(int argc, char *argv[])
{
	fd_set rfds;
	struct timeval tv;
	int retval;
	int count = 50;
	struct amazon_msg_buf log_msg;
	int ret = 0;
	char bufname[10] = {0};

	memset(log_msg.buf, 0, sizeof(struct amazon_msg_buf));
	if (argc < 2) {
		fprintf(stderr, "Usage: logger_test [device_node of metrics/vitals]\n"
			"For example: logger_test /dev/metrics\n");
		return -1;
	}

	if (!strstr(argv[1], "metrics") && !strstr(argv[1], "vitals")) {
		fprintf(stderr, "Error, wrong device_node\n");
		return -1;
	}

	int read_fd = open(argv[1], O_RDONLY|O_NONBLOCK);
	if (read_fd < 0) {
		fprintf(stderr, "Error: fail to open device %s\n", argv[1]);
		return -1;
	}

	while (count--) {
		FD_ZERO(&rfds);
		FD_SET(read_fd, &rfds);
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		retval = select(read_fd+1, &rfds, NULL, NULL, &tv);
		if (retval == -1) {
			if (errno != EINTR && errno != EAGAIN) {
				error_count++;
				perror("Error, in select()");
			} else
				continue;
		} else if (retval > 0) {
			ret = read(read_fd, log_msg.buf, 4076);
			if ((ret > 0) && (ret != log_msg.entry.len + sizeof(struct logger_entry))) {
				fprintf(stderr, "Error, read out length is not  expected as length=%d ="
					"payload.len=%d + hdr_size=%d",
					ret, log_msg.entry.len, sizeof(struct logger_entry));
				error_count++;
			} else if ((ret > 0) && (ret == log_msg.entry.len + sizeof(struct logger_entry))) {
				if (print_log_msg(&log_msg))
					error_count++;
				else
					log_count++;
			}
			continue;
		} else if (retval == 0) {
			printf("info: no data within 10 seconds, continue\n");
			continue;
		}
	}

	printf("\namazon logger test finished:\n  total log_read_count = %d\n"
			"  total error_count= %d\n", log_count, error_count);
	return 0;
}
