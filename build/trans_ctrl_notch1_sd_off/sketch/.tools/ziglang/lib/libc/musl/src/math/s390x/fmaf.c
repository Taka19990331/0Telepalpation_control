#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\s390x\\fmaf.c"
#include <math.h>

float fmaf(float x, float y, float z)
{
	__asm__ ("maebr %0, %1, %2" : "+f"(z) : "f"(x), "f"(y));
	return z;
}
