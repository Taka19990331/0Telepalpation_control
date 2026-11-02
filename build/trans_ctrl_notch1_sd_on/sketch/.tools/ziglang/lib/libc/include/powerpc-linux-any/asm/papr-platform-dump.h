#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\powerpc-linux-any\\asm\\papr-platform-dump.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _PAPR_PLATFORM_DUMP_H_
#define _PAPR_PLATFORM_DUMP_H_

#include <linux/types.h>
#include <asm/ioctl.h>
#include <asm/papr-miscdev.h>

/*
 * ioctl for /dev/papr-platform-dump. Returns a platform-dump handle fd
 * corresponding to dump tag.
 */
#define PAPR_PLATFORM_DUMP_IOC_CREATE_HANDLE _IOW(PAPR_MISCDEV_IOC_ID, 6, __u64)
#define PAPR_PLATFORM_DUMP_IOC_INVALIDATE    _IOW(PAPR_MISCDEV_IOC_ID, 7, __u64)

#endif /* _PAPR_PLATFORM_DUMP_H_ */