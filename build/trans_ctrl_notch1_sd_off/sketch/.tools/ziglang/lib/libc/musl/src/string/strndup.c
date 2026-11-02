#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\string\\strndup.c"
#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n)
{
	size_t l = strnlen(s, n);
	char *d = malloc(l+1);
	if (!d) return NULL;
	memcpy(d, s, l);
	d[l] = 0;
	return d;
}
