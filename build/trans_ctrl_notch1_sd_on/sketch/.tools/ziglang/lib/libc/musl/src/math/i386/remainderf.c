#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\i386\\remainderf.c"
#include <math.h>

float remainderf(float x, float y)
{
	unsigned short fpsr;
	// fprem1 does not introduce excess precision into x
	do __asm__ ("fprem1; fnstsw %%ax" : "+t"(x), "=a"(fpsr) : "u"(y));
	while (fpsr & 0x400);
	return x;
}

weak_alias(remainderf, dremf);
