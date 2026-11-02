#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\riscv64-openbsd-none\\riscv64\\setjmp.h"
/*	$OpenBSD: setjmp.h,v 1.4 2021/06/17 16:09:08 kettenis Exp $	*/

/*
 * machine/setjmp.h: machine dependent setjmp-related information.
 */

#define	_JBLEN		32 /* sp, ra, [f]s0-11, fscr, magic val, sigmask */
#define	_JB_SIGMASK	28