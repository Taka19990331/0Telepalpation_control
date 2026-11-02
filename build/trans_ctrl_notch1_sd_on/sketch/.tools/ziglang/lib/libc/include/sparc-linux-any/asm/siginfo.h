#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\sparc-linux-any\\asm\\siginfo.h"
/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __SPARC_SIGINFO_H
#define __SPARC_SIGINFO_H

#if defined(__sparc__) && defined(__arch64__)

#define __ARCH_SI_BAND_T int

#endif /* defined(__sparc__) && defined(__arch64__) */

#include <asm-generic/siginfo.h>


#define SI_NOINFO	32767		/* no information in siginfo_t */

#endif /* __SPARC_SIGINFO_H */