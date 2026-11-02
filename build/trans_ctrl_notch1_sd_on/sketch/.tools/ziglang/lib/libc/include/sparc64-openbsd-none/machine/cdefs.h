#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\sparc64-openbsd-none\\machine\\cdefs.h"
/*	$OpenBSD: cdefs.h,v 1.4 2013/03/28 17:30:45 martynas Exp $	*/

#ifndef	_MACHINE_CDEFS_H_
#define	_MACHINE_CDEFS_H_

#define __strong_alias(alias,sym)				\
	__asm__(".global " __STRING(alias) " ; "		\
	    __STRING(alias) " = " __STRING(sym))
#define __weak_alias(alias,sym)					\
	__asm__(".weak " __STRING(alias) " ; "			\
	    __STRING(alias) " = " __STRING(sym))
#define __warn_references(sym,msg)				\
	__asm__(".section .gnu.warning." __STRING(sym)		\
	    " ; .ascii \"" msg "\" ; .text")

#endif /* !_MACHINE_CDEFS_H_ */