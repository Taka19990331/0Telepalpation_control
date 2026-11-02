#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\multibyte\\mbrtoc32.c"
#include <uchar.h>
#include <wchar.h>

size_t mbrtoc32(char32_t *restrict pc32, const char *restrict s, size_t n, mbstate_t *restrict ps)
{
	static unsigned internal_state;
	if (!ps) ps = (void *)&internal_state;
	if (!s) return mbrtoc32(0, "", 1, ps);
	wchar_t wc;
	size_t ret = mbrtowc(&wc, s, n, ps);
	if (ret <= 4 && pc32) *pc32 = wc;
	return ret;
}
