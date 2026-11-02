#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\string\\wcsdup.c"
#include <stdlib.h>
#include <wchar.h>

wchar_t *wcsdup(const wchar_t *s)
{
	size_t l = wcslen(s);
	wchar_t *d = malloc((l+1)*sizeof(wchar_t));
	if (!d) return NULL;
	return wmemcpy(d, s, l+1);
}
