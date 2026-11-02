#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\x32\\fmaf.c"
#include <math.h>

#if __FMA__

float fmaf(float x, float y, float z)
{
	__asm__ ("vfmadd132ss %1, %2, %0" : "+x" (x) : "x" (y), "x" (z));
	return x;
}

#elif __FMA4__

float fmaf(float x, float y, float z)
{
	__asm__ ("vfmaddss %3, %2, %1, %0" : "=x" (x) : "x" (x), "x" (y), "x" (z));
	return x;
}

#else

#include "../fmaf.c"

#endif
