#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\s390x\\pthread_arch.h"
static inline uintptr_t __get_tp()
{
	uintptr_t tp;
	__asm__ (
		"ear  %0, %%a0\n"
		"sllg %0, %0, 32\n"
		"ear  %0, %%a1\n"
		: "=r"(tp));
	return tp;
}

#define MC_PC psw.addr
