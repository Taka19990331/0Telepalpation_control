#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\libsrc\\ws2tcpip\\in6_is_addr_v4compat.c"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#undef  __CRT__NO_INLINE
#define __CRT__NO_INLINE
#include <winsock2.h>
#include <ws2tcpip.h>

int IN6_IS_ADDR_V4COMPAT(const struct in6_addr *a)
{
	return ((a->s6_words[0] == 0) &&
		(a->s6_words[1] == 0) &&
		(a->s6_words[2] == 0) &&
		(a->s6_words[3] == 0) &&
		(a->s6_words[4] == 0) &&
		(a->s6_words[5] == 0) &&
	      !((a->s6_words[6] == 0) &&
		(a->s6_addr[14] == 0) &&
	       ((a->s6_addr[15] == 0) ||
		(a->s6_addr[15] == 1)) ) );
}
