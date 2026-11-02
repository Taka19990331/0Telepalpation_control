#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\passwd\\fgetpwent.c"
#define _GNU_SOURCE
#include "pwf.h"

struct passwd *fgetpwent(FILE *f)
{
	static char *line;
	static struct passwd pw;
	size_t size=0;
	struct passwd *res;
	__getpwent_a(f, &pw, &line, &size, &res);
	return res;
}
