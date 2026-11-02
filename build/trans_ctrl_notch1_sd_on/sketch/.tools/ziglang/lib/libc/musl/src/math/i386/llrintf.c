#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\i386\\llrintf.c"
#include <math.h>

long long llrintf(float x)
{
	long long r;
	__asm__ ("fistpll %0" : "=m"(r) : "t"(x) : "st");
	return r;
}
