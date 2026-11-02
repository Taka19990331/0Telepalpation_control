#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\arm-freebsd-eabihf\\machine\\smp.h"
#ifndef _MACHINE_SMP_H_
#define _MACHINE_SMP_H_

#include <sys/_cpuset.h>
#include <machine/pcb.h>

enum {
	IPI_AST,
	IPI_PREEMPT,
	IPI_RENDEZVOUS,
	IPI_STOP,
	IPI_STOP_HARD = IPI_STOP, /* These are synonyms on arm. */
	IPI_HARDCLOCK,
	IPI_TLB,		/* Not used now, but keep it reserved. */
	IPI_CACHE,		/* Not used now, but keep it reserved. */
	INTR_IPI_COUNT
};

void	init_secondary(int cpu);
void	mpentry(void);

void	ipi_all_but_self(u_int ipi);
void	ipi_cpu(int cpu, u_int ipi);
void	ipi_selected(cpuset_t cpus, u_int ipi);

/* Platform interface */
void	platform_mp_setmaxid(void);
void	platform_mp_start_ap(void);

#endif /* !_MACHINE_SMP_H_ */