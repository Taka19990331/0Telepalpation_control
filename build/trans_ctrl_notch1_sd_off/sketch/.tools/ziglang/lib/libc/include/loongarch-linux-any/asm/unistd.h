#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\loongarch-linux-any\\asm\\unistd.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#include <asm/bitsperlong.h>

#if __BITS_PER_LONG == 32
#include <asm/unistd_32.h>
#else
#include <asm/unistd_64.h>
#endif