#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\multibyte\\c32rtomb.c"
#include <uchar.h>
#include <wchar.h>

size_t c32rtomb(char *restrict s, char32_t c32, mbstate_t *restrict ps)
{
	return wcrtomb(s, c32, ps);
}
