#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\arm-openbsd-eabi\\arm\\machdep.h"
/*	$OpenBSD: machdep.h,v 1.6 2024/04/29 12:24:46 jsg Exp $	*/
/* $NetBSD: machdep.h,v 1.7 2002/02/21 02:52:21 thorpej Exp $ */

#ifndef _ARM_MACHDEP_H_
#define _ARM_MACHDEP_H_

/* misc prototypes used by the many arm machdeps */
void halt (void);
void data_abort_handler (trapframe_t *);
void prefetch_abort_handler (trapframe_t *);
void undefinedinstruction_bounce (trapframe_t *);
void dumpsys	(void);

/* 
 * note that we use void * as all the platforms have different ideas on what
 * the structure is
 */
u_int initarm (void *, void *, void *, paddr_t);

#endif