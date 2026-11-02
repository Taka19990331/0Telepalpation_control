#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\s390x-linux-any\\asm\\clp.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * ioctl interface for /dev/clp
 *
 * Copyright IBM Corp. 2016
 * Author(s): Martin Schwidefsky <schwidefsky@de.ibm.com>
 */

#ifndef _ASM_CLP_H
#define _ASM_CLP_H

#include <linux/types.h>
#include <linux/ioctl.h>

struct clp_req {
	unsigned int c : 1;
	unsigned int r : 1;
	unsigned int lps : 6;
	unsigned int cmd : 8;
	unsigned int : 16;
	unsigned int reserved;
	__u64 data_p;
};

#define CLP_IOCTL_MAGIC 'c'

#define CLP_SYNC _IOWR(CLP_IOCTL_MAGIC, 0xC1, struct clp_req)

#endif