#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\powerpc64-openbsd-none\\machine\\fpu.h"
/*	$OpenBSD: fpu.h,v 1.4 2022/03/24 18:42:05 kettenis Exp $	*/

/* public domain */

#ifndef _MACHINE_FPU_H
#define _MACHINE_FPU_H

#ifdef _KERNEL

void	save_vsx(struct proc *);
void	restore_vsx(struct proc *);

int	fpu_sigcode(struct proc *);

#endif

#endif /* _MACHINE_FPU_H_ */