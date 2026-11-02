#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\stdio\\gets.c"
#include "stdio_impl.h"
#include <limits.h>
#include <string.h>

char *gets(char *s)
{
	size_t i=0;
	int c;
	FLOCK(stdin);
	while ((c=getc_unlocked(stdin)) != EOF && c != '\n') s[i++] = c;
	s[i] = 0;
	if (c != '\n' && (!feof(stdin) || !i)) s = 0;
	FUNLOCK(stdin);
	return s;
}
