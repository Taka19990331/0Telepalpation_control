#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\netfilter\\xt_connmark.h"
/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/* Copyright (C) 2002,2004 MARA Systems AB <https://www.marasystems.com>
 * by Henrik Nordstrom <hno@marasystems.com>
 */

#ifndef _XT_CONNMARK_H
#define _XT_CONNMARK_H

#include <linux/types.h>

enum {
	XT_CONNMARK_SET = 0,
	XT_CONNMARK_SAVE,
	XT_CONNMARK_RESTORE
};

enum {
	D_SHIFT_LEFT = 0,
	D_SHIFT_RIGHT,
};

struct xt_connmark_tginfo1 {
	__u32 ctmark, ctmask, nfmask;
	__u8 mode;
};

struct xt_connmark_tginfo2 {
	__u32 ctmark, ctmask, nfmask;
	__u8 shift_dir, shift_bits, mode;
};

struct xt_connmark_mtinfo1 {
	__u32 mark, mask;
	__u8 invert;
};

#endif /*_XT_CONNMARK_H*/