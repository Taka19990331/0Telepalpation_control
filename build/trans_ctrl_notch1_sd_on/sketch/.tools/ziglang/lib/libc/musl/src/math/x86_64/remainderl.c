#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\x86_64\\remainderl.c"
#include <math.h>

long double remainderl(long double x, long double y)
{
	unsigned short fpsr;
	do __asm__ ("fprem1; fnstsw %%ax" : "+t"(x), "=a"(fpsr) : "u"(y));
	while (fpsr & 0x400);
	return x;
}
