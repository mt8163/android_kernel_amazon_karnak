/*
 * Copyright (C) 2011 Amazon Technologies, Inc.
 * Portions Copyright (C) 2007-2008 Google, Inc.
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

#ifndef _LINUX_LOGGER_KLOG_H
#define _LINUX_LOGGER_KLOG_H

void logger_kmsg_write(const char *log_msg, size_t len);

#endif /* _LINUX_LOGGER_KLOG_H */
