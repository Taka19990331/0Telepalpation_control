#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\multibyte\\mbrlen.c"
#include <wchar.h>

size_t mbrlen(const char *restrict s, size_t n, mbstate_t *restrict st)
{
	static unsigned internal;
	return mbrtowc(0, s, n, st ? st : (mbstate_t *)&internal);
}
