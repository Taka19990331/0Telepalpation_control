#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\glibc\\sysdeps\\unix\\sysv\\linux\\s390\\xstatver.h"
/* Versions of the 'struct stat' data structure used in compatibility xstat
   functions.  */

#include <bits/wordsize.h>

#if __WORDSIZE == 64
# define _STAT_VER_KERNEL	0
# define _STAT_VER_LINUX	1
# define _MKNOD_VER_LINUX	0
#else
# define _STAT_VER_LINUX_OLD	1
# define _STAT_VER_KERNEL	1
# define _STAT_VER_SVR4		2
# define _STAT_VER_LINUX	3
# define _MKNOD_VER_LINUX	1
# define _MKNOD_VER_SVR4	2
#endif
#define _STAT_VER		_STAT_VER_LINUX
#define _MKNOD_VER		_MKNOD_VER_LINUX
