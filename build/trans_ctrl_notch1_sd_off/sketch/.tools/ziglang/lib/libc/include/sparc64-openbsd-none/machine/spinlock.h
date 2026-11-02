#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\sparc64-openbsd-none\\machine\\spinlock.h"
/*	$OpenBSD: spinlock.h,v 1.3 2017/09/05 02:40:54 guenther Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(0x00)
#define _ATOMIC_LOCK_LOCKED	(0xFF)
typedef unsigned char _atomic_lock_t;

#endif