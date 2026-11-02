#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\netfilter_ipv4\\ipt_ah.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _IPT_AH_H
#define _IPT_AH_H

#include <linux/types.h>

struct ipt_ah {
	__u32 spis[2];			/* Security Parameter Index */
	__u8  invflags;			/* Inverse flags */
};



/* Values for "invflags" field in struct ipt_ah. */
#define IPT_AH_INV_SPI		0x01	/* Invert the sense of spi. */
#define IPT_AH_INV_MASK	0x01	/* All possible flags. */

#endif /*_IPT_AH_H*/