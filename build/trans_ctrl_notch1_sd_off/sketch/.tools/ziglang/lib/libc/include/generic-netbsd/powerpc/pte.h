#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\generic-netbsd\\powerpc\\pte.h"
/*	$NetBSD: pte.h,v 1.9 2006/08/05 21:26:49 sanjayl Exp $	*/

#ifdef _KERNEL_OPT
#include "opt_ppcarch.h"
#endif

#if defined (PPC_OEA) || defined (PPC_OEA64_BRIDGE) || defined (PPC_OEA64)
#include <powerpc/oea/pte.h>
#endif