#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\ipmi_bmc.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2015-2018, Intel Corporation.
 */

#ifndef _LINUX_IPMI_BMC_H
#define _LINUX_IPMI_BMC_H

#include <linux/ioctl.h>

#define __IPMI_BMC_IOCTL_MAGIC        0xB1
#define IPMI_BMC_IOCTL_SET_SMS_ATN    _IO(__IPMI_BMC_IOCTL_MAGIC, 0x00)
#define IPMI_BMC_IOCTL_CLEAR_SMS_ATN  _IO(__IPMI_BMC_IOCTL_MAGIC, 0x01)
#define IPMI_BMC_IOCTL_FORCE_ABORT    _IO(__IPMI_BMC_IOCTL_MAGIC, 0x02)

#endif /* _LINUX_IPMI_BMC_H */