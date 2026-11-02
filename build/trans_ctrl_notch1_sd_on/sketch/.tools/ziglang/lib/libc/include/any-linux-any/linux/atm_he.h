#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\atm_he.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/* atm_he.h */

#ifndef LINUX_ATM_HE_H
#define LINUX_ATM_HE_H

#include <linux/atmioc.h>

#define HE_GET_REG	_IOW('a', ATMIOC_SARPRV, struct atmif_sioc)

#define HE_REGTYPE_PCI	1
#define HE_REGTYPE_RCM	2
#define HE_REGTYPE_TCM	3
#define HE_REGTYPE_MBOX	4

struct he_ioctl_reg {
	unsigned addr, val;
	char type;
};

#endif /* LINUX_ATM_HE_H */