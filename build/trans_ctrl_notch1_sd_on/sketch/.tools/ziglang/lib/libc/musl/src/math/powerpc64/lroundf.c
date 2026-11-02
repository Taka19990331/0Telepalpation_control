#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\powerpc64\\lroundf.c"
#include <math.h>

#ifdef __VSX__

long lroundf(float x)
{
	long n;
	__asm__ (
		"xsrdpi %1, %1\n"
		"fctid %0, %1\n" : "=d"(n), "+f"(x));
	return n;
}

#else

#include "../lroundf.c"

#endif
