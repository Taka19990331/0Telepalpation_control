#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\shcore.h"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#ifndef _SHCORE_H_
#define _SHCORE_H_

#include <objidl.h>

#if NTDDI_VERSION >= NTDDI_WIN8

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSOS_DEFAULT = 0,
    BSOS_PREFERDESTINATIONSTREAM
} BSOS_OPTIONS;

STDAPI CreateRandomAccessStreamOnFile(PCWSTR filePath, DWORD accessMode, REFIID riid, void **ppv);
STDAPI CreateRandomAccessStreamOverStream(IStream *stream, BSOS_OPTIONS options, REFIID riid, void **ppv);
STDAPI CreateStreamOverRandomAccessStream(IUnknown *randomAccessStream, REFIID riid, void **ppv);

#ifdef __cplusplus
}
#endif

#endif /* NTDDI_VERSION >= NTDDI_WIN8 */

#endif
