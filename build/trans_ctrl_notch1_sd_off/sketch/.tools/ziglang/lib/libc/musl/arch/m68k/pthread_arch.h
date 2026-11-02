#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\m68k\\pthread_arch.h"
static inline uintptr_t __get_tp()
{
	return __syscall(SYS_get_thread_area);
}

#define TLS_ABOVE_TP
#define GAP_ABOVE_TP 0

#define TP_OFFSET 0x7000
#define DTP_OFFSET 0x8000

#define MC_PC gregs[R_PC]
