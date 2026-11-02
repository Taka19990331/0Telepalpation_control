#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\csky-linux-any\\asm\\cachectl.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef __ASM_CSKY_CACHECTL_H
#define __ASM_CSKY_CACHECTL_H

/*
 * See "man cacheflush"
 */
#define ICACHE  (1<<0)
#define DCACHE  (1<<1)
#define BCACHE  (ICACHE|DCACHE)

#endif /* __ASM_CSKY_CACHECTL_H */