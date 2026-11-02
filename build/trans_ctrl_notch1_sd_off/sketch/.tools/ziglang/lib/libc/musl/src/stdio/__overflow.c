#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\__overflow.c"
#include "stdio_impl.h"

int __overflow(FILE *f, int _c)
{
	unsigned char c = _c;
	if (!f->wend && __towrite(f)) return EOF;
	if (f->wpos != f->wend && c != f->lbf) return *f->wpos++ = c;
	if (f->write(f, &c, 1)!=1) return EOF;
	return c;
}
