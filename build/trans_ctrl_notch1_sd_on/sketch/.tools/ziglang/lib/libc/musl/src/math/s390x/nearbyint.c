#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\s390x\\nearbyint.c"
#include <math.h>

#if defined(__HTM__) || __ARCH__ >= 9

double nearbyint(double x)
{
	__asm__ ("fidbra %0, 0, %1, 4" : "=f"(x) : "f"(x));
	return x;
}

#else

#include "../nearbyint.c"

#endif
