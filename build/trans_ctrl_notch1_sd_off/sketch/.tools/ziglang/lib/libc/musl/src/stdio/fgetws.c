#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\fgetws.c"
#include "stdio_impl.h"
#include <wchar.h>

wint_t __fgetwc_unlocked(FILE *);

wchar_t *fgetws(wchar_t *restrict s, int n, FILE *restrict f)
{
	wchar_t *p = s;

	if (!n--) return s;

	FLOCK(f);

	for (; n; n--) {
		wint_t c = __fgetwc_unlocked(f);
		if (c == WEOF) break;
		*p++ = c;
		if (c == '\n') break;
	}
	*p = 0;
	if (ferror(f)) p = s;

	FUNLOCK(f);

	return (p == s) ? NULL : s;
}

weak_alias(fgetws, fgetws_unlocked);
