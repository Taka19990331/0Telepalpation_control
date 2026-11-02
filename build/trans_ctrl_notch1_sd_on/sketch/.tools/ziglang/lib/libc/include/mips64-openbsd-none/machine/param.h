#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\mips64-openbsd-none\\machine\\param.h"
/* $OpenBSD: param.h,v 1.5 2018/08/09 12:19:32 patrick Exp $ */

/* Public Domain */

#ifndef	_MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#define	MACHINE		"octeon"
#define	_MACHINE	octeon
#define	MACHINE_ARCH	"mips64"
#define	_MACHINE_ARCH	mips64
#define	MID_MACHINE	MID_MIPS64

#define	PAGE_SHIFT	14

#include <mips64/param.h>

#ifdef _KERNEL
#define __HAVE_FDT
#endif

#endif /* _MACHINE_PARAM_H_ */