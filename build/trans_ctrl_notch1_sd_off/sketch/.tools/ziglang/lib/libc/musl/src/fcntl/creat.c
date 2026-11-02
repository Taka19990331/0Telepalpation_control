#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\fcntl\\creat.c"
#include <fcntl.h>

int creat(const char *filename, mode_t mode)
{
	return open(filename, O_CREAT|O_WRONLY|O_TRUNC, mode);
}
