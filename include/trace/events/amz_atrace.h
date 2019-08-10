/*
 * amz_atrace_base.h
 *
 * Copyright 2015 Amazon.com, Inc. or its Affiliates. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 */

/*
 * This file enables ftrace logging in a way that atrace/systrace would understand
 * without any custom javascript change in chromium-trace
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM amz_atrace

#if !defined(_TRACE_AMZ_BASE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_AMZ_BASE_H

#include <linux/tracepoint.h>


#define KERNEL_ATRACE_COUNTER 0
#define KERNEL_ATRACE_BEGIN 1
#define KERNEL_ATRACE_END 2

#define print_flags_header(flags) __print_flags(flags, "",	\
	{ (1UL << KERNEL_ATRACE_COUNTER),	"C" },		\
	{ (1UL << KERNEL_ATRACE_BEGIN),		"B" },		\
	{ (1UL << KERNEL_ATRACE_END),		"E" })


#define print_flags_delim(flags) __print_flags(flags, "",	\
	{ (1UL << KERNEL_ATRACE_COUNTER),	"|1|" },		\
	{ (1UL << KERNEL_ATRACE_BEGIN),		"|1|" },		\
	{ (1UL << KERNEL_ATRACE_END),		"" })

TRACE_EVENT(tracing_mark_write,

	TP_PROTO(const char *name, unsigned int flags, unsigned int value),

	TP_ARGS(name, flags, value),

	TP_STRUCT__entry(
		__string(name, name)
		__field(unsigned int, flags)
		__field(unsigned int, value)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->flags = flags;
		__entry->value = value;
	),

	TP_printk("%s%s%s|%u", print_flags_header(__entry->flags), print_flags_delim(__entry->flags),
		__get_str(name), __entry->value)
);

#define ATRACE_COUNTER(name, value) trace_tracing_mark_write(name, (1 << KERNEL_ATRACE_COUNTER), value)
#define ATRACE_BEGIN(name) trace_tracing_mark_write(name, (1 << KERNEL_ATRACE_BEGIN), 0)
#define ATRACE_END(name) trace_tracing_mark_write("", (1 << KERNEL_ATRACE_END), 1)

#endif /* _TRACE_AMZ_BASE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
