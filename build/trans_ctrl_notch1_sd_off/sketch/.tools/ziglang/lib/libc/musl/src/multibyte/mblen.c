#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\multibyte\\mblen.c"
#include <stdlib.h>

int mblen(const char *s, size_t n)
{
	return mbtowc(0, s, n);
}
