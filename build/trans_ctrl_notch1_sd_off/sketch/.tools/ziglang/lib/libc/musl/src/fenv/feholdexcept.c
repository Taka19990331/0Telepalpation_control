#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\fenv\\feholdexcept.c"
#include <fenv.h>

int feholdexcept(fenv_t *envp)
{
	fegetenv(envp);
	feclearexcept(FE_ALL_EXCEPT);
	return 0;
}
