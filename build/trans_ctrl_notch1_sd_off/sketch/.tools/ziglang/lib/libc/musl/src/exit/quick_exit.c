#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\exit\\quick_exit.c"
#include <stdlib.h>
#include "libc.h"

static void dummy() { }
weak_alias(dummy, __funcs_on_quick_exit);

_Noreturn void quick_exit(int code)
{
	__funcs_on_quick_exit();
	_Exit(code);
}
