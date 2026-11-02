#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\fenv\\fegetexceptflag.c"
#include <fenv.h>

int fegetexceptflag(fexcept_t *fp, int mask)
{
	*fp = fetestexcept(mask);
	return 0;
}
