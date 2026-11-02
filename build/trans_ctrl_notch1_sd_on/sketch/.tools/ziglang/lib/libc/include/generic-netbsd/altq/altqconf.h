#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\generic-netbsd\\altq\\altqconf.h"
/*	$NetBSD: altqconf.h,v 1.4 2002/09/22 20:09:15 jdolecek Exp $	*/

#ifdef _KERNEL

#if defined(_KERNEL_OPT)
#include "opt_altq_enabled.h"
#endif

#include <sys/conf.h>

#ifdef ALTQ
#define	NALTQ	1
#else
#define	NALTQ	0
#endif

#endif /* _KERNEL */