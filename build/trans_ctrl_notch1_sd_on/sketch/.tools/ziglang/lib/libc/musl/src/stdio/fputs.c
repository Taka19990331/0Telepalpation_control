#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\fputs.c"
#include "stdio_impl.h"
#include <string.h>

int fputs(const char *restrict s, FILE *restrict f)
{
	size_t l = strlen(s);
	return (fwrite(s, 1, l, f)==l) - 1;
}

weak_alias(fputs, fputs_unlocked);
