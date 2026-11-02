#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\x86-linux-any\\asm\\posix_types_x32.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _ASM_X86_POSIX_TYPES_X32_H
#define _ASM_X86_POSIX_TYPES_X32_H

/*
 * This file is only used by user-level software, so you need to
 * be a little careful about namespace pollution etc.  Also, we cannot
 * assume GCC is being used.
 *
 * These types should generally match the ones used by the 64-bit kernel,
 *
 */

typedef long long __kernel_long_t;
typedef unsigned long long __kernel_ulong_t;
#define __kernel_long_t __kernel_long_t

#include <asm/posix_types_64.h>

#endif /* _ASM_X86_POSIX_TYPES_X32_H */