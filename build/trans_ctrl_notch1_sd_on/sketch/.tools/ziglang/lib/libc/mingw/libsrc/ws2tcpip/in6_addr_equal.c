#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\libsrc\\ws2tcpip\\in6_addr_equal.c"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#undef  __CRT__NO_INLINE
#define __CRT__NO_INLINE
#include <winsock2.h>
#include <ws2tcpip.h>

int IN6_ADDR_EQUAL(const struct in6_addr *a,const struct in6_addr *b)
{
	return (memcmp(a, b, sizeof(struct in6_addr)) == 0);
}
