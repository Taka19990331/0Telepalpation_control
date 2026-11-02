#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\musl\\include\\sys\\fsuid.h"
#ifndef _SYS_FSUID_H
#define _SYS_FSUID_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_uid_t
#define __NEED_gid_t

#include <bits/alltypes.h>

int setfsuid(uid_t);
int setfsgid(gid_t);

#ifdef __cplusplus
}
#endif

#endif
