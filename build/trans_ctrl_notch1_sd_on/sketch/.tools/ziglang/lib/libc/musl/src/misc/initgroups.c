#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\src\\misc\\initgroups.c"
#define _GNU_SOURCE
#include <grp.h>
#include <limits.h>

int initgroups(const char *user, gid_t gid)
{
	gid_t groups[NGROUPS_MAX];
	int count = NGROUPS_MAX;
	if (getgrouplist(user, gid, groups, &count) < 0) return -1;
	return setgroups(count, groups);
}
