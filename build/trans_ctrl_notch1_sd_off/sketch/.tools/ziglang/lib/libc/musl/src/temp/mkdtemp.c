#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\temp\\mkdtemp.c"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

char *mkdtemp(char *template)
{
	size_t l = strlen(template);
	int retries = 100;

	if (l<6 || memcmp(template+l-6, "XXXXXX", 6)) {
		errno = EINVAL;
		return 0;
	}

	do {
		__randname(template+l-6);
		if (!mkdir(template, 0700)) return template;
	} while (--retries && errno == EEXIST);

	memcpy(template+l-6, "XXXXXX", 6);
	return 0;
}
