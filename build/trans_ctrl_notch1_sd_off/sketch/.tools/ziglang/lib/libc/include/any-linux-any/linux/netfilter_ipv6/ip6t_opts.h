#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\netfilter_ipv6\\ip6t_opts.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _IP6T_OPTS_H
#define _IP6T_OPTS_H

#include <linux/types.h>

#define IP6T_OPTS_OPTSNR 16

struct ip6t_opts {
	__u32 hdrlen;			/* Header Length */
	__u8 flags;				/*  */
	__u8 invflags;			/* Inverse flags */
	__u16 opts[IP6T_OPTS_OPTSNR];	/* opts */
	__u8 optsnr;			/* Nr of OPts */
};

#define IP6T_OPTS_LEN 		0x01
#define IP6T_OPTS_OPTS 		0x02
#define IP6T_OPTS_NSTRICT	0x04

/* Values for "invflags" field in struct ip6t_rt. */
#define IP6T_OPTS_INV_LEN	0x01	/* Invert the sense of length. */
#define IP6T_OPTS_INV_MASK	0x01	/* All possible flags. */

#endif /*_IP6T_OPTS_H*/