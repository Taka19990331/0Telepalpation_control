#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\netfilter_ipv6\\ip6t_mh.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _IP6T_MH_H
#define _IP6T_MH_H

#include <linux/types.h>

/* MH matching stuff */
struct ip6t_mh {
	__u8 types[2];	/* MH type range */
	__u8 invflags;	/* Inverse flags */
};

/* Values for "invflags" field in struct ip6t_mh. */
#define IP6T_MH_INV_TYPE	0x01	/* Invert the sense of type. */
#define IP6T_MH_INV_MASK	0x01	/* All possible flags. */

#endif /*_IP6T_MH_H*/