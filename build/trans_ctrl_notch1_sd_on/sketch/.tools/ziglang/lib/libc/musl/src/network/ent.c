#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\network\\ent.c"
#include <netdb.h>

void sethostent(int x)
{
}

struct hostent *gethostent()
{
	return 0;
}

struct netent *getnetent()
{
	return 0;
}

void endhostent(void)
{
}

weak_alias(sethostent, setnetent);
weak_alias(endhostent, endnetent);
