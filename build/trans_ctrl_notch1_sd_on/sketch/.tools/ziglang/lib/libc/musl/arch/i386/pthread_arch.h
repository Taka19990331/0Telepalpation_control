#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\i386\\pthread_arch.h"
static inline uintptr_t __get_tp()
{
	uintptr_t tp;
	__asm__ ("movl %%gs:0,%0" : "=r" (tp) );
	return tp;
}

#define MC_PC gregs[REG_EIP]
