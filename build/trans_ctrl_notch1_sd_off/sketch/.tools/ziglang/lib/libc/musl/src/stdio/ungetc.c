#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\ungetc.c"
#include "stdio_impl.h"

int ungetc(int c, FILE *f)
{
	if (c == EOF) return c;

	FLOCK(f);

	if (!f->rpos) __toread(f);
	if (!f->rpos || f->rpos <= f->buf - UNGET) {
		FUNLOCK(f);
		return EOF;
	}

	*--f->rpos = c;
	f->flags &= ~F_EOF;

	FUNLOCK(f);
	return (unsigned char)c;
}
