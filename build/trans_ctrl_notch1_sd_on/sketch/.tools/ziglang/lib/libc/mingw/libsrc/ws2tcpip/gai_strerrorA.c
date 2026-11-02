#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\libsrc\\ws2tcpip\\gai_strerrorA.c"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#undef  __CRT__NO_INLINE
#define __CRT__NO_INLINE
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>

char *gai_strerrorA(int ecode)
{
	static char buff[GAI_STRERROR_BUFFER_SIZE + 1];
	wcstombs(buff, gai_strerrorW(ecode), GAI_STRERROR_BUFFER_SIZE + 1);
	return buff;
}
