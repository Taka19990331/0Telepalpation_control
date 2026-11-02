#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\netfilter\\xt_l2tp.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _LINUX_NETFILTER_XT_L2TP_H
#define _LINUX_NETFILTER_XT_L2TP_H

#include <linux/types.h>

enum xt_l2tp_type {
	XT_L2TP_TYPE_CONTROL,
	XT_L2TP_TYPE_DATA,
};

/* L2TP matching stuff */
struct xt_l2tp_info {
	__u32 tid;			/* tunnel id */
	__u32 sid;			/* session id */
	__u8 version;			/* L2TP protocol version */
	__u8 type;			/* L2TP packet type */
	__u8 flags;			/* which fields to match */
};

enum {
	XT_L2TP_TID	= (1 << 0),	/* match L2TP tunnel id */
	XT_L2TP_SID	= (1 << 1),	/* match L2TP session id */
	XT_L2TP_VERSION	= (1 << 2),	/* match L2TP protocol version */
	XT_L2TP_TYPE	= (1 << 3),	/* match L2TP packet type */
};

#endif /* _LINUX_NETFILTER_XT_L2TP_H */