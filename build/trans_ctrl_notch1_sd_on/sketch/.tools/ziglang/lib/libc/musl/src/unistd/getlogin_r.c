#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\unistd\\getlogin_r.c"
#include <unistd.h>
#include <string.h>
#include <errno.h>

int getlogin_r(char *name, size_t size)
{
	char *logname = getlogin();
	if (!logname) return ENXIO; /* or...? */
	if (strlen(logname) >= size) return ERANGE;
	strcpy(name, logname);
	return 0;
}
