#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\generic-musl\\sys\\un.h"
#ifndef	_SYS_UN_H
#define	_SYS_UN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#define __NEED_sa_family_t
#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
#define __NEED_size_t
#endif

#include <bits/alltypes.h>

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

#if defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
size_t strlen(const char *);
#define SUN_LEN(s) (2+strlen((s)->sun_path))
#endif

#ifdef __cplusplus
}
#endif

#endif