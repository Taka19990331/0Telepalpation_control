#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\libsrc\\ws2tcpip\\in6addr_setany.c"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#undef  __CRT__NO_INLINE
#define __CRT__NO_INLINE
#include <winsock2.h>
#include <ws2tcpip.h>

void IN6ADDR_SETANY(struct sockaddr_in6 *a)
{
	a->sin6_family = AF_INET6;
	a->sin6_port = 0;
	a->sin6_flowinfo = 0;
	a->sin6_addr = (struct in6_addr){0};
	a->sin6_scope_id = 0;
}
