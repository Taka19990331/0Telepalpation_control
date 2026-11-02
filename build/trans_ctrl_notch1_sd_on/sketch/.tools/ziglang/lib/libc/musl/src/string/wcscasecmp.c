#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\string\\wcscasecmp.c"
#include <wchar.h>
#include <wctype.h>

int wcscasecmp(const wchar_t *l, const wchar_t *r)
{
	return wcsncasecmp(l, r, -1);
}
