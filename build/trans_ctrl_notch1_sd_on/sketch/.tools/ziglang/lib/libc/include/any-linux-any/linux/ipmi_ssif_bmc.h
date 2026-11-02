#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\ipmi_ssif_bmc.h"
/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note*/
/*
 * Copyright (c) 2022, Ampere Computing LLC.
 */

#ifndef _LINUX_IPMI_SSIF_BMC_H
#define _LINUX_IPMI_SSIF_BMC_H

#include <linux/types.h>

/* Max length of ipmi ssif message included netfn and cmd field */
#define IPMI_SSIF_PAYLOAD_MAX         254
struct ipmi_ssif_msg {
	unsigned int len;
	__u8    payload[IPMI_SSIF_PAYLOAD_MAX];
};

#endif /* _LINUX_IPMI_SSIF_BMC_H */