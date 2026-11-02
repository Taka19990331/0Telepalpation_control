#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\__math_oflow.c"
#include "libm.h"

double __math_oflow(uint32_t sign)
{
	return __math_xflow(sign, 0x1p769);
}
