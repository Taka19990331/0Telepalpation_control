#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\mingw\\libsrc\\wspiapi\\WspiapiFreeAddrInfo.c"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#undef  __CRT__NO_INLINE
#define __CRT__NO_INLINE
#include <winsock2.h>
#include <wspiapi.h>

void WINAPI
WspiapiFreeAddrInfo (struct addrinfo *ai)
{
  static WSPIAPI_PFREEADDRINFO pfFreeAddrInfo = NULL;

  if (!pfFreeAddrInfo)
    pfFreeAddrInfo = (WSPIAPI_PFREEADDRINFO)(void(*)(void)) WspiapiLoad(2);
  (*pfFreeAddrInfo) (ai);
}
