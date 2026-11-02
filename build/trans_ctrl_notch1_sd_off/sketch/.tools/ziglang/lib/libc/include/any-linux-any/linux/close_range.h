#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-linux-any\\linux\\close_range.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _LINUX_CLOSE_RANGE_H
#define _LINUX_CLOSE_RANGE_H

/* Unshare the file descriptor table before closing file descriptors. */
#define CLOSE_RANGE_UNSHARE	(1U << 1)

/* Set the FD_CLOEXEC bit instead of closing the file descriptor. */
#define CLOSE_RANGE_CLOEXEC	(1U << 2)

#endif /* _LINUX_CLOSE_RANGE_H */