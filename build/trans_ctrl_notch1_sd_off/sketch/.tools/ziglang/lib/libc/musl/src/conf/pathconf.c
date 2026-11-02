#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\conf\\pathconf.c"
#include <unistd.h>

long pathconf(const char *path, int name)
{
	return fpathconf(-1, name);
}
