#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\ext2.c"
#include "stdio_impl.h"
#include <stdio_ext.h>

size_t __freadahead(FILE *f)
{
	return f->rend ? f->rend - f->rpos : 0;
}

const char *__freadptr(FILE *f, size_t *sizep)
{
	if (f->rpos == f->rend) return 0;
	*sizep = f->rend - f->rpos;
	return (const char *)f->rpos;
}

void __freadptrinc(FILE *f, size_t inc)
{
	f->rpos += inc;
}

void __fseterr(FILE *f)
{
	f->flags |= F_ERR;
}
