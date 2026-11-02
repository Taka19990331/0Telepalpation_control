#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\riscv64\\pthread_arch.h"
static inline uintptr_t __get_tp()
{
	uintptr_t tp;
	__asm__ __volatile__("mv %0, tp" : "=r"(tp));
	return tp;
}

#define TLS_ABOVE_TP
#define GAP_ABOVE_TP 0

#define DTP_OFFSET 0x800

#define MC_PC __gregs[0]
