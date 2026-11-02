#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\passwd\\putpwent.c"
#define _GNU_SOURCE
#include <pwd.h>
#include <stdio.h>

int putpwent(const struct passwd *pw, FILE *f)
{
	return fprintf(f, "%s:%s:%u:%u:%s:%s:%s\n",
		pw->pw_name, pw->pw_passwd, pw->pw_uid, pw->pw_gid,
		pw->pw_gecos, pw->pw_dir, pw->pw_shell)<0 ? -1 : 0;
}
