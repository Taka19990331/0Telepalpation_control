#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\generic-freebsd\\sys\\_maxphys.h"
/*-
 * This file is in the public domain.
 */
#ifndef MAXPHYS
#ifdef __ILP32__
#define MAXPHYS		(128 * 1024)
#else
#define MAXPHYS		(1024 * 1024)
#endif
#endif