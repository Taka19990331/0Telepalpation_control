#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\generic-openbsd\\mips64\\setjmp.h"
/*	$OpenBSD: setjmp.h,v 1.4 2018/01/08 16:44:32 visa Exp $	*/

/* Public domain */

#ifndef _MIPS64_SETJMP_H_
#define _MIPS64_SETJMP_H_

#define	_JB_MASK	(1 * REGSZ)
#define	_JB_PC		(2 * REGSZ)
#define	_JB_REGS	(3 * REGSZ)
#define	_JB_FPREGS	(37 * REGSZ)

#define	_JBLEN	83		/* size, in longs, of a jmp_buf */

#endif /* !_MIPS64_SETJMP_H_ */