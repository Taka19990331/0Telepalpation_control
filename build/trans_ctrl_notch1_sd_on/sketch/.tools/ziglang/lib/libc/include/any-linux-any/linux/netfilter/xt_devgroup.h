#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\netfilter\\xt_devgroup.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _XT_DEVGROUP_H
#define _XT_DEVGROUP_H

#include <linux/types.h>

enum xt_devgroup_flags {
	XT_DEVGROUP_MATCH_SRC	= 0x1,
	XT_DEVGROUP_INVERT_SRC	= 0x2,
	XT_DEVGROUP_MATCH_DST	= 0x4,
	XT_DEVGROUP_INVERT_DST	= 0x8,
};

struct xt_devgroup_info {
	__u32	flags;
	__u32	src_group;
	__u32	src_mask;
	__u32	dst_group;
	__u32	dst_mask;
};

#endif /* _XT_DEVGROUP_H */