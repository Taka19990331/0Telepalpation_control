#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\math\\__math_uflowf.c"
#include "libm.h"

float __math_uflowf(uint32_t sign)
{
	return __math_xflowf(sign, 0x1p-95f);
}
