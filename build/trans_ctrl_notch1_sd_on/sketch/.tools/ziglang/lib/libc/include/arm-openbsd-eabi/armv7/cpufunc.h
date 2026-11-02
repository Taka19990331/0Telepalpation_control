#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\arm-openbsd-eabi\\armv7\\cpufunc.h"
/*	$OpenBSD: cpufunc.h,v 1.1 2020/11/29 12:54:36 kettenis Exp $	*/

/* Public Domain */

#ifndef _MACHINE_CPUFUNC_H_
#define _MACHINE_CPUFUNC_H_

#include <arm/cpufunc.h>

#ifdef _KERNEL

register_t smc_call(register_t, register_t, register_t, register_t);

#endif	/* _KERNEL */

#endif	/* _MACHINE_CPUFUNC_H_ */