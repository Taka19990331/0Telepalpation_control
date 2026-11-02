#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\locale\\bind_textdomain_codeset.c"
#include <libintl.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

char *bind_textdomain_codeset(const char *domainname, const char *codeset)
{
	if (codeset && strcasecmp(codeset, "UTF-8"))
		errno = EINVAL;
	return NULL;
}
