#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\getlogin.c"
#include <unistd.h>
#include <stdlib.h>

char *getlogin(void)
{
	return getenv("LOGNAME");
}
