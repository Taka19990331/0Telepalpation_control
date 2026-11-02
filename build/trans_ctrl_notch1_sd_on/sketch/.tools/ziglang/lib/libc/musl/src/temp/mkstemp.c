#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\temp\\mkstemp.c"
#include <stdlib.h>

int mkstemp(char *template)
{
	return __mkostemps(template, 0, 0);
}
