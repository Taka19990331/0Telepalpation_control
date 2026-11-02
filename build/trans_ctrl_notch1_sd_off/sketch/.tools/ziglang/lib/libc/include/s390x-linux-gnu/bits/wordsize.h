#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\s390x-linux-gnu\\bits\\wordsize.h"
/* Determine the wordsize from the preprocessor defines.  */

#if defined __s390x__
# define __WORDSIZE	64
#else
# define __WORDSIZE	32
# define __WORDSIZE32_SIZE_ULONG       1
# define __WORDSIZE32_PTRDIFF_LONG     0
#endif

#define __WORDSIZE_TIME64_COMPAT32     0