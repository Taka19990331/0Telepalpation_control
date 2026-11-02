#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\lmapibuf.h"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef _LMAPIBUF_
#define _LMAPIBUF_

#ifdef __cplusplus
extern "C" {
#endif

  NET_API_STATUS WINAPI NetApiBufferAllocate(DWORD ByteCount,LPVOID *Buffer);
  NET_API_STATUS WINAPI NetApiBufferFree(LPVOID Buffer);
  NET_API_STATUS WINAPI NetApiBufferReallocate(LPVOID OldBuffer,DWORD NewByteCount,LPVOID *NewBuffer);
  NET_API_STATUS WINAPI NetApiBufferSize(LPVOID Buffer,LPDWORD ByteCount);
  NET_API_STATUS WINAPI NetapipBufferAllocate(DWORD ByteCount,LPVOID *Buffer);

#ifdef __cplusplus
}
#endif
#endif
