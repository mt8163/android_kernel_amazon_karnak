/*
 * gating.h
 *
 * Copyright 2018  Amazon Technologies, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#ifndef __PRIVACY_H__
#define __PRIVACY_H__

#include <linux/notifier.h>

enum gating_state {
	UNGATED = 0,	/* HW gating is OFF */
	GATED,		/* HW gating is ON */
	ONGOING,	/* work is pending to turn ON gating */
	NONE,		/* nothing ongoing, can be used to clear req_state */
};

/* register a srcu notifier call by providing a notifier block */
int register_gating_state_notifier(struct notifier_block *nb);

#endif
