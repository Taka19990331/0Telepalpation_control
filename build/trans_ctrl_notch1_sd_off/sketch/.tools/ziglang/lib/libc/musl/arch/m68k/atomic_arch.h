#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\arch\\m68k\\atomic_arch.h"
#define a_cas a_cas
static inline int a_cas(volatile int *p, int t, int s)
{
	__asm__ __volatile__ (
		"cas.l %0, %2, (%1)"
		: "+d"(t) : "a"(p), "d"(s) : "memory", "cc");
	return t;
}
