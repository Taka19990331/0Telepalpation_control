#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\env\\secure_getenv.c"
#define _GNU_SOURCE
#include <stdlib.h>
#include "libc.h"

char *secure_getenv(const char *name)
{
	return libc.secure ? NULL : getenv(name);
}
