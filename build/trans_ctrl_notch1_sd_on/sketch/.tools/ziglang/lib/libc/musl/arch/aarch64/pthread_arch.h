#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\aarch64\\pthread_arch.h"
static inline uintptr_t __get_tp()
{
	uintptr_t tp;
	__asm__ ("mrs %0,tpidr_el0" : "=r"(tp));
	return tp;
}

#define TLS_ABOVE_TP
#define GAP_ABOVE_TP 16

#define MC_PC pc
