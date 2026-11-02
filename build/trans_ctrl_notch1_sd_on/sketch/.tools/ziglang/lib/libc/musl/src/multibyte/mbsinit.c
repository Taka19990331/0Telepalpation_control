#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\multibyte\\mbsinit.c"
#include <wchar.h>

int mbsinit(const mbstate_t *st)
{
	return !st || !*(unsigned *)st;
}
