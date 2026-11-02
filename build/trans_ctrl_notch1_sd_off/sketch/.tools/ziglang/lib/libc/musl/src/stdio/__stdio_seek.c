#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\__stdio_seek.c"
#include "stdio_impl.h"
#include <unistd.h>

off_t __stdio_seek(FILE *f, off_t off, int whence)
{
	return __lseek(f->fd, off, whence);
}
