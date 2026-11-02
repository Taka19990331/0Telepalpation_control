#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\riscv32\\fma.c"
#include <math.h>

#if __riscv_flen >= 64

double fma(double x, double y, double z)
{
	__asm__ ("fmadd.d %0, %1, %2, %3" : "=f"(x) : "f"(x), "f"(y), "f"(z));
	return x;
}

#else

#include "../fma.c"

#endif
