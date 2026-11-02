#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\netfilter\\xt_realm.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _XT_REALM_H
#define _XT_REALM_H

#include <linux/types.h>

struct xt_realm_info {
	__u32 id;
	__u32 mask;
	__u8 invert;
};

#endif /* _XT_REALM_H */